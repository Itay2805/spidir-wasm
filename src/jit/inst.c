#include "inst.h"

#include "function.h"
#include "opcodes.h"
#include "spidir/module.h"
#include "util/vec.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"
#include "wasm/host.h"

#define JIT_PUSH(_type, _value) \
    do { \
        jit_value_t value__ = { \
            .type = _type, \
            .value = _value, \
        }; \
        vec_push(&label->stack, value__); \
    } while (0)

#define JIT_POP(_type) \
    ({ \
        jit_value_t value__ = vec_pop(&label->stack); \
        spidir_value_type_t type__ = _type; \
        CHECK(value__.type == type__, "Unexpected type (%d != %d)", value__.type, type__); \
        value__.value; \
    })

void jit_free_label(jit_label_t* label) {
    vec_free(&label->stack);
}

//----------------------------------------------------------------------------------------------------------------------
// Parametric Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_unreachable(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tunreachable");
    spidir_builder_build_unreachable(builder);
    label->terminated = true;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_nop(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tnop");

cleanup:
    return err;
}

static wasm_err_t jit_wasm_drop(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tdrop");
    CHECK(label->stack.length >= 1);
    vec_pop(&label->stack);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_select(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tselect");

    spidir_value_t c = JIT_POP(SPIDIR_TYPE_I32);
    jit_value_t val2 = vec_pop(&label->stack);
    jit_value_t val1 = vec_pop(&label->stack);
    CHECK(val1.type == val2.type);

    // prepare the next block
    spidir_block_t next_block = spidir_builder_create_block(builder);

    // we are going to use a brcond, if its zero it will take val1 and if its
    // non-zero it will take val2
    spidir_value_t values[] = { val1.value, val2.value };
    spidir_builder_build_brcond(builder, c, next_block, next_block);

    // setup the continuation
    spidir_builder_set_block(builder, next_block);
    spidir_value_t value = spidir_builder_build_phi(builder, val1.type, 2, values, NULL);

    // and push it
    JIT_PUSH(val1.type, value);

cleanup:
    return err;
}


static wasm_type_t* wasm_get_func_type(jit_context_t* ctx, uint32_t funcidx) {
    size_t imports_count = ctx->module->imports_count;
    typeidx_t typeidx;
    if (funcidx < imports_count) {
        typeidx = ctx->module->imports[funcidx].index;
    } else {
        typeidx = ctx->module->functions[funcidx - imports_count];
    }
    wasm_type_t* type = &ctx->module->types[typeidx];
    return type;
}

//----------------------------------------------------------------------------------------------------------------------
// Control Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_return(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \treturn");

    // handle return value if any
    spidir_value_t value = SPIDIR_VALUE_INVALID;
    if (func->ret_type != SPIDIR_TYPE_NONE) {
        value = JIT_POP(func->ret_type);
    }
    CHECK(label->stack.length == 0);
    spidir_builder_build_return(builder, value);

    // we terminated the label
    label->terminated = true;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_call(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_t* params = nullptr;

    // prepare the function for jitting
    uint32_t funcidx = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tcall %d", funcidx);

    RETHROW(jit_prepare_function(ctx, funcidx));
    jit_function_t* callee = &ctx->functions[funcidx];

    // get the function signature
    wasm_type_t* type = wasm_get_func_type(ctx, funcidx);
    CHECK(type != nullptr);

    // first param is the hidden mem base, pass it through unchanged
    // from our own frame so the callee sees the same module-instance state.
    size_t params_count = type->arg_types_count + 1;
    params = CALLOC(spidir_value_t, params_count);
    CHECK(params != nullptr);

    params[0] = spidir_builder_build_param_ref(builder, 0);

    // pop the rest of the args from the stack
    for (int i = 0; i < type->arg_types_count; i++) {
        size_t arg_index = type->arg_types_count - i - 1;
        spidir_value_type_t stype = jit_get_spidir_value_type(type->arg_types[arg_index]);
        spidir_value_t value = JIT_POP(stype);
        params[arg_index + 1] = value;
    }

    // perform the call
    spidir_value_t ret_val = spidir_builder_build_call(builder, callee->spidir, params_count, params);

    // if we have a return push it into the stack
    if (type->result_types_count > 0) {
        CHECK(type->result_types_count == 1);

        spidir_value_type_t stype = jit_get_spidir_value_type(type->result_types[0]);
        JIT_PUSH(stype, ret_val);
    }

cleanup:
    wasm_host_free(params);

    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Variable Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_local_get(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.get %d", index);
    CHECK(index < func->locals.length);
    spidir_value_t value = func->locals.elements[index].value;
    CHECK(value.id != SPIDIR_VALUE_INVALID.id);
    JIT_PUSH(func->locals.elements[index].type, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_local_set(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.set %d", index);
    CHECK(index < func->locals.length);
    func->locals.elements[index].value = JIT_POP(func->locals.elements[index].type);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_local_tee(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.tee %d", index);
    CHECK(index < func->locals.length);
    spidir_value_type_t value_type = func->locals.elements[index].type;
    spidir_value_t value = JIT_POP(value_type);
    func->locals.elements[index].value = value;
    JIT_PUSH(value_type, value);

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Numeric Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_i32_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    int32_t value = BUFFER_PULL_I32(code);
    JIT_TRACE("wasm: \ti32.const %d", value);
    JIT_PUSH(SPIDIR_TYPE_I32, spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, (uint32_t)value));

cleanup:
    return err;
}

static wasm_err_t jit_wasm_i64_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    int64_t value = BUFFER_PULL_I64(code);
    JIT_TRACE("wasm: \ti64.const %lld", (long long)value);
    JIT_PUSH(SPIDIR_TYPE_I64, spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, value));

cleanup:
    return err;
}

#define SWAP(a, b) \
    do { \
        typeof(a) temp__ = a; \
        a = b; \
        b = temp__; \
    } while (0)

static wasm_err_t jit_wasm_cmpi(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    // figure the exact type
    spidir_value_type_t type;
    switch (opcode) {
        case 0x45 ... 0x4F: type = SPIDIR_TYPE_I32; opcode -= 0x45; break;
        case 0x50 ... 0x5A: type = SPIDIR_TYPE_I64; opcode -= 0x50; break;
        default: CHECK_FAIL();
    }

    // get the args
    spidir_value_t arg2, arg1;
    if (opcode == 0) {
        // eqz
        arg2 = JIT_POP(type);
        arg1 = spidir_builder_build_iconst(builder, type, 0);
    } else {
        arg2 = JIT_POP(type);
        arg1 = JIT_POP(type);
    }

    // choose the compare, got gt kinds we just swap
    spidir_icmp_kind_t kind;
    switch (opcode) {
        case 0:
        case 1: kind = SPIDIR_ICMP_EQ; break;
        case 2: kind = SPIDIR_ICMP_NE; break;
        case 3: kind = SPIDIR_ICMP_SLT; break;
        case 4: kind = SPIDIR_ICMP_ULT; break;
        case 5: kind = SPIDIR_ICMP_SLT; SWAP(arg1, arg2); break;
        case 6: kind = SPIDIR_ICMP_ULT; SWAP(arg1, arg2); break;
        case 7: kind = SPIDIR_ICMP_SLE; break;
        case 8: kind = SPIDIR_ICMP_ULE; break;
        case 9: kind = SPIDIR_ICMP_SLE; SWAP(arg1, arg2); break;
        case 10: kind = SPIDIR_ICMP_ULE; SWAP(arg1, arg2); break;
        default: CHECK_FAIL();
    }

    // build the icmp, it always outputs i32
    spidir_value_t value = spidir_builder_build_icmp(builder,
        kind, SPIDIR_TYPE_I32,
        arg1, arg2
    );

    // push the result
    JIT_PUSH(SPIDIR_TYPE_I32, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_shift(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    spidir_value_type_t type;
    uint64_t mask;
    switch (opcode) {
        case 0x74 ... 0x76: type = SPIDIR_TYPE_I32; mask = 31; opcode -= 0x74; break;
        case 0x86 ... 0x88: type = SPIDIR_TYPE_I64; mask = 63; opcode -= 0x86; break;
        default: CHECK_FAIL();
    }

    // top of stack is the shift amount, below is the value
    spidir_value_t shift_amt = JIT_POP(type);
    spidir_value_t value = JIT_POP(type);

    // wasm reduces the shift count modulo the bit width - mask explicitly
    // so the result is well-defined no matter how the backend handles
    // out-of-range counts.
    shift_amt = spidir_builder_build_and(builder, shift_amt,
        spidir_builder_build_iconst(builder, type, mask));

    spidir_value_t result;
    switch (opcode) {
        case 0: result = spidir_builder_build_shl(builder, value, shift_amt); break;
        case 1: result = spidir_builder_build_ashr(builder, value, shift_amt); break;
        case 2: result = spidir_builder_build_lshr(builder, value, shift_amt); break;
        default: CHECK_FAIL();
    }

    JIT_PUSH(type, result);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_binopi(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    // figure the exact type
    spidir_value_type_t type;
    switch (opcode) {
        case 0x6A ... 0x73: type = SPIDIR_TYPE_I32; opcode -= 0x6A; break;
        case 0x7C ... 0x85: type = SPIDIR_TYPE_I64; opcode -= 0x7C; break;
        default: CHECK_FAIL();
    }

    // get the two values
    spidir_value_t arg2 = JIT_POP(type);
    spidir_value_t arg1 = JIT_POP(type);

    // and now perform the action
    spidir_value_t value;
    switch (opcode) {
        case 0: value = spidir_builder_build_iadd(builder, arg1, arg2); break;
        case 1: value = spidir_builder_build_isub(builder, arg1, arg2); break;
        case 2: value = spidir_builder_build_imul(builder, arg1, arg2); break;
        case 3: value = spidir_builder_build_sdiv(builder, arg1, arg2); break;
        case 4: value = spidir_builder_build_udiv(builder, arg1, arg2); break;
        case 5: value = spidir_builder_build_srem(builder, arg1, arg2); break;
        case 6: value = spidir_builder_build_urem(builder, arg1, arg2); break;
        case 7: value = spidir_builder_build_and(builder, arg1, arg2); break;
        case 8: value = spidir_builder_build_or(builder, arg1, arg2); break;
        case 9: value = spidir_builder_build_xor(builder, arg1, arg2); break;
        default: CHECK_FAIL();
    }

    // and push it back
    JIT_PUSH(type, value);

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Instruction lookup table
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_end(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tend");

    // only top-level end is supported at this point — nested blocks
    // arrive with the block/loop/br opcodes in later commits.
    CHECK(func->labels.length == 1);

    if (!label->terminated) {
        // this is a fallthrough from the entire function,
        // handle it like a return
        spidir_value_t value = SPIDIR_VALUE_INVALID;
        if (func->ret_type != SPIDIR_TYPE_NONE) {
            value = JIT_POP(func->ret_type);
        }
        spidir_builder_build_return(builder, value);
    }

    // stack must be empty at this point
    CHECK(label->stack.length == 0);

    // remove the block
    label = nullptr;
    jit_label_t top_label = vec_pop(&func->labels);
    jit_free_label(&top_label);

cleanup:
    return err;
}

const jit_instruction_t g_wasm_inst_jit_callbacks[0x100] = {
    [0x0B] = jit_wasm_end,

    // Parametric Instructions
    [0x00] = jit_wasm_unreachable,
    [0x01] = jit_wasm_nop,
    [0x1A] = jit_wasm_drop,
    [0x1B] = jit_wasm_select,

    // Control Instructions
    [0x0F] = jit_wasm_return,
    [0x10] = jit_wasm_call,

    // Variable Instructions
    [0x20] = jit_wasm_local_get,
    [0x21] = jit_wasm_local_set,
    [0x22] = jit_wasm_local_tee,

    // Numeric Instructions
    [0x41] = jit_wasm_i32_const,
    [0x42] = jit_wasm_i64_const,
    [0x45 ... 0x5A] = jit_wasm_cmpi,
    [0x6A ... 0x73] = jit_wasm_binopi,
    [0x74 ... 0x76] = jit_wasm_shift,
    [0x7C ... 0x85] = jit_wasm_binopi,
    [0x86 ... 0x88] = jit_wasm_shift,
};
