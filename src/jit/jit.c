#include "wasm/jit.h"

#include "function.h"
#include "jit_internal.h"
#include "buffer.h"

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

#include "util/hmap.h"

static spidir_codegen_machine_handle_t g_spidir_machine = nullptr;

typedef union code_map_entry code_map_entry_t;

static void spidir_log_callback(spidir_log_level_t level, const char* module, size_t module_len, const char* message, size_t message_len) {
    switch (level) {
        default:
        case SPIDIR_LOG_LEVEL_TRACE: DEBUG("%.*s: %.*s", (int)module_len, module, (int)message_len, message); break;
        case SPIDIR_LOG_LEVEL_DEBUG: DEBUG("%.*s: %.*s", (int)module_len, module, (int)message_len, message); break;
        case SPIDIR_LOG_LEVEL_INFO: TRACE("%.*s: %.*s", (int)module_len, module, (int)message_len, message); break;
        case SPIDIR_LOG_LEVEL_WARN: WARN("%.*s: %.*s", (int)module_len, module, (int)message_len, message); break;
        case SPIDIR_LOG_LEVEL_ERROR: ERROR("%.*s: %.*s", (int)module_len, module, (int)message_len, message); break;
    }
}

static void wasm_jit_init(void) {
    spidir_log_init(spidir_log_callback);
    spidir_log_set_max_level(SPIDIR_LOG_LEVEL_DEBUG);

    spidir_x64_machine_config_t machine_config = {
        .extern_code_model = SPIDIR_X64_CM_LARGE_ABS,
        .internal_code_model = SPIDIR_X64_CM_SMALL_PIC,
    };
    g_spidir_machine = spidir_codegen_create_x64_machine_with_config(&machine_config);
}

void wasm_module_jit_free(wasm_module_jit_t* jit) {
    if (jit->binary != nullptr) {
        wasm_host_jit_free(jit->binary, jit->rx_page_count, jit->ro_page_count);
        jit->binary = nullptr;
    }
    wasm_host_free(jit->exports);
    jit->exports = nullptr;
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

    // prepare the IR for everything
    while (ctx->queue.length != 0) {
        uint32_t funcidx = vec_pop(&ctx->queue);
        RETHROW(jit_function(ctx, funcidx));
    }

cleanup:
    return err;
}

static wasm_err_t jit_apply_reloc(uint8_t* code, size_t code_size, const spidir_codegen_reloc_t* reloc, void* target) {
    wasm_err_t err = WASM_NO_ERROR;

    uint64_t F = (uint64_t)target;
    uint64_t P = (uint64_t)code + reloc->offset;
    int64_t A = reloc->addend;

    switch (reloc->kind) {
        case SPIDIR_RELOC_X64_PC32: {
            CHECK(reloc->offset + 4 <= code_size);
            ptrdiff_t value = F + A - P;

            // ensure within signed 32bit range
            CHECK(INT32_MIN <= value);
            CHECK(value <= INT32_MAX);

            // place it
            POKE(uint32_t, P) = value;
        } break;

        case SPIDIR_RELOC_X64_ABS64: {
            CHECK(reloc->offset + 8 <= code_size);
            POKE(uint64_t, P) = F + A;
        } break;

        default:
            CHECK_FAIL();
    }

cleanup:
    return err;
}

static void* jit_get_indirect(void* func) {
    func -= 4;

    // place an endbr64
    // TODO: ignore when IBT is not supported
    uint8_t* opcode = func;
    opcode[0] = 0xF3;
    opcode[1] = 0x0F;
    opcode[2] = 0x1E;
    opcode[3] = 0xFA;

    return func;
}

typedef union code_map_entry {
    struct {
        uint32_t code_offset;
        uint32_t rodata_offset;
    };
    uint64_t value;
} code_map_entry_t;

