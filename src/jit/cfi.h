#pragma once

#include "jit/helpers.h"
#include "wasm/error.h"
#include "wasm/wasm.h"

wasm_err_t jit_create_cfi_thunk(jit_context_t* ctx, uint32_t funcidx);

uint64_t jit_cfi_get_type_id(jit_context_t* ctx, wasm_type_t* type);
