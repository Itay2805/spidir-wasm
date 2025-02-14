#include <binary_reader.h>
#include <stb_ds.h>
#include <stb_ds.h>
#include <stdlib.h>
#include <util/except.h>
#include <wasm/error.h>

#include "internal.h"

WASM_API_EXTERN wasm_module_t* wasm_module_new(wasm_store_t* store, const wasm_byte_vec_t* binary) {
    return wasm_module_deserialize(store, binary);
}

static wasm_err_t wasm_module_verify_header(binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t* magic = binary_reader_pull(reader, 4);
    CHECK(magic != NULL);
    CHECK(magic[0] == 0x00);
    CHECK(magic[1] == 0x61);
    CHECK(magic[2] == 0x73);
    CHECK(magic[3] == 0x6D);

    uint8_t* version = binary_reader_pull(reader, 4);
    CHECK(version != NULL);
    CHECK(version[0] == 0x01);
    CHECK(version[1] == 0x00);
    CHECK(version[2] == 0x00);
    CHECK(version[3] == 0x00);

cleanup:
    return err;
}

static wasm_valtype_t* wasm_parse_valtype(uint8_t valtype) {
    switch (valtype) {
        case 0x7F: return wasm_valtype_new_i32();
        case 0x7E: return wasm_valtype_new_i64();
        case 0x7D: return wasm_valtype_new_f32();
        case 0x7C: return wasm_valtype_new_f64();
        case 0x70: return wasm_valtype_new_funcref();
        case 0x6F: return wasm_valtype_new_externref();
        default: return NULL;
    }
}

static wasm_err_t wasm_parse_resulttype(binary_reader_t* reader, wasm_valtype_vec_t* result) {
    wasm_err_t err = WASM_NO_ERROR;

    wasm_valtype_vec_new_uninitialized(result, BINARY_READER_PULL_U32(reader));
    CHECK(result->data != NULL);

    const uint8_t* data = BINARY_READER_PULL(reader, result->size);
    for (int64_t i = 0; i < result->size; i++) {
        // create a new type from the array
        wasm_valtype_t* valtype = wasm_parse_valtype(data[i]);
        CHECK(valtype != NULL);

        // store it directly since we own it
        result->data[i] = valtype;
    }

cleanup:
    return err;
}

static wasm_err_t wasm_module_parse_type_section(wasm_module_t* module, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(module->typefuncs.data == NULL);

    uint32_t type_count = BINARY_READER_PULL_U32(reader);
    wasm_functype_vec_new_uninitialized(&module->typefuncs, type_count);
    CHECK(module->typefuncs.data != NULL);

    for (int64_t i = 0; i < type_count; i++) {
        // create an empty functype
        wasm_functype_t* functype = wasm_functype_new(NULL, NULL);
        CHECK(functype != NULL);
        module->typefuncs.data[i] = functype;

        // func type prefix
        CHECK(BINARY_READER_PULL_BYTE(reader) == 0x60);

        // parse the params and results
        RETHROW(wasm_parse_resulttype(reader, &functype->params));
        RETHROW(wasm_parse_resulttype(reader, &functype->results));
    }

cleanup:
    return err;
}

static void u32_to_hex(uint32_t value, char arr[8]) {
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (value >> (28 - i * 4)) & 0xF;
        arr[i] = "0123456789ABCDEF"[nibble];
    }
}

static wasm_err_t wasm_module_parse_function_section(
    spidir_module_handle_t spidir_module,
    wasm_module_t* module,
    binary_reader_t* reader
) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_type_t* arg_types = NULL;

    CHECK(module->typefuncs.data != NULL);
    CHECK(module->functions == NULL);

    uint32_t function_count = BINARY_READER_PULL_U32(reader);
    arrsetlen(module->functions, function_count);
    __builtin_memset(module->functions, 0, function_count * sizeof(*module->functions));

    for (int64_t i = 0; i < function_count; i++) {
        // create an empty function
        wasm_func_t* func = wasm_host_calloc(1, sizeof(*func));
        CHECK(func != NULL);
        module->functions[i] = func;

        uint32_t func_type_idx = BINARY_READER_PULL_U32(reader);
        CHECK(module->typefuncs.data[func_type_idx] != NULL);
        wasm_functype_t* func_type = module->typefuncs.data[func_type_idx];

        // create an array of arg types
        for (int64_t j = 0; j < func_type->params.size; j++) {
            spidir_value_type_t type;
            switch (func_type->params.data[j]->kind) {
                case WASM_I32: type = SPIDIR_TYPE_I32; break;
                case WASM_I64: type = SPIDIR_TYPE_I64; break;
                default: CHECK_FAIL();
            }
            arrpush(arg_types, type);
        }

        // ensure there is either zero or one result types
        spidir_value_type_t result_type;
        if (func_type->results.size == 0) {
            result_type = SPIDIR_TYPE_NONE;
        } else if (func_type->results.size == 1) {
            switch (func_type->results.data[0]->kind) {
                case WASM_I32: result_type = SPIDIR_TYPE_I32; break;
                case WASM_I64: result_type = SPIDIR_TYPE_I64; break;
                default: CHECK_FAIL();
            }
        } else {
            CHECK_FAIL("Maximum of 1 result types is supported");
        }

        // unique name
        // TODO: use debug info to give proper names instead
        char name[sizeof("func") + 8] = "func";
        u32_to_hex(i, name + 4);

        // create the spidir function, we will populate it later
        // when we get to the code section
        func->jit.function = spidir_module_create_function(
            spidir_module,
            name,
            result_type,
            arrlen(arg_types),
            arg_types
        );

        arrfree(arg_types);
    }

