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

    // Control Instructions
    [0x0F] = jit_wasm_return,

    // Numeric Instructions
    [0x41] = jit_wasm_i32_const,
};
