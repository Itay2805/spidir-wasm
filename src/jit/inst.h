#pragma once

#include <spidir/module.h>
#include <stdint.h>

#include "jit_internal.h"
#include "buffer.h"
#include "util/vec.h"

typedef struct jit_value {
    spidir_value_t value;
    spidir_value_type_t type;
} jit_value_t;

typedef vec(jit_value_t) jit_values_t;

typedef struct jit_label {
    // the block of this label
    spidir_block_t block;

    // the current stack of the label
    jit_values_t stack;

    // did we terminate the block yet
    bool terminated;
} jit_label_t;

typedef vec(jit_label_t) jit_labels_t;

typedef struct jit_function_ctx {
    // the types of all the locals
    jit_values_t locals;

    // handle returning values
    spidir_value_type_t ret_type;

    // the labels stack
    jit_labels_t labels;
} jit_function_ctx_t;

typedef wasm_err_t (*jit_instruction_t)(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* inst, jit_label_t* label);

void jit_free_label(jit_label_t* label);

extern const jit_instruction_t g_wasm_inst_jit_callbacks[0x100];
