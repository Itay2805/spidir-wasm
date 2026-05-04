#include "wasm/wasm.h"

#include "buffer.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"
#include "wasm/host.h"

typedef enum wasm_section_id : uint8_t {
    WASM_SECTION_CUSTOM = 0,
    WASM_SECTION_TYPE = 1,
    WASM_SECTION_IMPORT = 2,
    WASM_SECTION_FUNCTION = 3,
    WASM_SECTION_TABLE = 4,
    WASM_SECTION_MEMORY = 5,
    WASM_SECTION_GLOBAL = 6,
    WASM_SECTION_EXPORT = 7,
    WASM_SECTION_START = 8,
    WASM_SECTION_ELEMENT = 9,
    WASM_SECTION_CODE = 10,
    WASM_SECTION_DATA = 11,
    WASM_SECTION_DATA_COUNT = 12,
    WASM_SECTION_TAG = 13,
} wasm_section_id_t;

typedef struct wasm_section {
    wasm_section_id_t id;
    buffer_t contents;
} wasm_section_t;

static void wasm_type_free(wasm_type_t* type) {
    wasm_host_free(type->arg_types);
    wasm_host_free(type->result_types);
}

void wasm_module_free(wasm_module_t* module) {
    for (int i = 0; i < module->types_count; i++) {
        wasm_type_free(&module->types[i]);
    }

    for (int i = 0; i < module->imports_count; i++) {
        wasm_host_free(module->imports[i].item_name);
        wasm_host_free(module->imports[i].module_name);
    }

    for (int i = 0; i < module->functions_count; i++) {
        wasm_host_free(module->code[i].code);
    }

    for (int i = 0; i < module->exports_count; i++) {
        wasm_host_free(module->exports[i].name);
    }

    for (int i = 0; i < module->elems_count; i++) {
        wasm_host_free(module->elems[i].funcs);
    }

    for (int i = 0; i < module->data_segments_count; i++) {
        wasm_host_free(module->data_segments[i].data);
    }


    wasm_host_free(module->data_segments);
    wasm_host_free(module->types);
    wasm_host_free(module->imports);
    wasm_host_free(module->functions);
    wasm_host_free(module->globals);
    wasm_host_free(module->exports);
    wasm_host_free(module->tables);
    wasm_host_free(module->elems);
    wasm_host_free(module->code);
}

static wasm_err_t module_pull_magic_version(buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    char* magic_version = buffer_pull(buffer, 8);
    CHECK(magic_version != nullptr);
    CHECK(magic_version[0] == 0x00);
    CHECK(magic_version[1] == 0x61);
    CHECK(magic_version[2] == 0x73);
    CHECK(magic_version[3] == 0x6D);
    CHECK(magic_version[4] == 0x01);
    CHECK(magic_version[5] == 0x00);
    CHECK(magic_version[6] == 0x00);
    CHECK(magic_version[7] == 0x00);

cleanup:
    return err;
}

static wasm_err_t wasm_pull_section(buffer_t* buffer, wasm_section_t* section) {
    wasm_err_t err = WASM_NO_ERROR;

    section->id = BUFFER_PULL(wasm_section_id_t, buffer);
    section->contents.len = BUFFER_PULL_U32(buffer);
    section->contents.data = buffer_pull(buffer, section->contents.len);
    CHECK(section->contents.data != nullptr);

cleanup:
    return err;
}

static const wasm_section_id_t m_expected_section_order[] = {
    WASM_SECTION_TYPE,
    WASM_SECTION_IMPORT,
    WASM_SECTION_FUNCTION,
    WASM_SECTION_TABLE,
    WASM_SECTION_MEMORY,
    WASM_SECTION_TAG,
    WASM_SECTION_GLOBAL,
    WASM_SECTION_EXPORT,
    WASM_SECTION_START,
    WASM_SECTION_ELEMENT,
    WASM_SECTION_DATA_COUNT,
    WASM_SECTION_CODE,
    WASM_SECTION_DATA
};

