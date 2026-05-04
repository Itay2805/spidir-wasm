#pragma once

#include <stdint.h>

#include "util/except.h"
#include "jit_internal.h"

wasm_err_t jit_prepare_function(jit_context_t* ctx, uint32_t funcidx);

wasm_err_t jit_function(jit_context_t* context, uint32_t funcidx);
