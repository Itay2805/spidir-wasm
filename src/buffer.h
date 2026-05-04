#pragma once

#include <stddef.h>
#include <stdint.h>

#include "util/string.h"
#include "wasm/error.h"
#include "wasm/wasm.h"

typedef struct buffer {
    void* data;
    size_t len;
} buffer_t;

static inline buffer_t init_buffer(void* data, size_t len) {
    return (buffer_t){
        .data = data,
        .len = len,
    };
}

wasm_err_t buffer_push(buffer_t* buffer, const void* data, size_t len);
wasm_err_t buffer_fill(buffer_t* buffer, uint8_t value, size_t len);
wasm_err_t buffer_align(buffer_t* buffer, uint8_t value, size_t alignment);

wasm_err_t buffer_pull_u32(buffer_t* buffer, uint32_t* value);
wasm_err_t buffer_pull_u64(buffer_t* buffer, uint64_t* value);
wasm_err_t buffer_pull_i32(buffer_t* buffer, int32_t* value);
wasm_err_t buffer_pull_i64(buffer_t* buffer, int64_t* value);

wasm_err_t buffer_pull_name(buffer_t* buffer, buffer_t* name);

static inline void* buffer_pull(buffer_t* buffer, size_t len) {
    if (buffer->len < len) {
        return nullptr;
    }
    void* ptr = buffer->data;
    buffer->data += len;
    buffer->len -= len;
    return ptr;
}

#define BUFFER_PULL(type, buffer) \
    ({ \
        void* data__ = buffer_pull(buffer, sizeof(type)); \
        CHECK(data__ != nullptr); \
        type value__; \
        memcpy(&value__, data__, sizeof(type)); \
        value__; \
    })

#define BUFFER_PULL_U32(buffer) \
    ({ \
        uint32_t value__ = 0; \
        RETHROW(buffer_pull_u32(buffer, &value__)); \
        value__; \
    })

#define BUFFER_PULL_U64(buffer) \
    ({ \
        uint64_t value__ = 0; \
        RETHROW(buffer_pull_u64(buffer, &value__)); \
        value__; \
    })

#define BUFFER_PULL_I32(buffer) \
    ({ \
        int32_t value__ = 0; \
        RETHROW(buffer_pull_i32(buffer, &value__)); \
        value__; \
    })

#define BUFFER_PULL_I64(buffer) \
    ({ \
        int64_t value__ = 0; \
        RETHROW(buffer_pull_i64(buffer, &value__)); \
        value__; \
    })

wasm_err_t buffer_pull_val_type(buffer_t* buffer, wasm_value_type_t* valtype);
