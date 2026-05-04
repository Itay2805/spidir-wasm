#pragma once

#include "spidir/module.h"
#include "wasm/error.h"

typedef struct jit_context jit_context_t;

typedef enum jit_helper_kind {
    JIT_HELPER_MEMORY_SIZE,
    JIT_HELPER_MEMORY_GROW,
    JIT_HELPER_MEMORY_COPY,
    JIT_HELPER_MEMORY_FILL,
    JIT_HELPER_TRAP,
    JIT_HELPER_COUNT,
} jit_helper_kind_t;

/**
 * Resolve a helper's host-side address from the spidir extern function
 * id stored in a relocation. Used by the relocation applier in jit.c to
 * patch external_function relocs that target a helper. Returns nullptr
 * if no helper has been registered with this id.
 */
void* jit_helper_lookup_address(jit_context_t* ctx, uint32_t external_id);

/**
 * Lazily creates the spidir extern function for `kind` (so each helper
 * is declared at most once per module) and returns a funcref that can
 * be passed straight to spidir_builder_build_call.
 */
wasm_err_t jit_get_helper(jit_context_t* ctx, jit_helper_kind_t kind, spidir_funcref_t* out);