static int find_section_index(wasm_section_id_t id, int index) {
    for (int i = index; i < ARRAY_LENGTH(m_expected_section_order); i++) {
        if (m_expected_section_order[i] == id) {
            return i;
        }
    }
    return -1;
}

static wasm_err_t wasm_pull_result_type(buffer_t* buffer, wasm_value_type_t** out_types, uint32_t* out_count) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    wasm_value_type_t* types = CALLOC(wasm_value_type_t, count);
    CHECK(types != nullptr);
    *out_count = count;

    for (int i = 0; i < count; i++) {
        wasm_value_type_t type;
        RETHROW(buffer_pull_val_type(buffer, &type));
        types[i] = type;
    }

    *out_types = types;

cleanup:
    if (IS_ERROR(err)) {
        wasm_host_free(types);
    }

    return err;
}

static wasm_err_t wasm_parse_type_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t type_count = BUFFER_PULL_U32(buffer);
    module->types = CALLOC(wasm_type_t, type_count);
    CHECK(module->types != nullptr);
    module->types_count = type_count;

    for (int i = 0; i < type_count; i++) {
        uint8_t type = BUFFER_PULL(uint8_t, buffer);
        wasm_type_t wasm_type = {};

        switch (type) {
            case 0x60: {
                RETHROW(wasm_pull_result_type(buffer, &wasm_type.arg_types, &wasm_type.arg_types_count));
                RETHROW(wasm_pull_result_type(buffer, &wasm_type.result_types, &wasm_type.result_types_count));
            } break;

            default: {
                CHECK_FAIL("Unknown type %x", type);
            } break;
        }

        module->types[i] = wasm_type;
    }

    CHECK(buffer->len == 0);

cleanup:
    return err;
}
static wasm_err_t wasm_parse_import_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;
    char* module_name = nullptr;
    char* item_name = nullptr;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->imports = CALLOC(wasm_import_t, count);
    CHECK(module->imports != nullptr);
    module->imports_count = count;

    for (int i = 0; i < count; i++) {
        // pull the module name
        buffer_t module_name_buf = {};
        RETHROW(buffer_pull_name(buffer, &module_name_buf));
        CHECK(module_name_buf.len > 0);
        module_name = wasm_host_calloc(1, module_name_buf.len + 1);
        memcpy(module_name, module_name_buf.data, module_name_buf.len);
        module_name[module_name_buf.len] = '\0';

        // pull the item name
        buffer_t item_name_buf = {};
        RETHROW(buffer_pull_name(buffer, &item_name_buf));
        CHECK(item_name_buf.len > 0);
        item_name = wasm_host_calloc(1, item_name_buf.len + 1);
        memcpy(item_name, item_name_buf.data, item_name_buf.len);
        item_name[item_name_buf.len] = '\0';

        // get the type and index
        uint8_t byte = BUFFER_PULL(uint8_t, buffer);

        // get the index
        wasm_extern_type_t kind;
        uint32_t index = BUFFER_PULL_U32(buffer);
        switch (byte) {
            case 0x00: {
                CHECK(index < module->types_count);
                kind = WASM_EXTERN_FUNC;
            } break;

            default: {
                CHECK_FAIL("Unknown export type %x (%s, %s)", byte, module_name, item_name);
            } break;
        }

        // and now insert it into the hashmap
        module->imports[i] = (wasm_import_t){
            .item_name = item_name,
            .module_name = module_name,
            .kind = kind,
            .index = index,
        };

        module_name = nullptr;
        item_name = nullptr;
    }

    CHECK(buffer->len == 0);

cleanup:
    wasm_host_free(module_name);
    wasm_host_free(item_name);

    return err;
}

static wasm_err_t wasm_parse_function_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->functions = CALLOC(typeidx_t, count);
    CHECK(module->functions != nullptr);
    module->functions_count = count;

    for (int i = 0; i < count; i++) {
        uint32_t typeidx = BUFFER_PULL_U32(buffer);
        CHECK(typeidx < module->types_count);
        module->functions[i] = typeidx;
    }

    CHECK(buffer->len == 0);

cleanup:
    return err;
}

