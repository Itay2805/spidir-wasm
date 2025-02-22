#include <wasm/host.h>

#include "internal.h"

WASM_API_EXTERN wasm_engine_t* wasm_engine_new(void) {
    return wasm_engine_new_with_config(NULL);
}

WASM_API_EXTERN wasm_engine_t* wasm_engine_new_with_config(wasm_config_t* config) {
    wasm_engine_t* engine = wasm_host_calloc(1, sizeof(wasm_engine_t));
    if (engine == NULL) return NULL;
    engine->config = config;
    return engine;
}

WASM_API_EXTERN void wasm_engine_delete(wasm_engine_t* engine) {
    if (engine == NULL) return;
    wasm_config_delete(engine->config);
    wasm_host_free(engine);
}

WASM_API_EXTERN wasm_config_t* wasm_config_new(void) {
    return wasm_host_calloc(1, sizeof(wasm_config_t));
}

WASM_API_EXTERN void wasm_config_spidir_dump(wasm_config_t* config, spidir_dump_callback_t callback, void* ctx) {
    config->dump_callback = callback;
    config->dump_callback_ctx = ctx;
}

WASM_API_EXTERN void wasm_config_optimize(wasm_config_t* config, bool optimize) {
    config->optimize = optimize;
}

WASM_API_EXTERN void wasm_config_delete(wasm_config_t* config) {
    wasm_host_free(config);
}

