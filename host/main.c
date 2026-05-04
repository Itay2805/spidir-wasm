#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <wasm/wasm.h>
#include <wasm/jit.h>

#include <util/except.h>
#include <spidir/log.h>

#include "wasm/error.h"
#include "wasm/host.h"

typedef enum option_type {
    // options with short version
    OPTION_HELP = 'h',
    OPTION_MODULE = 'm',
    OPTION_DEBUG = 'd',

    // options without short version
    OPTION_BASE = 0xFF,

    OPTION_LOG_LEVEL,
    OPTION_SPIDIR_DUMP,
} option_type_t;

static struct option long_options[] = {
    { "help", no_argument, 0, OPTION_HELP },
    { "module", required_argument, 0, OPTION_MODULE },
    { "debug", no_argument, 0, OPTION_DEBUG },
    { "log-level", required_argument, 0, OPTION_LOG_LEVEL },

    { "spidir-dump", optional_argument, 0, OPTION_SPIDIR_DUMP },

    { 0, 0, 0, 0 },
};

static const char* spidir_log_level_to_string(spidir_log_level_t level) {
    switch (level) {
        case SPIDIR_LOG_LEVEL_ERROR: return "ERROR";
        case SPIDIR_LOG_LEVEL_WARN: return "WARN";
        case SPIDIR_LOG_LEVEL_INFO: return "INFO";
        case SPIDIR_LOG_LEVEL_DEBUG: return "DEBUG";
        case SPIDIR_LOG_LEVEL_TRACE: return "TRACE";
        default: return "LOG";
    }
}

static void stdout_log_callback(spidir_log_level_t level, const char* module, size_t module_len, const char* message, size_t message_len) {
    printf("[%s %.*s] %.*s\n", spidir_log_level_to_string(level), (int) module_len, module, (int) message_len, message);
}

static spidir_dump_status_t spidir_dump_callback(const char* data, size_t size, void* ctx) {
    FILE* file = (FILE*)ctx;
    fprintf(file, "%.*s", (int)size, data);
    fflush(file);
    return SPIDIR_DUMP_CONTINUE;
}

static wasm_module_t m_module = {};
static wasm_module_jit_t m_module_jit = {};
static void* m_memory_base = nullptr;
static size_t m_memory_size = 0;

int main(int argc, char** argv) {
    wasm_err_t err = WASM_NO_ERROR;
    FILE* module_file = nullptr;
    char* module_binary = nullptr;
    void* state_base = nullptr;
    int status = EXIT_SUCCESS;

    char* module_path = nullptr;
    FILE* spidir_output_file = nullptr;

    // enable logging and set them to warn by default
    spidir_log_init(stdout_log_callback);
    spidir_log_set_max_level(SPIDIR_LOG_LEVEL_WARN);

    wasm_jit_config_t config = {
        .optimize = true,
    };

    bool used_help = false;
    while (1) {
        int option_index = 0;

        int c = getopt_long(argc, argv, "hm:d", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case OPTION_MODULE: {
                CHECK(module_path == nullptr, "Module already specified");
                module_path = strdup(optarg);
                CHECK(module_path != nullptr);
            } break;

            case OPTION_DEBUG: {
                // don't enable optimizations
                config.optimize = false;
            } break;

            case OPTION_SPIDIR_DUMP: {
                TRACE("%s", optarg);
                if (optarg == nullptr) {
                    config.dump_callback = spidir_dump_callback;
                    config.dump_arg = stdout;
                } else {
                    spidir_output_file = fopen(optarg, "wb");
                    CHECK(spidir_output_file != NULL);
                    config.dump_callback = spidir_dump_callback;
                    config.dump_arg = spidir_output_file;
                }
            } break;

            case OPTION_LOG_LEVEL: {
                // errno = 0;
                unsigned long level = strtoul(optarg, nullptr, 0);
                CHECK(errno == 0);

                // TODO: also set the wasm log level from here
                spidir_log_set_max_level(level);
            } break;

            case OPTION_HELP: {
                TRACE(" -h | --help                        print this help text");
                TRACE(" -m | --module <file>               the wasm module file to compile");
                TRACE(" -d | --debug                       don't perform jit optimizations");
                TRACE("      --log-level <level>           set the log level (0=none, 1=error, 2=warn, 3=info, debug=4, trace=5)");
                TRACE("      --spidir-dump <file>          dump the spidir output into a file (use `-` for stdout)");
                used_help = true;
            } break;

            default: {
                CHECK_FAIL();
            } break;
        }
    }

    if (used_help) {
        goto cleanup;
    }

    // check we have the module path
    CHECK(module_path != nullptr, "Missing module");

    // open the module file
    module_file = fopen(module_path, "rb");
    CHECK(module_file != nullptr, "%s: %s", strerror(errno), module_path);

    free(module_path);
    module_path = nullptr;

    // get the module size
    CHECK(fseek(module_file, 0, SEEK_END) == 0, "%s", strerror(errno));
    size_t module_size = ftell(module_file);
    CHECK(module_size, "%s", strerror(errno))
    CHECK(fseek(module_file, 0, SEEK_SET) == 0, "%s", strerror(errno));

    // read the file
    module_binary = wasm_host_calloc(1, module_size);
    CHECK(module_binary != nullptr);
    CHECK(fread(module_binary, module_size, 1, module_file) == 1, "%s", strerror(errno));

    fclose(module_file);
    module_file = nullptr;

    // TODO: proper frontend
    RETHROW(wasm_load_module(&m_module, module_binary, module_size));

    free(module_binary);
    module_binary = nullptr;

    // jit the module
    RETHROW(wasm_module_jit(&m_module, &m_module_jit, &config));

    // allocate the runtime state buffer (globals + tables) and seed it
    // with the JIT-built initializer so funcref tables come up populated
    if (m_module_jit.state_size != 0) {
        state_base = malloc(m_module_jit.state_size);
        CHECK(state_base != nullptr);
        if (m_module_jit.state_init != nullptr) {
            memcpy(state_base, m_module_jit.state_init, m_module_jit.state_size);
        } else {
            memset(state_base, 0, m_module_jit.state_size);
        }
    }

    // we reserve 8gb of address space as required
    m_memory_base = mmap(
        NULL,
        8ull * 1024ull * 1024ull * 1024ull,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
        -1,
        0
    );
    CHECK(m_memory_base != MAP_FAILED);

    // from that mapped the first x bytes (skip when the module declares no
    // memory — modules that never touch memory never read this region)
    if (m_module.memory_min != 0) {
        m_memory_size = m_module.memory_min;
        void* mapped = mmap(
            m_memory_base,
            m_module.memory_min,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );
        CHECK(mapped != MAP_FAILED);
        CHECK(mapped == m_memory_base);
    }

    // get the entry point and run it
    int64_t index = wasm_find_export(&m_module, "_start");
    CHECK(index >= 0);
    int (*entry)(void* memory, void* state) = m_module_jit.exports[index].func.address;
    status = entry(m_memory_base, state_base);

cleanup:
    wasm_module_jit_free(&m_module_jit);
    wasm_module_free(&m_module);
    free(module_path);

    free(state_base);
    if (m_memory_base != nullptr && m_memory_base != MAP_FAILED) {
        munmap(m_memory_base, 8ull * 1024ull * 1024ull * 1024ull);
    }

    if (spidir_output_file != NULL) {
        fclose(spidir_output_file);
    }

    if (module_file != NULL) {
        fclose(module_file);
    }

    return IS_ERROR(err) ? EXIT_FAILURE : status;
}