static wasm_err_t wasm_parse_table_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->tables = CALLOC(wasm_table_t, count);
    CHECK(module->tables != nullptr);
    module->tables_count = count;

    for (int i = 0; i < count; i++) {
        // MVP-only reftype: funcref (0x70). Externref (0x6F) and any
        // other reference types are out of scope.
        uint8_t reftype = BUFFER_PULL(uint8_t, buffer);
        CHECK(reftype == 0x70, "Unsupported reftype %02x", reftype);

        uint8_t limit_type = BUFFER_PULL(uint8_t, buffer);
        if (limit_type == 0x00) {
            module->tables[i].min = BUFFER_PULL_U32(buffer);
            module->tables[i].max = UINT32_MAX;
        } else if (limit_type == 0x01) {
            module->tables[i].min = BUFFER_PULL_U32(buffer);
            module->tables[i].max = BUFFER_PULL_U32(buffer);
        } else {
            CHECK_FAIL("Invalid table limit %02x", limit_type);
        }

        CHECK(module->tables[i].min <= module->tables[i].max);
    }

    CHECK(buffer->len == 0);

cleanup:
    return err;
}

static wasm_err_t wasm_parse_constant_expr(buffer_t* buffer, wasm_value_t* value) {
    wasm_err_t err = WASM_NO_ERROR;

    // get the expression
    uint8_t byte = BUFFER_PULL(uint8_t, buffer);
    switch (byte) {
        case 0x41: {
            value->kind = WASM_VALUE_TYPE_I32;
            value->value.i32 = BUFFER_PULL_I32(buffer);
        } break;

        case 0x42: {
            value->kind = WASM_VALUE_TYPE_I64;
            value->value.i64 = BUFFER_PULL_I64(buffer);
        } break;

        case 0x43: {
            value->kind = WASM_VALUE_TYPE_F32;
            value->value.f32 = BUFFER_PULL(float, buffer);
        } break;

        case 0x44: {
            value->kind = WASM_VALUE_TYPE_F64;
            value->value.f64 = BUFFER_PULL(double, buffer);
        } break;

        default:
            CHECK_FAIL("%x", byte);
    }

    // ensure we end up with expression end
    CHECK(BUFFER_PULL(uint8_t, buffer) == 0x0B);

cleanup:
    return err;
}

wasm_err_t wasm_parse_global_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->globals = CALLOC(wasm_global_t, count);
    CHECK(module->globals != nullptr);
    module->globals_count = count;

    for (int i = 0; i < count; i++) {
        wasm_value_type_t type;
        RETHROW(buffer_pull_val_type(buffer, &type));
        uint8_t mut = BUFFER_PULL(uint8_t, buffer);
        CHECK(mut == 0x00 || mut == 0x01);

        wasm_global_t global = {
            .mutable = mut == 0x01
        };

        // parse the expression and ensure we get the correct
        // type at the end of it
        RETHROW(wasm_parse_constant_expr(buffer, &global.value));
        CHECK(global.value.kind == type);

        // append it
        module->globals[i] = global;
    }

    CHECK(buffer->len == 0);

cleanup:
    return err;
}

static wasm_err_t wasm_parse_memory_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    CHECK(count == 1, "Multi-memory is not supported");

    // TODO: multi-memory support
    // TODO: support for 64bit addresses

    uint8_t limit_type = BUFFER_PULL(uint8_t, buffer);
    if (limit_type == 0x00) {
        module->memory_min = BUFFER_PULL_U64(buffer);
        module->memory_max = (UINT32_MAX / WASM_PAGE_SIZE) - 1;
    } else if (limit_type == 0x01) {
        module->memory_min = BUFFER_PULL_U64(buffer);
        module->memory_max = BUFFER_PULL_U64(buffer);
    } else {
        CHECK_FAIL("Invalid limit %02x", limit_type);
    }

    // ensure the min is less or equals to the max
    CHECK(module->memory_min <= module->memory_max);

    // turn into bytes instead of pages
    CHECK(!__builtin_mul_overflow(module->memory_min, WASM_PAGE_SIZE, &module->memory_min));
    CHECK(!__builtin_mul_overflow(module->memory_max, WASM_PAGE_SIZE, &module->memory_max));

    // ensure both stay within 4GB
    CHECK(module->memory_min < SIZE_4GB);
    CHECK(module->memory_max < SIZE_4GB);

    CHECK(buffer->len == 0);

