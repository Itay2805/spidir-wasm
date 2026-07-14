#pragma once

#include "jit/helpers.h"
#include "util/vec.h"
#include "wasm/error.h"
#include "wasm/jit.h"

wasm_err_t jit_codegen(wasm_module_jit_t* jit, jit_context_t* ctx, wasm_jit_config_t* config);
