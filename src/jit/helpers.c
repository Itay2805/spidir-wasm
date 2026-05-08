#include "helpers.h"
#include "jit_internal.h"
#include "spidir/module.h"
#include "util/except.h"
#include "util/string.h"

#include <stdatomic.h>
#include <stdint.h>

static void jit_helper_memory_copy(void* mem_base, uint32_t d, uint32_t s, uint32_t n) {
    memmove((char*)mem_base + d, (char*)mem_base + s, n);
}

static void jit_helper_memory_fill(void* mem_base, uint32_t d, uint32_t val, uint32_t n) {
    memset((char*)mem_base + d, (uint8_t)val, n);
}

static uint32_t jit_helper_atomic_rmw_cmpxchg_1(_Atomic(uint8_t)* addr, uint32_t expected, uint32_t replacement) { uint8_t old = expected; atomic_compare_exchange_strong(addr, &old, replacement); return old; }
static uint32_t jit_helper_atomic_rmw_cmpxchg_2(_Atomic(uint16_t)* addr, uint32_t expected, uint32_t replacement) { uint16_t old = expected; atomic_compare_exchange_strong(addr, &old, replacement); return old; }
static uint32_t jit_helper_atomic_rmw_cmpxchg_4(_Atomic(uint32_t)* addr, uint32_t expected, uint32_t replacement) { uint32_t old = expected; atomic_compare_exchange_strong(addr, &old, replacement); return old; }
static uint64_t jit_helper_atomic_rmw_cmpxchg_8(_Atomic(uint64_t)* addr, uint64_t expected, uint64_t replacement) { uint64_t old = expected; atomic_compare_exchange_strong(addr, &old, replacement); return old; }

static void jit_helper_trap(void) {
    __builtin_trap();
}

//----------------------------------------------------------------------------------------------------------------------
// The actual helper definitions
//----------------------------------------------------------------------------------------------------------------------

typedef struct helper_def {
    const char* const name;
    void* const address;
    const spidir_value_type_t ret_type;
    const spidir_value_type_t* const arg_types;
} helper_def_t;

#define HELPER_FUNC_MAKE_SIG(x) SPIDIR_TYPE_##x
#define HELPER_FUNC_SIG(...) (const spidir_value_type_t[]){ MAP(HELPER_FUNC_MAKE_SIG, COMMA, ## __VA_ARGS__, NONE) }

#define HELPER_FUNC(_func, _ret_type, ...) \
    { \
        .name = #_func, \
        .address = _func, \
        .ret_type = SPIDIR_TYPE_##_ret_type, \
        .arg_types = HELPER_FUNC_SIG(__VA_ARGS__) \
    }

static const helper_def_t m_helper_defs[JIT_HELPER_COUNT] = {
    [JIT_HELPER_MEMORY_SIZE] = HELPER_FUNC(wasm_host_memory_size, I32, PTR),
    [JIT_HELPER_MEMORY_GROW] = HELPER_FUNC(wasm_host_memory_grow, I32, PTR, I32),
    
    [JIT_HELPER_MEMORY_COPY] = HELPER_FUNC(jit_helper_memory_copy, NONE, PTR, I32, I32, I32),
    [JIT_HELPER_MEMORY_FILL] = HELPER_FUNC(jit_helper_memory_fill, NONE, PTR, I32, I32, I32),

    [JIT_HELPER_TRAP] = HELPER_FUNC(jit_helper_trap, NONE),

    [JIT_HELPER_ATOMIC_RMW_CMPXCHG_1] = HELPER_FUNC(jit_helper_atomic_rmw_cmpxchg_1, I32, PTR, I32, I32),
    [JIT_HELPER_ATOMIC_RMW_CMPXCHG_2] = HELPER_FUNC(jit_helper_atomic_rmw_cmpxchg_2, I32, PTR, I32, I32),
    [JIT_HELPER_ATOMIC_RMW_CMPXCHG_4] = HELPER_FUNC(jit_helper_atomic_rmw_cmpxchg_4, I32, PTR, I32, I32),
    [JIT_HELPER_ATOMIC_RMW_CMPXCHG_8] = HELPER_FUNC(jit_helper_atomic_rmw_cmpxchg_8, I64, PTR, I64, I64),
};

wasm_err_t jit_get_helper(jit_context_t* ctx, jit_helper_kind_t kind, spidir_funcref_t* out) {
    wasm_err_t err = WASM_NO_ERROR;
    CHECK(kind < JIT_HELPER_COUNT);

    if (!ctx->helpers_inited[kind]) {
        const helper_def_t* def = &m_helper_defs[kind];
        CHECK(def->name != nullptr);

        size_t arg_count = 0;
        for (; def->arg_types[arg_count] != SPIDIR_TYPE_NONE; arg_count++);
        
        ctx->helpers[kind] = spidir_module_create_extern_function(
            ctx->spidir,
            def->name,
            def->ret_type,
            arg_count,
            def->arg_types
        );
        ctx->helpers_inited[kind] = true;
    }

    *out = spidir_funcref_make_external(ctx->helpers[kind]);

cleanup:
    return err;
}

const char* jit_get_helper_name(const char* address) {
    for (int i = 0; i < JIT_HELPER_COUNT; i++) {
        if (m_helper_defs[i].address == address) {
            return m_helper_defs[i].name;
        }
    }
    return nullptr;
}

void* jit_helper_lookup_address(jit_context_t* ctx, uint32_t external_id) {
    for (int i = 0; i < JIT_HELPER_COUNT; i++) {
        if (ctx->helpers_inited[i] && ctx->helpers[i].id == external_id) {
            return m_helper_defs[i].address;
        }
    }
    return nullptr;
}
