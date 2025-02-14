#include <wasm/host.h>

#include "internal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Value vectors
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef WASM_DECLARE_VEC
#define WASM_DECLARE_VEC(name, ptr_or_none) \
    WASM_API_EXTERN void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) { \
        out->data = NULL; \
        out->size = 0; \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_new_uninitialized(wasm_##name##_vec_t* out, size_t size) { \
        out->data = wasm_host_calloc(sizeof(*out->data), size); \
        if (out->data == NULL) return; \
        out->size = size; \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t size, wasm_##name##_t ptr_or_none const data[]) { \
        out->data = wasm_host_calloc(sizeof(*out->data), size); \
        if (out->data == NULL) return; \
        out->size = size; \
        __builtin_memcpy(out->data, data, sizeof(*out->data) * size); \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_copy(wasm_##name##_vec_t* out, const wasm_##name##_vec_t* v) { \
        wasm_##name##_vec_new(out, v->size, v->data); \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_delete(wasm_##name##_vec_t* vec) { \
        wasm_host_free(vec->data); \
        vec->data = NULL; \
        vec->size = 0; \
    }

WASM_DECLARE_VEC(byte, )
WASM_DECLARE_VEC(val, )

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type vectors - these own reference to stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef WASM_DECLARE_VEC
#define WASM_DECLARE_VEC(name, ptr_or_none) \
    WASM_API_EXTERN void wasm_##name##_vec_new_empty(wasm_##name##_vec_t* out) { \
        out->data = NULL; \
        out->size = 0; \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_new_uninitialized(wasm_##name##_vec_t* out, size_t size) { \
        out->data = wasm_host_calloc(sizeof(*out->data), size); \
        if (out->data == NULL) return; \
        out->size = size; \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_new(wasm_##name##_vec_t* out, size_t size, wasm_##name##_t ptr_or_none const data[]) { \
        out->data = wasm_host_calloc(sizeof(*out->data), size); \
        if (out->data == NULL) return; \
        out->size = size; \
        __builtin_memcpy(out->data, data, sizeof(*out->data) * size); \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_copy(wasm_##name##_vec_t* out, const wasm_##name##_vec_t* v) { \
        out->data = wasm_host_calloc(sizeof(*out->data), v->size); \
        if (out->data == NULL) return; \
        out->size = v->size; \
        for (size_t i = 0; i < out->size; i++) { \
            if (v->data[i] != NULL)  { \
                out->data[i] = wasm_##name##_copy(v->data[i]); \
            } \
        } \
    } \
    WASM_API_EXTERN void wasm_##name##_vec_delete(wasm_##name##_vec_t* vec) { \
        for (size_t i = 0; i < vec->size; i++) { \
            wasm_##name##_delete(vec->data[i]); \
        } \
        wasm_host_free(vec->data); \
        vec->data = NULL; \
        vec->size = 0; \
    }

// WASM_DECLARE_VEC(frame, *)
// WASM_DECLARE_VEC(extern, *)
WASM_DECLARE_VEC(valtype, *)
WASM_DECLARE_VEC(functype, *)
WASM_DECLARE_VEC(globaltype, *)
// WASM_DECLARE_VEC(tabletype, *)
WASM_DECLARE_VEC(memorytype, *)
WASM_DECLARE_VEC(externtype, *)
// WASM_DECLARE_VEC(importtype, *)
WASM_DECLARE_VEC(exporttype, *)
