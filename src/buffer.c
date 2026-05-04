#include "buffer.h"

#include "util/except.h"
#include "util/string.h"
#include "wasm/error.h"
#include "wasm/host.h"

wasm_err_t buffer_push(buffer_t* buffer, const void* data, size_t len) {
    wasm_err_t err = WASM_NO_ERROR;

    void* new_data = wasm_host_realloc(buffer->data, buffer->len + len);
    CHECK(new_data != nullptr);
    buffer->data = new_data;
    memcpy(new_data + buffer->len, data, len);
    buffer->len += len;

cleanup:
    return err;
}

wasm_err_t buffer_fill(buffer_t* buffer, uint8_t value, size_t len) {
    wasm_err_t err = WASM_NO_ERROR;

    void* new_data = wasm_host_realloc(buffer->data, buffer->len + len);
    CHECK(new_data != nullptr);
    buffer->data = new_data;
    memset(new_data + buffer->len, value, len);
    buffer->len += len;

cleanup:
    return err;
}

wasm_err_t buffer_align(buffer_t* buffer, uint8_t value, size_t alignment) {
    wasm_err_t err = WASM_NO_ERROR;

    size_t diff = buffer->len % alignment;
    if (diff != 0) {
        size_t len = alignment - diff;
        RETHROW(buffer_fill(buffer, value, len));
    }

cleanup:
    return err;
}

wasm_err_t buffer_pull_u32(buffer_t* buffer, uint32_t* value) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;
    do {
        CHECK(shift <= 28);
        byte = BUFFER_PULL(uint8_t, buffer);
        result |= (uint32_t)(byte & 0x7f) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);

    *value = result;

cleanup:
    return err;
}

wasm_err_t buffer_pull_u64(buffer_t* buffer, uint64_t* value) {
    wasm_err_t err = WASM_NO_ERROR;

    uint64_t result = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;
    do {
        CHECK(shift <= 63);
        byte = BUFFER_PULL(uint8_t, buffer);
        result |= (uint64_t)(byte & 0x7f) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);

    *value = result;

cleanup:
    return err;
}

wasm_err_t buffer_pull_i32(buffer_t* buffer, int32_t* out) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;
    do {
        CHECK(shift <= 28);
        byte = BUFFER_PULL(uint8_t, buffer);
        result |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);

    // sign-extend if the encoding's sign bit (0x40 of the final byte) is set
    // and we haven't already filled the full 32 bits
    if ((shift < 32) && (byte & 0x40)) {
        result |= ~(uint32_t)0 << shift;
    }

    *out = (int32_t)result;

cleanup:
    return err;
}

wasm_err_t buffer_pull_i64(buffer_t* buffer, int64_t* out) {
    wasm_err_t err = WASM_NO_ERROR;

    uint64_t result = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;
    do {
        CHECK(shift <= 63);
        byte = BUFFER_PULL(uint8_t, buffer);
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while ((byte & 0x80) != 0);

    if ((shift < 64) && (byte & 0x40)) {
        result |= ~(uint64_t)0 << shift;
    }

    *out = (int64_t)result;

cleanup:
    return err;
}

wasm_err_t buffer_pull_name(buffer_t* buffer, buffer_t* name) {
    wasm_err_t err = WASM_NO_ERROR;

    name->len = BUFFER_PULL_U32(buffer);
    name->data = buffer_pull(buffer, name->len);
    CHECK(name->data != nullptr);

    // TODO: verify utf8

cleanup:
    return err;
}

wasm_err_t buffer_pull_val_type(buffer_t* buffer, wasm_value_type_t* valtype) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t byte = BUFFER_PULL(uint8_t, buffer);
    switch (byte) {
        case 0x7C: *valtype = WASM_VALUE_TYPE_F64; break;
        case 0x7D: *valtype = WASM_VALUE_TYPE_F32; break;
        case 0x7E: *valtype = WASM_VALUE_TYPE_I64; break;
        case 0x7F: *valtype = WASM_VALUE_TYPE_I32; break;
        default: CHECK_FAIL("%x", byte);
    }

cleanup:
    return err;
}
