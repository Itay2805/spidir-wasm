#include "vec.h"

#include "util/except.h"
#include "wasm/host.h"

typedef struct vec {
    uint32_t length;
    uint32_t capacity;
    void* data;
} vec_t;

wasm_err_t vec_grow_sized(void* a, size_t elemsize, uint32_t addlen, uint32_t min_cap) {
    wasm_err_t err = WASM_NO_ERROR;

    vec_t* arr = a;

    // get the new min len
    size_t min_len = arr->length + addlen;

    // compute the minimum capacity needed
    if (min_len > min_cap) {
        min_cap = min_len;
    }

    if (min_cap <= arr->capacity) {
        goto cleanup;
    }

    // increase needed caapcity to guarantee O(1) amortized
    // TODO: how to make this not overflow
    if (min_cap < 2 * arr->capacity) {
        min_cap = 2 * arr->capacity;
    } else if (min_cap < 4) {
        min_cap = 4;
    }
    CHECK(min_cap <= UINT32_MAX);

    size_t size = 0;
    CHECK(!__builtin_mul_overflow(elemsize, min_cap, &size));

    void* b = wasm_host_realloc(arr->data, size);
    CHECK(b != nullptr);

    arr->data = b;
    arr->capacity = min_cap;

cleanup:
    return err;
}
