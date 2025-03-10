#pragma once

#include "wasm/wasm.h"

#include <spidir/module.h>

// config

struct wasm_config_t {
    // dump configuration
    spidir_dump_callback_t dump_callback;
    void* dump_callback_ctx;

    // should the optimizer run
    bool optimize;
};

// types

struct wasm_functype_t {
    wasm_valtype_vec_t params;
    wasm_valtype_vec_t results;
};

struct wasm_memorytype_t {
    wasm_limits_t limits;
};

struct wasm_valtype_t {
    wasm_valkind_t kind;
};

struct wasm_globaltype_t {
    wasm_val_t init;
    wasm_valtype_t* content;
    wasm_mutability_t mutability;
};

struct wasm_externtype_t {
    wasm_externkind_t kind;
    union {
        wasm_functype_t* functype;
        wasm_globaltype_t* globaltype;
        wasm_tabletype_t* tabletype;
        wasm_memorytype_t* memorytype;
    };
};

struct wasm_exporttype_t {
    wasm_name_t name;
    wasm_externtype_t* externtype;
    uint32_t index;
};

struct wasm_func_t {
    wasm_functype_t* functype;

    union {
        // the content of the function when it is
        // being jitted
        struct {
            spidir_function_t function;
        } jit;

        // the content of the function when its
        // turned into an instance
        struct {

        } instance;
    };
};

// containers

struct wasm_engine_t {
    // the config set for the engine
    wasm_config_t* config;
};

struct wasm_store_t {
    // the engine of this store
    wasm_engine_t* engine;

};


struct wasm_module_t {
    wasm_functype_vec_t typefuncs;
    wasm_memorytype_vec_t memorytypes;
    wasm_globaltype_vec_t globaltypes;
    wasm_exporttype_vec_t exporttypes;

    wasm_func_t** functions;
};

typedef enum wasm_section_id : uint8_t {
    WASM_CUSTOM_SECTION = 0,
    WASM_TYPE_SECTION = 1,
    WASM_IMPORT_SECTION = 2,
    WASM_FUNCTION_SECTION = 3,
    WASM_TABLE_SECTION = 4,
    WASM_MEMORY_SECTION = 5,
    WASM_GLOBAL_SECTION = 6,
    WASM_EXPORT_SECTION = 7,
    WASM_START_SECTION = 8,
    WASM_ELEMENT_SECTION = 9,
    WASM_CODE_SECTION = 10,
    WASM_DATA_SECTION = 11,
    WASM_DATA_COUNT_SECTION = 12,
} wasm_section_id_t;

/**
 * Helper function to parse a valtype
 */
wasm_valtype_t* wasm_parse_valtype(uint8_t valtype);
