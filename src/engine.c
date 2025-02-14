#include <wasm/host.h>

#include "internal.h"

WASM_API_EXTERN wasm_engine_t* wasm_engine_new(void) {
    return wasm_host_calloc(sizeof(wasm_engine_t), 1);
}

WASM_API_EXTERN wasm_engine_t* wasm_engine_new_with_config(wasm_config_t* config) {
    return wasm_engine_new();
}

WASM_API_EXTERN void wasm_engine_delete(wasm_engine_t* engine) {
    if (engine == NULL) return;

    wasm_host_free(engine);
}
