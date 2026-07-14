#include "wasm/jit.h"

#include "function.h"
#include "jit/cfi.h"
#include "jit/codegen.h"
#include "jit/helpers.h"
#include "jit_internal.h"
#include "libcall.h"
#include "buffer.h"

#include "spidir/module.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"

#include "spidir/log.h"
#include "spidir/codegen.h"
#include "spidir/x64.h"
#include "spidir/opt.h"
#include "wasm/error.h"
#include "wasm/host.h"
#include <stdint.h>
#include <cpuid.h>

#include "util/hmap.h"
#include "wasm/wasm.h"

void wasm_module_jit_free(wasm_module_jit_t* jit) {
    if (jit->binary != nullptr) {
        wasm_host_jit_free(jit->binary, jit->rx_page_count, jit->ro_page_count);
        jit->binary = nullptr;
    }
    wasm_host_free(jit->exports);
    wasm_host_free(jit->state_init);
    wasm_host_free(jit->debug.funcs);
    wasm_host_free(jit->debug.relocs);
    jit->exports = nullptr;
    jit->state_init = nullptr;
    jit->debug.funcs = nullptr;
    jit->debug.relocs = nullptr;
    jit->debug.funcs_count = 0;
    jit->debug.relocs_count = 0;
}

static wasm_err_t jit_prepare_table(jit_context_t* ctx, uint32_t id) {
    wasm_err_t err = WASM_NO_ERROR;

    // generate a name for the table
    char name[64];
    name[0] = 't';
    name[1] = 'a';
    name[2] = 'b';
    name[3] = 'l';
    name[4] = 'e';
    name[5] = '_';
    int digits = u64toa(id, &name[6]);
    name[6 + digits] = '\0';

    // create it as a relocation
    // TODO: when we support expanding tables we will need to do something 
    //       about it
    ctx->tables[id].global = spidir_module_create_extern_global(ctx->spidir, name);
    ctx->tables[id].length = ctx->module->tables[id].min;

cleanup:
    return err;
}

static wasm_err_t jit_emit_spidir(jit_context_t* ctx) {
    wasm_err_t err = WASM_NO_ERROR;

    // add all the exported functions into the functions
    // that we want to jit
    for (int i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_t* export = &ctx->module->exports[i];
        if (export->kind == WASM_EXPORT_FUNC) {
            RETHROW(jit_prepare_function(ctx, export->index));
        }
    }

    // prepare the tables
    for (int i = 0; i < ctx->module->tables_count; i++) {
        RETHROW(jit_prepare_table(ctx, i));
    } 

    // Anything referenced by an elem segment is also reachable through
    // call_indirect at runtime so it must be prepared and queued for
    // codegen even when no direct call exists in the module.
    for (int i = 0; i < ctx->module->elems_count; i++) {
        wasm_elem_segment_t* elem = &ctx->module->elems[i];
        for (uint32_t j = 0; j < elem->funcs_count; j++) {
            uint32_t funcidx = elem->funcs[j];
            RETHROW(jit_prepare_function(ctx, funcidx));

            // becase we use a go-indirect we need to actually generate 
            // a cfi thunk that is used for the functype type-checking
            RETHROW(jit_create_cfi_thunk(ctx, funcidx));
        }
    }

    // And the entry-function should also be jitted
    if (ctx->module->start_func >= 0) {
        RETHROW(jit_prepare_function(ctx, ctx->module->start_func));
    }

    // prepare the IR for everything
    while (ctx->queue.length != 0) {
        uint32_t funcidx = vec_pop(&ctx->queue);
        RETHROW(jit_function(ctx, funcidx));
    }

cleanup:
    return err;
}

