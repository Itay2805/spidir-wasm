#include "cfi.h"

#include "jit/helpers.h"
#include "jit/inst.h"
#include "jit_internal.h"
#include "spidir/module.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"
#include "wasm/error.h"
#include "wasm/host.h"
#include "wasm/wasm.h"

typedef struct jit_cfi_ctx {
    jit_context_t* ctx;
    wasm_type_t* type;
    spidir_funcref_t ref;
    wasm_err_t err;
} jit_cfi_ctx_t;

static void jit_build_cfi_thunk(spidir_builder_handle_t builder, void* _ctx) {
    wasm_err_t err = WASM_NO_ERROR;
    jit_cfi_ctx_t* build = _ctx;
    spidir_value_t* params = nullptr;
    jit_context_t* ctx = build->ctx;

    // get the type and its id, we assume the type always comes
    // from the types array
    wasm_type_t* type = build->type;
    uint64_t type_id = jit_cfi_get_type_id(ctx, build->type);

    // all the blocks we need
    spidir_block_t entry = spidir_builder_create_block(builder);
    spidir_block_t cfi_failed = spidir_builder_create_block(builder);
    spidir_block_t cfi_success = spidir_builder_create_block(builder);
    spidir_builder_set_entry_block(builder, entry);

    // the entry block checks the type is correct
    spidir_builder_set_block(builder, entry);
    spidir_value_t expected_type_id = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, type_id);
    spidir_value_t got_type_id = spidir_builder_build_param_ref(builder, 2);
    spidir_builder_build_brcond(builder, 
        spidir_builder_build_icmp(
            builder, 
            SPIDIR_ICMP_EQ, SPIDIR_TYPE_I32, 
            expected_type_id, got_type_id
        ),
        cfi_success, cfi_failed
    );

    // the failure path, just trap
    spidir_builder_set_block(builder, cfi_failed);
    RETHROW(jit_emit_trap(builder, ctx));

    // the success path, call the real function directly
    spidir_builder_set_block(builder, cfi_success);

    // setup the args, we passthrough most of it
    size_t arg_count = type->arg_types_count + 2;
    params = CALLOC(spidir_value_t, arg_count);
    CHECK(params != nullptr);

    // the first two params are the mem base + state base
    params[0] = spidir_builder_build_param_ref(builder, 0);
    params[1] = spidir_builder_build_param_ref(builder, 1);

    // the rest come after the type id
    for (int i = 0; i < type->arg_types_count; i++) {
        params[2 + i] = spidir_builder_build_param_ref(builder, 3 + i);
    }

    // and now call it and return it
    spidir_value_t res = spidir_builder_build_call(builder, build->ref, arg_count, params);
    spidir_builder_build_return(builder, res);

cleanup:
    wasm_host_free(params);

    build->err = err;
}

wasm_err_t jit_create_cfi_thunk(jit_context_t* ctx, uint32_t funcidx) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_type_t* args = nullptr;

    wasm_type_t* type = wasm_get_func(ctx->module, funcidx);
    CHECK(type != nullptr);
    
    // the ret type
    spidir_value_type_t ret_type = SPIDIR_TYPE_NONE;
    if (type->result_types_count == 1) {
        ret_type = jit_get_spidir_value_type(type->result_types[0]);
    } else {
        CHECK(type->result_types_count == 0);
    }

    // the args
    size_t args_count = type->arg_types_count + 3;
    args = CALLOC(spidir_value_type_t, args_count);
    int ai = 0;
    
    args[ai++] = SPIDIR_TYPE_PTR; // the memory base
    args[ai++] = SPIDIR_TYPE_PTR; // the state base
    args[ai++] = SPIDIR_TYPE_I64; // the type id

    for (int64_t i = 0; i < type->arg_types_count; i++) {
        args[ai++] = jit_get_spidir_value_type(type->arg_types[i]);
    }

    // TODO: generate a nice debug name
    char name[64];
    name[0] = 'c';
    name[1] = 'f';
    name[2] = 'i';
    name[3] = '.';
    int digits = u64toa(funcidx, &name[4]);
    name[4 + digits] = '\0';

    // create the spidir function for it
    jit_function_t* func = &ctx->functions[funcidx];
    func->has_cfi = true;
    func->cfi_thunk = spidir_module_create_function(
        ctx->spidir,
        name,
        ret_type,
        args_count, args
    );

    // and build it right away because we can
    jit_cfi_ctx_t cfi_ctx = {
        .ref = func->spidir,
        .type = type,
        .ctx = ctx,
        .err = WASM_NO_ERROR,
    };
    spidir_module_build_function(
        ctx->spidir,
        ctx->functions[funcidx].cfi_thunk,
        jit_build_cfi_thunk,
        &cfi_ctx
    );
    RETHROW(cfi_ctx.err);

cleanup:
    wasm_host_free(args);

    return err;
}

uint64_t jit_cfi_get_type_id(jit_context_t* ctx, wasm_type_t* type) {
    // NOTE: we assume the type is part of the types array and is of the current module
    //       so we can derive the type id back really easily
    uint64_t idx = type - ctx->module->types;
    
    // just use the index as a type index for now
    return idx;
}