static wasm_err_t jit_emit_code(jit_context_t* ctx, wasm_module_jit_t* jit, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;

    // TODO: non-lazy init or something
    if (g_spidir_machine == nullptr) {
        wasm_jit_init();
    }

    vec(spidir_codegen_blob_handle_t) blobs = {};
    vec(spidir_function_t) functions = {};

    // maps
    hmap_t code_map = HMAP_INIT;

    spidir_codegen_config_t codgen_config = {
        .verify_ir = true,
        .verify_regalloc = true
    };

    // TODO: generate functions lazily based on relocations
    //       so once spidir can inline we only generate whatever
    //       that is needed (we already only generate functions that
    //       are called at the wasm level)

    size_t code_size = 0;
    size_t rodata_size = 0;

    // now go over and jit everything
    for (int64_t i = 0; i < ctx->module->functions_count; i++) {
        if (!ctx->functions[i].inited) {
            continue;
        }

        spidir_funcref_t funcref = ctx->functions[i].spidir;
        if (!spidir_funcref_is_internal(funcref)) {
            continue;
        }

        spidir_function_t func = spidir_funcref_get_internal(funcref);

        // actually emit the function
        spidir_codegen_blob_handle_t blob;
        spidir_codegen_status_t status = spidir_codegen_emit_function(
            g_spidir_machine,
            &codgen_config,
            ctx->spidir, func,
            &blob
        );
        CHECK(status == SPIDIR_CODEGEN_OK, "%d", status);

        // push the blob and the function next to
        // it for easier lookups later
        vec_push(&blobs, blob);
        vec_push(&functions, func);

        //
        // constpool
        //

        rodata_size += ALIGN_UP(rodata_size, spidir_codegen_blob_get_constpool_align(blob));
        size_t rodata_offset = rodata_size;
        rodata_size += spidir_codegen_blob_get_constpool_size(blob);

        //
        // code
        //

        // we align the code and pad it with 16 bytes, this
        // place will also be used for indirect jumps
        code_size = ALIGN_UP(code_size, 16);
        code_size += 16;
        size_t code_offset = code_size;
        code_size += spidir_codegen_blob_get_code_size(blob);

        code_map_entry_t entry = {
            .code_offset = code_offset,
            .rodata_offset = rodata_offset,
        };
        hmap_insert(&code_map, func.id, entry.value);
    }

    // align everything to page size
    size_t page_size = wasm_host_page_size();
    code_size = ALIGN_UP(code_size, page_size);
    rodata_size = ALIGN_UP(rodata_size, page_size);

    // now that we know the sizes allocate the full range
    jit->rx_page_count = code_size / page_size;
    jit->ro_page_count = rodata_size / page_size;
    jit->binary = wasm_host_jit_alloc(jit->rx_page_count, jit->ro_page_count);
    CHECK(jit->binary != nullptr);

    void* jit_code = jit->binary;
    void* jit_rodata = jit->binary + code_size;

    // initialize the jit as required
    if (code_size != 0) memset(jit_code, 0xCC, code_size);
    if (rodata_size != 0) memset(jit_rodata, 0x00, rodata_size);

    // now go over all the blobs
    // and apply all the relocs
    for (int i = 0; i < functions.length; i++) {
        spidir_function_t func = functions.elements[i];
        spidir_codegen_blob_handle_t blob = blobs.elements[i];

        // get the offsets
        code_map_entry_t entry = {};
        CHECK(hmap_lookup(&code_map, func.id, &entry.value));
        size_t func_code_offset = entry.code_offset;
        size_t const_pool_offset = entry.rodata_offset;

        //
        // copy the blobs
        //

        if (spidir_codegen_blob_get_code_size(blob) != 0) {
            memcpy(
                jit_code + func_code_offset,
                spidir_codegen_blob_get_code(blob),
                spidir_codegen_blob_get_code_size(blob)
            );
        }

        if (spidir_codegen_blob_get_constpool_size(blob) != 0) {
            memcpy(
                jit_rodata + const_pool_offset,
                spidir_codegen_blob_get_constpool(blob),
                spidir_codegen_blob_get_constpool_size(blob)
            );
        }

        // go over the relocations of the function
        const spidir_codegen_reloc_t* relocs = spidir_codegen_blob_get_relocs(blob);
        for (int j = 0; j < spidir_codegen_blob_get_reloc_count(blob); j++) {
            const spidir_codegen_reloc_t* reloc = &relocs[j];

            // resolve the target
            void* target = nullptr;
            if (reloc->target_kind == SPIDIR_RELOC_TARGET_CONSTPOOL) {
                target = jit_rodata + const_pool_offset;

            } else if (reloc->target_kind == SPIDIR_RELOC_TARGET_INTERNAL_FUNCTION) {
                // get the callee entry
                code_map_entry_t callee_entry = {};
                CHECK(hmap_lookup(&code_map, reloc->target.internal.id, &callee_entry.value));
                target = jit_code + callee_entry.code_offset;

                // we currently expect local functions to only ever have direct accesses
                CHECK(reloc->target_kind == SPIDIR_RELOC_X64_PC32);

            } else {
                // External-function relocations only become resolvable
                // once we have the runtime helper registry — until then
                // any module that needs one is rejected here.
                CHECK_FAIL();
            }

            // actually apply the reloc
            RETHROW(jit_apply_reloc(
                jit_code + func_code_offset,
                spidir_codegen_blob_get_code_size(blob),
                reloc,
                target
            ));
        }
    }

    // fill the export table, this will also emit the endbr64
    jit->exports = CALLOC(wasm_jit_export_t, ctx->module->exports_count);
    CHECK(jit->exports != nullptr);

    for (int i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_type_t kind = ctx->module->exports[i].kind;
        uint32_t index = ctx->module->exports[i].index;

        if (kind == WASM_EXPORT_FUNC) {
            CHECK(ctx->functions[index].inited);
            spidir_funcref_t funcref = ctx->functions[index].spidir;

            // TODO: I think its possible to export an import (?)
            CHECK(spidir_funcref_is_internal(funcref));

            // get the entry
            code_map_entry_t entry = {};
            CHECK(hmap_lookup(&code_map, spidir_funcref_get_internal(funcref).id, &entry.value));

            // add the exported function as an indirect target
            void* target = jit_get_indirect(jit_code + entry.code_offset);
            jit->exports[i].func.address = target;

        } else if (kind == WASM_EXPORT_GLOBAL) {
            // get the global's offset
            jit->exports[i].global.offset = ctx->globals[index].offset;

        } else if (kind == WASM_EXPORT_MEMORY) {
            // nothing to do...

        } else {
            CHECK_FAIL();
        }
    }

    // we are finished with jitting, we can lock the region and
    // let everything use it
    CHECK(wasm_host_jit_lock(jit->binary, jit->rx_page_count, jit->ro_page_count));

cleanup:
    // free the global stuff
    if (IS_ERROR(err)) {
        wasm_module_jit_free(jit);
    }

    // free the local stuff
    for (int i = 0; i < blobs.length; i++) {
        spidir_codegen_blob_destroy(blobs.elements[i]);
    }
    vec_free(&blobs);
    vec_free(&functions);
    hmap_free(&code_map);

    return err;
}

