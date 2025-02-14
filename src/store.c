#include <wasm/host.h>

#include "internal.h"

WASM_API_EXTERN wasm_store_t* wasm_store_new(wasm_engine_t* engine) {
    wasm_store_t* wasm_store = wasm_host_calloc(sizeof(*wasm_store), 1);
    if (wasm_store == NULL) {
        return NULL;
    }

    return wasm_store;
}

WASM_API_EXTERN void wasm_store_delete(wasm_store_t* store) {
    if (store == NULL) return;

    wasm_host_free(store);
}