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

typedef struct wasm_value {
    wasm_value_type_t kind;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
    } value;
} wasm_value_t;

typedef struct wasm_type {
    wasm_value_type_t* arg_types;
    wasm_value_type_t* result_types;
    uint32_t arg_types_count;
    uint32_t result_types_count;
} wasm_type_t;

typedef struct wasm_global {
    wasm_value_t value;
    bool mutable;
} wasm_global_t;

typedef struct wasm_table {
    uint32_t min;
    uint32_t max;
} wasm_table_t;

typedef struct wasm_elem_segment {
    uint32_t tableidx;
    uint32_t offset;
    uint32_t funcs_count;
    uint32_t* funcs;
} wasm_elem_segment_t;

typedef struct wasm_data_segment {
    uint32_t offset;
    uint32_t len;
    void* data;
} wasm_data_segment_t;

typedef enum wasm_extern_type {
    WASM_EXTERN_FUNC,
    WASM_EXTERN_TABLE,
    WASM_EXTERN_MEM,
    WASM_EXTERN_GLOBAL,
    WASM_EXTERN_TAG,
} wasm_extern_type_t;

typedef struct wasm_import {
    char* module_name;
    char* item_name;
    wasm_extern_type_t kind;
    uint32_t index;
} wasm_import_t;

typedef enum wasm_export_kind {
    WASM_EXPORT_FUNC,
    WASM_EXPORT_TABLE,
    WASM_EXPORT_MEMORY,
    WASM_EXPORT_GLOBAL,
    WASM_EXPORT_TAG,
} wasm_export_type_t;

typedef struct wasm_export {
    char* name;
    wasm_export_type_t kind;
    uint32_t index;
} wasm_export_t;

typedef struct wasm_code {
    void* code;
    uint32_t length;
} wasm_code_t;

typedef uint32_t typeidx_t;

typedef struct wasm_module {
    wasm_type_t* types;
    typeidx_t* functions;
    wasm_global_t* globals;
    wasm_import_t* imports;
    wasm_export_t* exports;
    wasm_table_t* tables;
    wasm_elem_segment_t* elems;
    wasm_data_segment_t* data_segments;

    // same amount as functions count
    wasm_code_t* code;

    uint64_t memory_min;
    uint64_t memory_max;

    uint32_t types_count;
    uint32_t functions_count;
    uint32_t globals_count;
    uint32_t imports_count;
    uint32_t exports_count;
    uint32_t tables_count;
    uint32_t elems_count;
    uint32_t data_segments_count;
} wasm_module_t;

wasm_err_t wasm_load_module(wasm_module_t* module, void* data, size_t size);

/**
 * Find an export in the module, returns null if not found
 */
int64_t wasm_find_export(wasm_module_t* module, const char* name);

/**
 * Free the contents of the given module
 */
void wasm_module_free(wasm_module_t* module);
