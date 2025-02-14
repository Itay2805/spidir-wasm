#pragma once

#include <wasm/host.h>
#include <wasm/error.h>

#include "cpp_magic.h"

#define DEBUG(fmt, ...) wasm_host_log(WASM_HOST_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define TRACE(fmt, ...) wasm_host_log(WASM_HOST_LOG_TRACE, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...)  wasm_host_log(WASM_HOST_LOG_WARN, fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) wasm_host_log(WASM_HOST_LOG_ERROR, fmt, ##__VA_ARGS__)

#define IS_ERROR(err) ((err) != WASM_NO_ERROR)

#define CHECK_ERROR(expr, error, ...) \
    do { \
        if (!(expr)) { \
            err = error; \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__)); \
            ERROR("Check failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            goto cleanup; \
        } \
    } while(0);

#define CHECK(expr, ...) CHECK_ERROR(expr, WASM_ERROR_CHECK_FAILED, ##__VA_ARGS__)

#define RETHROW(expr) \
    do { \
        err = expr; \
        if (IS_ERROR(err)) { \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            goto cleanup; \
        } \
    } while (0)

#define CHECK_FAIL(...) CHECK(0, ##__VA_ARGS__)
