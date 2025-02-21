#include "binary_reader.h"

void* binary_reader_pull(binary_reader_t* reader, size_t size) {
    if (reader->size < size) {
        return NULL;
    }

    void* ptr = reader->ptr;
    reader->ptr += size;
    reader->size -= size;
    return ptr;
}

wasm_err_t binary_reader_pull_u64(binary_reader_t* reader, uint64_t* out) {
    wasm_err_t err = WASM_NO_ERROR;
    uint64_t result = 0;
    uint32_t shift = 0;

    while (true) {
        uint8_t byte = BINARY_READER_PULL_BYTE(reader);
        result |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }

    *out = result;

cleanup:
    return err;
}

wasm_err_t binary_reader_pull_i64(binary_reader_t* reader, int64_t* out) {
    wasm_err_t err = WASM_NO_ERROR;
    int64_t result = 0;
    uint32_t shift = 0;

    uint8_t byte;
    do {
        byte = BINARY_READER_PULL_BYTE(reader);
        result |= ((byte & 0x7F) << shift);
        shift += 7;
    } while ((byte & 0x80) != 0);

    if ((shift < 64) && (byte & 0x40)) {
        result |= (~0u << shift);
    }

    *out = result;

cleanup:
    return err;
}
