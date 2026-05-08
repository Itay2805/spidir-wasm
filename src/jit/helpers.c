#include "helpers.h"
#include "jit_internal.h"
#include "spidir/module.h"
#include "util/except.h"
#include "util/string.h"

#include <stdatomic.h>
#include <stdint.h>

static void jit_helper_memory_copy(void* d, const void* s, uint32_t n) {
    if (n != 0) {
        memmove(d, s, n);
    }
}

static void jit_helper_memory_fill(void* d, uint32_t val, uint32_t n) {
    if (n != 0) {
        memset(d, (uint8_t)val, n);
    }
}

static void jit_helper_memory_init(void* dst, void* data, uint32_t data_len, uint32_t offset, uint32_t length) {
    // ensure we don't copy over the data length
    uint64_t top_offset = offset + length;
    ASSERT(top_offset <= data_len);

    if (length != 0) {
        // the data will be null if the code used 
        // data.drop on the data slot 
        ASSERT(data != nullptr);

        // copy it 
        memcpy(dst, data + offset, length);
    }
}

static void jit_helper_atomic_store_1(_Atomic(uint8_t)* addr, uint32_t value) { atomic_store(addr, value); }
static void jit_helper_atomic_store_2(_Atomic(uint16_t)* addr, uint32_t value) { atomic_store(addr, value); }
static void jit_helper_atomic_store_4(_Atomic(uint32_t)* addr, uint32_t value) { atomic_store(addr, value); }
static void jit_helper_atomic_store_8(_Atomic(uint64_t)* addr, uint64_t value) { atomic_store(addr, value); }

static uint32_t jit_helper_atomic_load_1(_Atomic(uint8_t)* addr) { return atomic_load(addr); }
static uint32_t jit_helper_atomic_load_2(_Atomic(uint16_t)* addr) { return atomic_load(addr); }
static uint32_t jit_helper_atomic_load_4(_Atomic(uint32_t)* addr) { return atomic_load(addr); }
static uint64_t jit_helper_atomic_load_8(_Atomic(uint64_t)* addr) { return atomic_load(addr); }

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
    
    [JIT_HELPER_MEMORY_COPY] = HELPER_FUNC(jit_helper_memory_copy, NONE, PTR, PTR, I32),
    [JIT_HELPER_MEMORY_FILL] = HELPER_FUNC(jit_helper_memory_fill, NONE, PTR, I32, I32),
    [JIT_HELPER_MEMORY_INIT] = HELPER_FUNC(jit_helper_memory_init, NONE, PTR, PTR, I32, I32, I32),

    [JIT_HELPER_TRAP] = HELPER_FUNC(jit_helper_trap, NONE),

    [JIT_HELPER_ATOMIC_STORE_1] = HELPER_FUNC(jit_helper_atomic_store_1, NONE, PTR, I32),
    [JIT_HELPER_ATOMIC_STORE_2] = HELPER_FUNC(jit_helper_atomic_store_2, NONE, PTR, I32),
    [JIT_HELPER_ATOMIC_STORE_4] = HELPER_FUNC(jit_helper_atomic_store_4, NONE, PTR, I32),
    [JIT_HELPER_ATOMIC_STORE_8] = HELPER_FUNC(jit_helper_atomic_store_8, NONE, PTR, I64),

    [JIT_HELPER_ATOMIC_LOAD_1] = HELPER_FUNC(jit_helper_atomic_load_1, I32, PTR),
    [JIT_HELPER_ATOMIC_LOAD_2] = HELPER_FUNC(jit_helper_atomic_load_2, I32, PTR),
    [JIT_HELPER_ATOMIC_LOAD_4] = HELPER_FUNC(jit_helper_atomic_load_4, I32, PTR),
    [JIT_HELPER_ATOMIC_LOAD_8] = HELPER_FUNC(jit_helper_atomic_load_8, I64, PTR),

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
