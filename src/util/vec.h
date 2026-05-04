#pragma once

#include "util/except.h"
#include "wasm/host.h"
#include <stddef.h>

#define vec(type) \
    struct { \
        uint32_t length; \
        uint32_t capacity; \
        type* elements; \
    }

#define vec_set_cap(x, n) \
    do { \
        vec_grow(x, 0, n); \
    } while(0);

#define vec_set_len(x, n) \
    do { \
        typeof(x) arr__ = x; \
        size_t n__ = n; \
        if (arr__->capacity < n) { \
            arrsetcap(arr__, n); \
        } \
        arr__->length = n; \
    } while(0);

#define vec_push(x, ...) \
    do { \
        typeof(x) arr__ = (x); \
        vec_maybe_grow(arr__, 1); \
        arr__->elements[arr__->length++] = __VA_ARGS__; \
    } while(0)

#define vec_pop(x) \
    ({ \
        typeof(x) arr__ = (x); \
        CHECK(arr__->length > 0); \
        arr__->length--; \
        arr__->elements[arr__->length]; \
    })

#define vec_add(x, n) \
    ({ \
        typeof(x) arr__ = (x); \
        uint32_t n__ = (n); \
        vec_maybe_grow(arr__, n__); \
        typeof(arr__->elements) ptr__ = &arr__->elements[arr__->length]; \
        arr__->length += n__; \
        ptr__; \
    })

#define vec_last(x) \
    *({ \
        typeof(x) arr__ = (x); \
        CHECK(arr__->length > 0); \
        &arr__->elements[arr__->length - 1]; \
    })

#define vec_maybe_grow(x, n) \
    do { \
        typeof(x) arr___ = x; \
        uint32_t n___ = n; \
        if (arr___->length + n___ > arr___->capacity) { \
            vec_grow(arr___, n___, 0); \
        } \
    } while(0)

#define vec_grow(x, n, c) \
    do { \
        RETHROW(vec_grow_sized(x, sizeof(*(x)->elements), n, c)); \
    } while(0)

wasm_err_t vec_grow_sized(void* a, size_t elemsize, uint32_t addlen, uint32_t min_cap);

#define vec_free(x) \
    do { \
        typeof(x) arr__ = x; \
        wasm_host_free(arr__->elements); \
        arr__->elements = nullptr; \
        arr__->capacity = 0; \
        arr__->length = 0; \
    } while (0)