cleanup:
    arrfree(arg_types);

    return err;
}

static wasm_err_t wasm_module_parse_memory_section(wasm_module_t* module, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(module->memorytypes.data == NULL);

    uint32_t memorytypes_count = BINARY_READER_PULL_U32(reader);
    wasm_memorytype_vec_new_uninitialized(&module->memorytypes, memorytypes_count);
    CHECK(module->memorytypes.data != NULL);

    for (int64_t i = 0; i < memorytypes_count; i++) {
        // create an empty functype
        wasm_memorytype_t* memorytype = wasm_memorytype_new(NULL);
        CHECK(memorytype != NULL);
        module->memorytypes.data[i] = memorytype;

        uint8_t type = BINARY_READER_PULL_BYTE(reader);
        memorytype->limits.min = BINARY_READER_PULL_U32(reader);
        if (type == 0x01) {
            memorytype->limits.max = BINARY_READER_PULL_U32(reader);
        } else {
            CHECK(type == 0x00);
            memorytype->limits.max = wasm_limits_max_default;
        }
    }

cleanup:
    return err;
}

static wasm_err_t wasm_parse_constant_expression(wasm_val_t* value, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    // the constant byte
    switch (BINARY_READER_PULL_BYTE(reader)) {
        case 0x41: value->kind = WASM_I32; value->of.i32 = BINARY_READER_PULL_I32(reader); break;
        case 0x42: value->kind = WASM_I64; value->of.i32 = BINARY_READER_PULL_I64(reader); break;
        default: CHECK_FAIL();
    }

    // check for the end byte
    CHECK(BINARY_READER_PULL_BYTE(reader) == 0x0B);

cleanup:
    return err;
}

static wasm_err_t wasm_module_parse_global_section(wasm_module_t* module, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(module->globaltypes.data == NULL);

    uint32_t globals_count = BINARY_READER_PULL_U32(reader);
    wasm_globaltype_vec_new_uninitialized(&module->globaltypes, globals_count);
    CHECK(module->globaltypes.data != NULL);

    for (int64_t i = 0; i < globals_count; i++) {
        // create an empty functype
        wasm_globaltype_t* globaltype = wasm_globaltype_new(NULL, 0);
        CHECK(globaltype != NULL);
        module->globaltypes.data[i] = globaltype;

        wasm_valtype_t* valtype = wasm_parse_valtype(BINARY_READER_PULL_BYTE(reader));
        CHECK(valtype != NULL);
        globaltype->content = valtype;

        // parse the mutability
        switch (BINARY_READER_PULL_BYTE(reader)) {
            case 0x00: globaltype->mutability = WASM_CONST; break;
            case 0x01: globaltype->mutability = WASM_VAR; break;
            default: CHECK_FAIL();
        }

        // parse the initial value of the global
        RETHROW(wasm_parse_constant_expression(&globaltype->init, reader));
    }

cleanup:
    return err;
}