static wasm_err_t jit_build_state_init(jit_context_t* ctx, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    if (jit->state_size == 0) {
        goto cleanup;
    }

    jit->state_init = wasm_host_calloc(1, jit->state_size);
    CHECK(jit->state_init != nullptr);

    //
    // lay out the global values
    //

    for (int64_t i = 0; i < ctx->module->globals_count; i++) {
        wasm_global_t* global = &ctx->module->globals[i];
        if (!global->mutable) continue;
        jit_global_t* jit_global = &ctx->globals[i];

        void* data = jit->state_init + jit_global->offset;
        switch (global->value.kind) {
            case WASM_VALUE_TYPE_F32: POKE(float, data) = global->value.value.f32; break;
            case WASM_VALUE_TYPE_F64: POKE(double, data) = global->value.value.f64; break;
            case WASM_VALUE_TYPE_I32: POKE(int32_t, data) = global->value.value.i32; break;
            case WASM_VALUE_TYPE_I64: POKE(int64_t, data) = global->value.value.i64; break;
            default: CHECK_FAIL();
        }
    }

    //
    // lay out data entries
    //

    for (int64_t i = 0; i < ctx->module->data_count; i++) {
        wasm_data_t* data = &ctx->module->data[i];
        if (!data->active) {
            jit_data_t* jit_data = &ctx->data[i];

            // TODO: make them be their own allocation maybe? how should memory management 
            //       for these work? do we even really need it to be deallocated on 
            //       data.drop?
            void** data_ptr = jit->state_init + jit_data->offset;
            *data_ptr = ctx->module->data[i].data;
        }
    }

cleanup:
    return err;
}

static wasm_err_t jit_prepare_state(jit_context_t* ctx, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    size_t offset = 0;

    if (ctx->module->globals_count != 0) {
        ctx->globals = CALLOC(jit_global_t, ctx->module->globals_count);
        CHECK(ctx->globals != nullptr);
        for (int64_t i = 0; i < ctx->module->globals_count; i++) {
            wasm_global_t* global = &ctx->module->globals[i];
            spidir_value_type_t type = jit_get_spidir_value_type(global->value.kind);
            ctx->globals[i].type = type;
            if (global->mutable) {
                // mutable, we need space for it
                offset = ALIGN_UP(offset, jit_get_spidir_size(type));
                ctx->globals[i].offset = offset;
                offset += jit_get_spidir_size(type);
            } else {
                // immutable, not going to be allocated, mark it as such
                ctx->globals[i].offset = -1;
            }
        }
    }

    //
    // Layout passive data
    //

    if (ctx->module->data_count != 0) {
        ctx->data = CALLOC(jit_data_t, ctx->module->data_count);
        CHECK(ctx->data != nullptr);
        for (int64_t i = 0; i < ctx->module->data_count; i++) {
            wasm_data_t* data = &ctx->module->data[i];
            if (!data->active) {
                // passive segments get a single void* entry
                offset = ALIGN_UP(offset, sizeof(void*));
                ctx->data[i].offset = offset;
                offset += sizeof(void*);
            } else {
                // active segments don't get an offset
                ctx->data[i].offset = -1;
            }
        }
    }

    jit->state_size = offset;

cleanup:
    return err;
}


wasm_err_t wasm_module_jit(wasm_module_t* module, wasm_module_jit_t* jit, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;
    // use a default config when one is not provided
    if (config == nullptr) {
        static wasm_jit_config_t default_config = {
            .optimize = true
        };
        config = &default_config;
    }

    jit_context_t ctx = {
        .module = module,
        .config = config,
    };

    // it should be cheap enough to allocate it linearly
    ctx.functions = CALLOC(jit_function_t, module->functions_count + module->imports_count);
    CHECK(ctx.functions != nullptr);

    ctx.tables = CALLOC(jit_table_t, module->tables_count);
    CHECK(ctx.tables != nullptr);

    // setup the runtime state buffer (globals + tables)
    RETHROW(jit_prepare_state(&ctx, jit));

    ctx.spidir = spidir_module_create();

    // emit the spidir IR
    RETHROW(jit_emit_spidir(&ctx));

    // optimize the module
    if (config->optimize) {
        spidir_opt_run(ctx.spidir);
    }

    // dump the module if we need to
    if (config->dump_callback != nullptr) {
        spidir_module_dump(ctx.spidir, config->dump_callback, config->dump_arg);
    }

    // actually codegen the entire bloody thing
    RETHROW(jit_codegen(jit, &ctx, config));

    // setup the initial state
    RETHROW(jit_build_state_init(&ctx, jit));

cleanup:
    if (ctx.spidir != nullptr) {
        spidir_module_destroy(ctx.spidir);
    }
    wasm_host_free(ctx.functions);
    wasm_host_free(ctx.globals);
    wasm_host_free(ctx.tables);
    wasm_host_free(ctx.data);
    vec_free(&ctx.queue);

    return err;
}