cleanup:
    return err;
}

// Parses an active funcref element segment of the form supported by MVP:
// kind 0 = (i32.const offset) vec(funcidx). Other kinds (passive, decla-
// rative, expr-vector, externref) are intentionally rejected so unknown
// shapes produce a clear error rather than a silent miscompile.
static wasm_err_t wasm_parse_element_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;
    uint32_t* funcs = nullptr;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->elems = CALLOC(wasm_elem_segment_t, count);
    CHECK(module->elems != nullptr);
    module->elems_count = count;

    for (int i = 0; i < count; i++) {
        uint32_t kind = BUFFER_PULL_U32(buffer);
        CHECK(kind == 0, "Unsupported elem segment kind %u", kind);

        // active segment, default tableidx 0, offset is a constant expr
        wasm_value_t offset_expr = {};
        RETHROW(wasm_parse_constant_expr(buffer, &offset_expr));
        CHECK(offset_expr.kind == WASM_VALUE_TYPE_I32);

        uint32_t funcs_count = BUFFER_PULL_U32(buffer);
        funcs = CALLOC(uint32_t, funcs_count);
        CHECK(funcs != nullptr);

        for (int j = 0; j < funcs_count; j++) {
            uint32_t fidx = BUFFER_PULL_U32(buffer);
            CHECK(fidx < module->functions_count + module->imports_count);
            funcs[j] = fidx;
        }

        module->elems[i] = (wasm_elem_segment_t){
            .tableidx = 0,
            .offset = (uint32_t)offset_expr.value.i32,
            .funcs_count = funcs_count,
            .funcs = funcs,
        };
        funcs = nullptr;
    }

    CHECK(buffer->len == 0);

cleanup:
    wasm_host_free(funcs);
    return err;
}

// Parses a kind-0 active data segment of the form supported by MVP:
// (i32.const offset) vec(byte). Other kinds (passive, memidx-explicit)
// are intentionally rejected so unknown shapes produce a clear error
// rather than a silent miscompile. The bytes are copied into a freshly
// allocated buffer owned by the module so callers don't need to keep
// the original input alive.
static wasm_err_t wasm_parse_data_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;
    void* data = nullptr;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->data_segments = CALLOC(wasm_data_segment_t, count);
    CHECK(module->data_segments != nullptr);
    module->data_segments_count = count;

    for (int i = 0; i < count; i++) {
        uint32_t kind = BUFFER_PULL_U32(buffer);
        CHECK(kind == 0, "Unsupported data segment kind %u", kind);

        // active segment, default memidx 0, offset is a constant expr
        wasm_value_t offset_expr = {};
        RETHROW(wasm_parse_constant_expr(buffer, &offset_expr));
        CHECK(offset_expr.kind == WASM_VALUE_TYPE_I32);

        uint32_t len = BUFFER_PULL_U32(buffer);
        void* src = buffer_pull(buffer, len);
        CHECK(src != nullptr);

        data = wasm_host_calloc(1, len);
        CHECK(len == 0 || data != nullptr);
        memcpy(data, src, len);

        module->data_segments[i] = (wasm_data_segment_t){
            .offset = (uint32_t)offset_expr.value.i32,
            .len = len,
            .data = data,
        };
        data = nullptr;
    }

    CHECK(buffer->len == 0);

cleanup:
    wasm_host_free(data);
    return err;
}