// Lays out the runtime state buffer: mutable globals get a slot, immutables
// stay constants in the IR. The combined size is exposed as
// wasm_module_jit_t::state_size.
static wasm_err_t jit_prepare_state(jit_context_t* ctx, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    ctx->globals = CALLOC(jit_global_t, ctx->module->globals_count);
    CHECK(ctx->module->globals_count == 0 || ctx->globals != nullptr);

    size_t offset = 0;
    for (int i = 0; i < ctx->module->globals_count; i++) {
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

    jit->state_size = offset;

cleanup:
    return err;
}

wasm_err_t wasm_module_jit(wasm_module_t* module, wasm_module_jit_t* jit, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;
    jit_context_t ctx = {
        .module = module
    };

    // use a default config when one is not provided
    if (config == nullptr) {
        static wasm_jit_config_t default_config = {
            .optimize = true
        };
        config = &default_config;
    }

    // it should be cheap enough to allocate it linearly
    ctx.functions = CALLOC(jit_function_t, module->functions_count + module->imports_count);
    CHECK(ctx.functions != nullptr);

    // setup the runtime state buffer (globals)
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

    // generate the entire thing
    RETHROW(jit_emit_code(&ctx, jit, config));

cleanup:
    if (ctx.spidir != nullptr) {
        spidir_module_destroy(ctx.spidir);
    }
    wasm_host_free(ctx.functions);
    wasm_host_free(ctx.globals);
    vec_free(&ctx.queue);

    return err;
}
