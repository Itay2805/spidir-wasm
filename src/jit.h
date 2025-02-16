#pragma once

#include <binary_reader.h>
#include <spidir/module.h>
#include <util/except.h>
#include <wasm/wasm.h>

/**
 * Jit a single wasm function
 */
wasm_err_t wasm_jit_function(
    spidir_module_handle_t spidir_module,
    wasm_module_t* module,
    wasm_func_t* func,
    binary_reader_t* code
);