static wasm_err_t wasm_parse_export_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;
    char* name = nullptr;

    uint32_t count = BUFFER_PULL_U32(buffer);
    module->exports = CALLOC(wasm_export_t, count);
    CHECK(module->exports != nullptr);
    module->exports_count = count;

    for (int i = 0; i < count; i++) {
        // create a null terminated copy of the name
        buffer_t name_buf = {};
        RETHROW(buffer_pull_name(buffer, &name_buf));
        CHECK(name_buf.len > 0);
        name = wasm_host_calloc(1, name_buf.len + 1);
        memcpy(name, name_buf.data, name_buf.len);
        name[name_buf.len] = '\0';

        // get the type and index
        uint8_t byte = BUFFER_PULL(uint8_t, buffer);

        // get the index
        wasm_export_type_t kind;
        uint32_t index = BUFFER_PULL_U32(buffer);
        switch (byte) {
            case 0x00: CHECK(index < module->functions_count); kind = WASM_EXPORT_FUNC; break;
            case 0x02: CHECK(index == 0); kind = WASM_EXPORT_MEMORY; break;
            case 0x03: CHECK(index < module->globals_count); kind = WASM_EXPORT_GLOBAL; break;
            default: CHECK_FAIL("Unknown export type %x (%s)", byte, name);
        }

        // and now insert it into the hashmap
        module->exports[i] = (wasm_export_t){
            .kind = kind,
            .index = index,
            .name = name
        };
        name = nullptr;
    }

    CHECK(buffer->len == 0);

cleanup:
    wasm_host_free(name);

    return err;
}

static wasm_err_t wasm_parse_code_section(wasm_module_t* module, buffer_t* buffer) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t count = BUFFER_PULL_U32(buffer);
    CHECK(count == module->functions_count);
    module->code = CALLOC(wasm_code_t, count);
    CHECK(module->code != nullptr);

    for (int i = 0; i < count; i++) {
        wasm_code_t* code = &module->code[i];

        code->length = BUFFER_PULL_U32(buffer);
        void* data = buffer_pull(buffer, code->length);
        CHECK(data != nullptr);

        code->code = wasm_host_calloc(1, code->length);
        memcpy(code->code, data, code->length);
    }

    CHECK(buffer->len == 0);

cleanup:
    return err;
}

wasm_err_t wasm_load_module(wasm_module_t* module, void* data, size_t size) {
    wasm_err_t err = WASM_NO_ERROR;

    buffer_t buffer = init_buffer(data, size);
    RETHROW(module_pull_magic_version(&buffer));

    int current_index = 0;
    while (buffer.len != 0) {
        wasm_section_t section = {};
        RETHROW(wasm_pull_section(&buffer, &section));

        // check ordering
        if (section.id != 0) {
            int index = find_section_index(section.id, current_index);
            CHECK(index >= current_index);
            current_index = index + 1;
        }

        // actually check the section
        buffer_t* contents = &section.contents;
        switch (section.id) {
            case WASM_SECTION_CUSTOM: {
                buffer_t name = {};
                RETHROW(buffer_pull_name(contents, &name));

                // just ignore the custom sections for now
            } break;

            case WASM_SECTION_TYPE: RETHROW(wasm_parse_type_section(module, contents)); break;
            case WASM_SECTION_IMPORT: RETHROW(wasm_parse_import_section(module, contents)); break;
            case WASM_SECTION_FUNCTION: RETHROW(wasm_parse_function_section(module, contents)); break;
            case WASM_SECTION_TABLE: RETHROW(wasm_parse_table_section(module, contents)); break;
            case WASM_SECTION_MEMORY: RETHROW(wasm_parse_memory_section(module, contents)); break;
            case WASM_SECTION_GLOBAL: RETHROW(wasm_parse_global_section(module, contents)); break;
            case WASM_SECTION_EXPORT: RETHROW(wasm_parse_export_section(module, contents)); break;
            case WASM_SECTION_ELEMENT: RETHROW(wasm_parse_element_section(module, contents)); break;
            case WASM_SECTION_CODE: RETHROW(wasm_parse_code_section(module, contents)); break;
            case WASM_SECTION_DATA: RETHROW(wasm_parse_data_section(module, contents)); break;

            default: {
                CHECK_FAIL("wasm: ignoring section %d", section.id);
            } break;
        }
    }

cleanup:
    if (IS_ERROR(err)) {
        wasm_module_free(module);
    }

    return err;
}

int64_t wasm_find_export(wasm_module_t* module, const char* name) {
    for (int64_t i = 0; i < module->exports_count; i++) {
        if (strcmp(module->exports[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
