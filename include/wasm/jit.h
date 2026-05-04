#pragma once

#include <stdbool.h>
#include <spidir/codegen.h>
#include "wasm/wasm.h"

#include "error.h"

typedef struct wasm_jit_config {
    /**
     * For dumping the spidir module, can be set to
     * null to not dump it at all
     */
    spidir_dump_callback_t dump_callback;
    void* dump_arg;

    /**
     * Should we run the spidir optimizations
     */
    bool optimize;
} wasm_jit_config_t;

typedef union wasm_jit_export {
    struct {
        void* address;
    } func;
} wasm_jit_export_t;

typedef struct wasm_module_jit {
    void* binary;
    size_t rx_page_count;
    size_t ro_page_count;

    // the addresses of exported functions
    wasm_jit_export_t* exports;
} wasm_module_jit_t;

wasm_err_t wasm_module_jit(wasm_module_t* module, wasm_module_jit_t* jitted_module, wasm_jit_config_t* config);

void wasm_module_jit_free(wasm_module_jit_t* jit);
