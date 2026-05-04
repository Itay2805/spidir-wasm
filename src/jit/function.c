#include "function.h"

#include "inst.h"
#include "opcodes.h"
#include "buffer.h"
#include "spidir/module.h"
#include "util/defs.h"
#include "util/except.h"
#include "wasm/host.h"
#include <stdint.h>

typedef struct jit_build_ctx {
    wasm_err_t err;
    jit_context_t* ctx;
    uint32_t funcidx;
} jit_build_ctx_t;

wasm_err_t jit_prepare_function(jit_context_t* ctx, uint32_t funcidx) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_type_t* args = nullptr;

    size_t imports_count = ctx->module->imports_count;
    CHECK(funcidx < ctx->module->functions_count + imports_count);

    if (ctx->functions[funcidx].inited) {
        goto cleanup;
    }

    typeidx_t typeidx;
    if (funcidx < imports_count) {
        typeidx = ctx->module->imports[funcidx].index;
    } else {
        typeidx = ctx->module->functions[funcidx - imports_count];
    }
    wasm_type_t* type = &ctx->module->types[typeidx];

    // the ret type
    spidir_value_type_t ret_type = SPIDIR_TYPE_NONE;
    if (type->result_types_count == 1) {
        ret_type = jit_get_spidir_value_type(type->result_types[0]);
    } else {
        CHECK(type->result_types_count == 0);
    }

    // the args, with two hidden parameters: the memory base and the
    // runtime state base (currently just the globals region).
    size_t args_count = type->arg_types_count + 2;
    args = CALLOC(spidir_value_type_t, args_count);
    int ai = 0;

    args[ai++] = SPIDIR_TYPE_PTR; // the memory base
    args[ai++] = SPIDIR_TYPE_PTR; // the state base (globals)

    for (int64_t i = 0; i < type->arg_types_count; i++) {
        args[ai++] = jit_get_spidir_value_type(type->arg_types[i]);
    }

    char name[64];
    wasm_host_snprintf(name, sizeof(name), "func%d", funcidx);

    if (funcidx < ctx->module->imports_count) {
        // for imports we create an extern reference
        spidir_extern_function_t func = spidir_module_create_extern_function(
            ctx->spidir,
            name,
            ret_type,
            args_count, args
        );
        ctx->functions[funcidx].spidir = spidir_funcref_make_external(func);
    } else {
        // for normal function create as internal function
        spidir_function_t func = spidir_module_create_function(
            ctx->spidir,
            name,
            ret_type,
            args_count, args
        );
        ctx->functions[funcidx].spidir = spidir_funcref_make_internal(func);

        // only queue internal functions for jitting
        vec_push(&ctx->queue, funcidx);
    }

    ctx->functions[funcidx].inited = true;

cleanup:
    wasm_host_free(args);

    return err;
}

static void jit_build_function(spidir_builder_handle_t builder, void* _ctx) {
    wasm_err_t err = WASM_NO_ERROR;

    // the general context
    jit_build_ctx_t* build = _ctx;
    uint32_t funcidx = build->funcidx;
    jit_context_t* ctx = build->ctx;
    jit_function_ctx_t func = {};

    JIT_TRACE("wasm: func%d", funcidx);

    // this must be an internal function, verify as such
    uint32_t imports_count = ctx->module->imports_count;
    CHECK(funcidx >= imports_count);
    funcidx -= imports_count;
    typeidx_t typeidx = ctx->module->functions[funcidx];
    wasm_type_t* type = &ctx->module->types[typeidx];

    // the code buffer
    buffer_t code = {
        .data = ctx->module->code[funcidx].code,
        .len = ctx->module->code[funcidx].length
    };

    // the main block
    jit_label_t label = {};

    // setup params
    wasm_value_type_t* arg_types = type->arg_types;

    size_t args_count = type->arg_types_count;
    jit_value_t* args = vec_add(&func.locals, args_count);
    for (int i = 0; i < args_count; i++) {
        args[i].type = jit_get_spidir_value_type(arg_types[i]);
        // hidden params 0..1 are mem/state; wasm args follow
        args[i].value = spidir_builder_build_param_ref(builder, i + 2);
    }

    // setup ret type
    CHECK(type->result_types_count <= 1);
    if (type->result_types_count == 1) {
        func.ret_type = jit_get_spidir_value_type(type->result_types[0]);
    } else {
        func.ret_type = SPIDIR_TYPE_NONE;
    }

    // setup locals
    uint32_t locals_count = BUFFER_PULL_U32(&code);
    for (int64_t i = 0; i < locals_count; i++) {
        uint32_t count = BUFFER_PULL_U32(&code);

        wasm_value_type_t type = 0;
        RETHROW(buffer_pull_val_type(&code, &type));

        jit_value_t* locals = vec_add(&func.locals, count);
        for (int j = 0; j < count; j++) {
            locals[j].type = jit_get_spidir_value_type(type);
            locals[j].value = spidir_builder_build_iconst(builder, locals[j].type, 0);
        }
    }

    // setup the entry block
    spidir_block_t block = spidir_builder_create_block(builder);
    spidir_builder_set_entry_block(builder, block);
    spidir_builder_set_block(builder, block);
    vec_push(&func.labels, label);

    // jit everything
    while (code.len != 0) {
        // ensure we have a label currently
        CHECK(func.labels.length > 0);

        // get the next opcode
        uint8_t byte = BUFFER_PULL(uint8_t, &code);
        jit_instruction_t callback = g_wasm_inst_jit_callbacks[byte];
        CHECK(callback != nullptr, "Unknown wasm opcode %02x", byte);

        // emit it
        RETHROW(callback(builder, &code, ctx, &func, &vec_last(&func.labels)));
    }

    // ensure we have no more labels left
    CHECK(func.labels.length == 0);

cleanup:
    // freeup everything
    while (func.labels.length != 0) {
        jit_label_t label = vec_pop(&func.labels);
        jit_free_label(&label);
    }
    vec_free(&func.labels);
    vec_free(&func.locals);

    build->err = err;
}

wasm_err_t jit_function(jit_context_t* ctx, uint32_t funcidx) {
    wasm_err_t err = WASM_NO_ERROR;

    jit_function_t* function = &ctx->functions[funcidx];
    CHECK(function->inited);

    jit_build_ctx_t context = {
        .err = WASM_NO_ERROR,
        .funcidx = funcidx,
        .ctx = ctx
    };
    CHECK(spidir_funcref_is_internal(function->spidir));
    spidir_module_build_function(
        ctx->spidir,
        spidir_funcref_get_internal(function->spidir),
        jit_build_function,
        &context
    );
    RETHROW(context.err);

cleanup:
    return err;
}
