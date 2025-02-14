#pragma once

#include <stddef.h>
#include <stdint.h>

#include <util/except.h>
#include <wasm/error.h>
#include <wasm/wasm.h>

typedef struct binary_reader {
    void* ptr;
    size_t size;
} binary_reader_t;

void* binary_reader_pull(binary_reader_t* reader, size_t size);
wasm_err_t binary_reader_pull_u64(binary_reader_t* reader, uint64_t* out);
wasm_err_t binary_reader_pull_i64(binary_reader_t* reader, int64_t* out);

#define BINARY_READER_PULL(reader, size) \
    ({ \
        void* result__ = binary_reader_pull(reader, size); \
        CHECK(result__ != NULL); \
        result__; \
    })

#define BINARY_READER_PULL_BYTE(reader) \
    ({ \
        binary_reader_t* reader__ = reader; \
        CHECK(reader__->size >= 1); \
        uint8_t result__ = *(uint8_t*)reader__->ptr; \
        reader__->size--; \
        reader__->ptr++; \
        result__; \
    })

#define BINARY_READER_PULL_U32(reader) \
    ({ \
        uint64_t result__; \
        RETHROW(binary_reader_pull_u64(reader, &result__)); \
        (uint32_t)result__; \
    })

#define BINARY_READER_PULL_U64(reader) \
    ({ \
        uint64_t result__; \
        RETHROW(binary_reader_pull_u32(reader, &result__)); \
        result__; \
    })

#define BINARY_READER_PULL_I32(reader) \
    ({ \
        int64_t result__; \
        RETHROW(binary_reader_pull_i64(reader, &result__)); \
        (int32_t)result__; \
    })

#define BINARY_READER_PULL_I64(reader) \
    ({ \
        int64_t result__; \
        RETHROW(binary_reader_pull_i64(reader, &result__)); \
        result__; \
    })
