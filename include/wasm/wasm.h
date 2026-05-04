#pragma once

#include <stdint.h>
#include <stddef.h>

#include "error.h"

#define WASM_PAGE_SIZE (65536)

typedef enum wasm_value_type {
    // Number Types
    WASM_VALUE_TYPE_F64,
    WASM_VALUE_TYPE_F32,
    WASM_VALUE_TYPE_I64,
    WASM_VALUE_TYPE_I32,
} wasm_value_type_t;
