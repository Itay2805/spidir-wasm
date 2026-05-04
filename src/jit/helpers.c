#include "helpers.h"
#include "jit_internal.h"
#include "spidir/module.h"
#include "util/except.h"
#include "util/string.h"

#include <stdint.h>

static void helper_memory_copy(void* mem_base, uint32_t d, uint32_t s, uint32_t n) {
    memmove((char*)mem_base + d, (char*)mem_base + s, n);
}

static void helper_memory_fill(void* mem_base, uint32_t d, uint32_t val, uint32_t n) {
    memset((char*)mem_base + d, (uint8_t)val, n);
}

static void helper_trap(void) {
    __builtin_trap();
}

typedef struct helper_def {
    const char* const name;
    void* const address;
    const spidir_value_type_t ret_type;
    const uint8_t arg_count;
    const spidir_value_type_t* const arg_types;
} helper_def_t;

static const helper_def_t g_helper_defs[JIT_HELPER_COUNT] = {
    [JIT_HELPER_MEMORY_COPY] = (const helper_def_t){
        .name = "memory.copy",
        .address = helper_memory_copy,
        .ret_type = SPIDIR_TYPE_NONE,
        .arg_count = 4,
        .arg_types = (const spidir_value_type_t[]){
            SPIDIR_TYPE_PTR, // mem_base
            SPIDIR_TYPE_I32, // d
            SPIDIR_TYPE_I32, // s
            SPIDIR_TYPE_I32, // n
        },
    },
    [JIT_HELPER_MEMORY_FILL] = {
        .name = "memory.fill",
        .address = helper_memory_fill,
        .ret_type = SPIDIR_TYPE_NONE,
        .arg_count = 4,
        .arg_types = (const spidir_value_type_t[]){
            SPIDIR_TYPE_PTR, // mem_base
            SPIDIR_TYPE_I32, // d
            SPIDIR_TYPE_I32, // val
            SPIDIR_TYPE_I32, // n
        },
    },
    [JIT_HELPER_TRAP] = {
        .name = "trap",
        .address = helper_trap,
        .ret_type = SPIDIR_TYPE_NONE,
        .arg_count = 0,
        .arg_types = nullptr,
    },
};

wasm_err_t jit_get_helper(jit_context_t* ctx, jit_helper_kind_t kind, spidir_funcref_t* out) {
    wasm_err_t err = WASM_NO_ERROR;
    CHECK(kind < JIT_HELPER_COUNT);

    if (!ctx->helpers_inited[kind]) {
        const helper_def_t* def = &g_helper_defs[kind];
        ctx->helpers[kind] = spidir_module_create_extern_function(
            ctx->spidir,
            def->name,
            def->ret_type,
            def->arg_count,
            def->arg_types
        );
        ctx->helpers_inited[kind] = true;
    }

    *out = spidir_funcref_make_external(ctx->helpers[kind]);

cleanup:
    return err;
}

void* jit_helper_lookup_address(jit_context_t* ctx, uint32_t external_id) {
    for (int i = 0; i < JIT_HELPER_COUNT; i++) {
        if (ctx->helpers_inited[i] && ctx->helpers[i].id == external_id) {
            return g_helper_defs[i].address;
        }
    }
    return nullptr;
}