static wasm_err_t wasm_module_parse_export_section(wasm_module_t* module, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(module->exporttypes.data == NULL);

    uint32_t exports_count = BINARY_READER_PULL_U32(reader);
    wasm_exporttype_vec_new_uninitialized(&module->exporttypes, exports_count);
    CHECK(module->exporttypes.data != NULL);

    for (int64_t i = 0; i < exports_count; i++) {
        // create an empty functype
        wasm_exporttype_t* exporttype = wasm_exporttype_new(NULL, NULL);
        CHECK(exporttype != NULL);
        module->exporttypes.data[i] = exporttype;

        // copy the name
        wasm_byte_vec_new_uninitialized(&exporttype->name, BINARY_READER_PULL_U32(reader));
        CHECK(exporttype->name.data != NULL);
        __builtin_memcpy(exporttype->name.data, BINARY_READER_PULL(reader, exporttype->name.size), exporttype->name.size);

        uint8_t export_kind = BINARY_READER_PULL_BYTE(reader);

        // the index of the extern
        uint32_t index = BINARY_READER_PULL_U32(reader);
        exporttype->index = index;

        // initialize extern
        switch (export_kind) {
            case 0x00: {
                CHECK(index < arrlen(module->functions));
                exporttype->externtype = wasm_functype_as_externtype(module->functions[index]->functype);
            } break;

            case 0x01: {
                // TODO: table support
                CHECK_FAIL();
                // CHECK(index < module.table);
                // exporttype->externtype = wasm_functype_as_externtype(module->functions[index]->functype);
            } break;

            case 0x02: {
                CHECK(index < module->memorytypes.size);
                exporttype->externtype = wasm_memorytype_as_externtype(module->memorytypes.data[index]);
            } break;

            case 0x03: {
                CHECK(index < module->globaltypes.size);
                exporttype->externtype = wasm_globaltype_as_externtype(module->globaltypes.data[index]);
            } break;

            default:
                CHECK_FAIL();
        }

    }

cleanup:
    return err;
}

static wasm_err_t wasm_module_parse_code_section(wasm_module_t* module, binary_reader_t* reader) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(module->functions != NULL);

    uint32_t functions_count = BINARY_READER_PULL_U32(reader);
    CHECK(functions_count == arrlen(module->functions));

    for (int64_t i = 0; i < functions_count; i++) {
        // create an empty functype
        // wasm_func_t* func = module->functions[i];

        uint32_t code_size = BINARY_READER_PULL_U32(reader);
        void* code = BINARY_READER_PULL(reader, code_size);

        // TODO: jit it from here
        (void)code;
    }

cleanup:
    return err;
}

WASM_API_EXTERN wasm_module_t* wasm_module_deserialize(wasm_store_t* store, const wasm_byte_vec_t* binary) {
    wasm_err_t err = WASM_NO_ERROR;
    wasm_module_t* module = NULL;
    spidir_module_handle_t spidir_module = NULL;

    binary_reader_t reader = {
        .ptr = binary->data,
        .size = binary->size,
    };

    // validate the header
    RETHROW(wasm_module_verify_header(&reader));

    // create the module object
    module = wasm_host_calloc(1, sizeof(*module));
    CHECK(module != NULL);

    // create the jit module
    spidir_module = spidir_module_create();
    CHECK(spidir_module != NULL);

    // and now we can parse the sections
    while (reader.size != 0) {
        wasm_section_id_t section_id = BINARY_READER_PULL_BYTE(&reader);
        uint32_t section_size = BINARY_READER_PULL_U32(&reader);
        void* section_data = BINARY_READER_PULL(&reader, section_size);

        // setup a reader for the section
        binary_reader_t section_reader = {
            .ptr = section_data,
            .size = section_size,
        };

        switch (section_id) {
            // ignore custom sections
            case WASM_CUSTOM_SECTION: break;

            // parse normal sections
            case WASM_TYPE_SECTION: RETHROW(wasm_module_parse_type_section(module, &section_reader)); break;
            case WASM_FUNCTION_SECTION: RETHROW(wasm_module_parse_function_section(spidir_module, module, &section_reader)); break;
            case WASM_MEMORY_SECTION: RETHROW(wasm_module_parse_memory_section(module, &section_reader)); break;
            case WASM_GLOBAL_SECTION: RETHROW(wasm_module_parse_global_section(module, &section_reader)); break;
            case WASM_EXPORT_SECTION: RETHROW(wasm_module_parse_export_section(module, &section_reader)); break;
            case WASM_CODE_SECTION: RETHROW(wasm_module_parse_code_section(module, &section_reader)); break;
            default: CHECK_FAIL("Unknown section id %d", section_id);
        }
    }

cleanup:
    if (IS_ERROR(err)) {
        wasm_module_delete(module);
        module = NULL;
    }

    if (spidir_module != NULL) spidir_module_destroy(spidir_module);

    return module;
}

WASM_API_EXTERN void wasm_module_delete(wasm_module_t* module) {
    if (module != NULL) {
        for (int i = 0; i < arrlen(module->functions); i++) {
            wasm_func_delete(module->functions[i]);
        }
        arrfree(module->functions);

        wasm_exporttype_vec_delete(&module->exporttypes);
        wasm_globaltype_vec_delete(&module->globaltypes);
        wasm_memorytype_vec_delete(&module->memorytypes);
        wasm_functype_vec_delete(&module->typefuncs);
        wasm_host_free(module);
    }
}


