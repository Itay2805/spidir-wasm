
#include <stdalign.h>
#include <stddef.h>
#include <util/except.h>

void* spidir_platform_alloc(size_t size, size_t align) {
    return wasm_host_calloc(size, align);
}

void spidir_platform_free(void* ptr, size_t size, size_t align) {
    (void) size;
    (void) align;
    wasm_host_free(ptr);
}

void* spidir_platform_realloc(void* ptr, size_t old_size, size_t align, size_t new_size) {
    return wasm_host_realloc(ptr, new_size);
}

void spidir_platform_panic(const char* message, size_t message_len) {
    ERROR("spidir: %.*s", message_len, message);
    __builtin_trap();
}