void wasm_host_log(wasm_host_log_level_t log_level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch (log_level) {
        case WASM_HOST_LOG_RAW: break;
        case WASM_HOST_LOG_DEBUG: printf("[?] "); break;
        case WASM_HOST_LOG_TRACE: printf("[*] "); break;
        case WASM_HOST_LOG_WARN: printf("[!] "); break;
        case WASM_HOST_LOG_ERROR: printf("[-] "); break;
    }
    vprintf(fmt, args);
    if (log_level != WASM_HOST_LOG_RAW) {
        printf("\n");
    }
    va_end(args);
}

void wasm_host_snprintf(char* buffer, size_t len, const char* fmt, ...) {
    va_list args = {};
    va_start(args, fmt);
    vsnprintf(buffer, len, fmt, args);
    va_end(args);
}

int32_t wasm_host_memory_size(void* memory_base) {
    return m_memory_size / WASM_PAGE_SIZE;
}

int32_t wasm_host_memory_grow(void* memory_base, int32_t new_page_count) {
    if (new_page_count < 0) return -1;

    // Spec: refuse if growth would exceed the declared max. The size
    // increase happens only along the way to a successful mmap, so the
    // -1 return path leaves m_memory_size untouched.
    size_t old_count = m_memory_size / WASM_PAGE_SIZE;
    size_t new_count = old_count + (size_t)new_page_count;
    if (new_count * WASM_PAGE_SIZE > m_module.memory_max) return -1;

    // Map only the newly-added range. The original pages are already
    // mapped (since startup); using MAP_FIXED over them would zap any
    // data the program has stored there.
    if (new_page_count != 0) {
        void* added = mmap(
            (char*)memory_base + old_count * WASM_PAGE_SIZE,
            (size_t)new_page_count * WASM_PAGE_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );
        if (added == MAP_FAILED) return -1;
    }

    m_memory_size = new_count * WASM_PAGE_SIZE;
    return (int32_t)old_count;
}

void* wasm_host_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void* wasm_host_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void wasm_host_free(void* ptr) {
    free(ptr);
}

size_t wasm_host_page_size(void) {
    return 4096;
}

void* wasm_host_jit_alloc(size_t rx_page_count, size_t ro_page_count) {
    void* ptr = mmap(nullptr, (rx_page_count + ro_page_count) * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;
    return ptr;
}

bool wasm_host_jit_lock(void* ptr, size_t rx_page_count, size_t ro_page_count) {
    size_t rx_size = rx_page_count * 4096;
    size_t ro_size = ro_page_count * 4096;

    if (mprotect(ptr, rx_size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect(PROT_READ | PROT_EXEC)");
        return false;
    }

    if (mprotect(ptr + rx_size, ro_size, PROT_READ) != 0) {
        perror("mprotect(PROT_READ)");
        return false;
    }

    return true;
}

void wasm_host_jit_free(void* ptr, size_t rx_page_count, size_t ro_page_count) {
    munmap(ptr, (rx_page_count + ro_page_count) * 4096);
}
