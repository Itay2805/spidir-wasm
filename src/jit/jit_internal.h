#pragma once

#include "wasm/wasm.h"

#include "helpers.h"
#include "util/vec.h"
#include "util/except.h"
#include "spidir/module.h"
#include "wasm/jit.h"

typedef struct jit_function {
    spidir_funcref_t spidir;
    spidir_function_t cfi_thunk;
    void* address;
    bool inited;
    bool has_cfi;
} jit_function_t;

typedef struct jit_global {
    size_t offset;
    spidir_value_type_t type;
} jit_global_t;

typedef struct jit_table {
    spidir_extern_global_t global;
    uint32_t length;
    bool used;
} jit_table_t;

typedef struct jit_data {
    size_t offset;
} jit_data_t;

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

    // the data segments
    jit_data_t* data;

    // queue of functions to do
    function_queue_t queue;

    // Lazy registry of JIT runtime helpers. Filled on first call to
    // jit_get_helper(kind); the relocation applier resolves the spidir
    // extern function id back to the host C address via this table.
    spidir_extern_function_t helpers[JIT_HELPER_COUNT];
    bool helpers_inited[JIT_HELPER_COUNT];

    // the config
    wasm_jit_config_t* config;
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
