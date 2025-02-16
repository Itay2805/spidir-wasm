#pragma once

#include <stddef.h>
#include <stdint.h>
#include <spidir/module.h>

typedef enum wasm_host_log_level {
    WASM_HOST_LOG_RAW,
    WASM_HOST_LOG_DEBUG,
    WASM_HOST_LOG_TRACE,
    WASM_HOST_LOG_WARN,
    WASM_HOST_LOG_ERROR,
} wasm_host_log_level_t;

/**
 * Performs a log on the host, \n is not included
 */
void wasm_host_log(wasm_host_log_level_t log_level, const char* fmt, ...);

void* wasm_host_calloc(size_t nmemb, size_t size);
void* wasm_host_realloc(void* ptr, size_t new_size);
void wasm_host_free(void* ptr);
