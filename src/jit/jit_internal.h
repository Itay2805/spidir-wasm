#pragma once

#include "wasm/wasm.h"

#include "util/vec.h"
#include "util/except.h"
#include "spidir/module.h"

#if 0
    #define JIT_TRACE(fmt, ...) TRACE(fmt, ##__VA_ARGS__)
#else
    #define JIT_TRACE(fmt, ...)
#endif

typedef struct jit_function {
    spidir_funcref_t spidir;
    bool inited;
} jit_function_t;

typedef struct jit_global {
    size_t offset;
    spidir_value_type_t type;
} jit_global_t;

// One entry per wasm table. Tables are laid out in the same runtime buffer
// as globals, starting after the globals region. `offset` is the byte offset
// of slot 0 within the combined state buffer; `length` is the number of
// funcref slots (used to materialize the runtime bounds check).
typedef struct jit_table {
    size_t offset;
    uint32_t length;
} jit_table_t;

typedef vec(uint32_t) function_queue_t;

typedef struct jit_context {
    spidir_module_handle_t spidir;
    wasm_module_t* module;

    // the jit functions
    jit_function_t* functions;

    // the globals
    jit_global_t* globals;

    // the tables
    jit_table_t* tables;

    // queue of functions to do
    function_queue_t queue;
} jit_context_t;

static inline spidir_value_type_t jit_get_spidir_value_type(wasm_value_type_t type) {
    switch (type) {
        case WASM_VALUE_TYPE_F64: return SPIDIR_TYPE_F64;
        case WASM_VALUE_TYPE_F32: return SPIDIR_TYPE_F32;
        case WASM_VALUE_TYPE_I64: return SPIDIR_TYPE_I64;
        case WASM_VALUE_TYPE_I32: return SPIDIR_TYPE_I32;
        default: ASSERT(!"Invalid wasm type");
    }
}

static inline size_t jit_get_spidir_size(spidir_value_type_t type) {
    switch (type) {
        case SPIDIR_TYPE_F64: return 8;
        case SPIDIR_TYPE_F32: return 4;
        case SPIDIR_TYPE_I64: return 8;
        case SPIDIR_TYPE_I32: return 4;
        default: ASSERT(!"Invalid spidir type");
    }
}

static inline spidir_mem_size_t jit_get_spidir_mem_size(spidir_value_type_t type) {
    switch (type) {
        case SPIDIR_TYPE_F64: return SPIDIR_MEM_SIZE_8;
        case SPIDIR_TYPE_F32: return SPIDIR_MEM_SIZE_4;
        case SPIDIR_TYPE_I64: return SPIDIR_MEM_SIZE_8;
        case SPIDIR_TYPE_I32: return SPIDIR_MEM_SIZE_4;
        default: ASSERT(!"Invalid spidir type");
    }
}
