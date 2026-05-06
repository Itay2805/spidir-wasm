#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <wasm/wasm.h>
#include <wasm/jit.h>
#include <wasm/debug_elf.h>

#include <util/except.h>
#include <spidir/log.h>

#include "wasm/error.h"
#include "wasm/host.h"

// --- GDB JIT interface ----------------------------------------------------
// See https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html. GDB sets
// a software breakpoint on __jit_debug_register_code; whenever the JIT
// publishes a new ELF it calls that function and GDB walks
// __jit_debug_descriptor.first_entry to pick up the change. The interface is
// passive — when no debugger is attached the call is just a function entry.
typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN = 1,
    JIT_UNREGISTER_FN = 2,
} jit_actions_t;

struct jit_code_entry {
    struct jit_code_entry* next_entry;
    struct jit_code_entry* prev_entry;
    const char* symfile_addr;
    uint64_t symfile_size;
};

struct jit_descriptor {
    uint32_t version;
    uint32_t action_flag;
    struct jit_code_entry* relevant_entry;
    struct jit_code_entry* first_entry;
};

// GDB documents this as the standard symbol it watches; the no-inline / used
// attributes keep the breakpoint from being optimized out. This is the
// canonical pattern from the GDB manual.
__attribute__((noinline, used))
void __jit_debug_register_code(void) {
    __asm__ __volatile__("");
}

struct jit_descriptor __jit_debug_descriptor = { 1, 0, nullptr, nullptr };

typedef enum option_type {
    // options with short version
    OPTION_HELP = 'h',
    OPTION_MODULE = 'm',
    OPTION_DEBUG = 'd',

    // options without short version
    OPTION_BASE = 0xFF,

    OPTION_LOG_LEVEL,
    OPTION_SPIDIR_DUMP,
    OPTION_EMIT_DEBUG_ELF,
    OPTION_GDB_JIT,
} option_type_t;

static struct option long_options[] = {
    { "help", no_argument, 0, OPTION_HELP },
    { "module", required_argument, 0, OPTION_MODULE },
    { "debug", no_argument, 0, OPTION_DEBUG },
    { "log-level", required_argument, 0, OPTION_LOG_LEVEL },

    { "spidir-dump", optional_argument, 0, OPTION_SPIDIR_DUMP },

    // Dump a debug ELF that mirrors the JIT'd binary (see
    // wasm_jit_emit_debug_elf). Useful with `objdump -d -r` or `readelf -a`.
    { "emit-debug-elf", required_argument, 0, OPTION_EMIT_DEBUG_ELF },

    // Publish the same debug ELF to GDB through the JIT interface so a
    // backtrace inside generated code resolves to wasm-level function names.
    { "gdb-jit", no_argument, 0, OPTION_GDB_JIT },

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

// Test-only host imports under module name "env". Used by tests/cases/imports.wat
// to verify the JIT's import-call path. Real runtimes would link against WASI
// or a richer import surface; this stays minimal so the test is self-contained.
//
// Imported wasm functions are codegen'd with the same hidden (memory, state)
// prefix as internal functions, so the host signatures must include them too.
static int32_t host_env_add_i32(void* memory, void* state, int32_t a, int32_t b) {
    (void)memory; (void)state;
    return a + b;
}

static int64_t host_env_mul_i64(void* memory, void* state, int64_t a, int64_t b) {
    (void)memory; (void)state;
    return a * b;
}

static int32_t host_env_magic(void* memory, void* state) {
    (void)memory; (void)state;
    return (int32_t)0xDEADBEEF;
}

static void* resolve_import(void* arg, const char* module, const char* name, wasm_type_t* type) {
    (void)arg; (void)type;
    if (strcmp(module, "env") == 0) {
        if (strcmp(name, "add_i32") == 0) return host_env_add_i32;
        if (strcmp(name, "mul_i64") == 0) return host_env_mul_i64;
        if (strcmp(name, "magic") == 0)   return host_env_magic;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    wasm_err_t err = WASM_NO_ERROR;
    FILE* module_file = nullptr;
    char* module_binary = nullptr;
    void* state_base = nullptr;
    int status = EXIT_SUCCESS;

    char* module_path = nullptr;
    FILE* spidir_output_file = nullptr;
    char* debug_elf_path = nullptr;
    bool gdb_jit = false;
    void* debug_elf_data = nullptr;
    size_t debug_elf_size = 0;
    struct jit_code_entry* gdb_jit_entry = nullptr;

    // enable logging and set them to warn by default
    spidir_log_init(stdout_log_callback);
    spidir_log_set_max_level(SPIDIR_LOG_LEVEL_WARN);

    wasm_jit_config_t config = {
        .optimize = true,
        .resolve_import = resolve_import,
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

            case OPTION_EMIT_DEBUG_ELF: {
                CHECK(debug_elf_path == nullptr, "Debug ELF path already specified");
                debug_elf_path = strdup(optarg);
                CHECK(debug_elf_path != nullptr);
            } break;

            case OPTION_GDB_JIT: {
                gdb_jit = true;
            } break;

            case OPTION_HELP: {
                TRACE(" -h | --help                        print this help text");
                TRACE(" -m | --module <file>               the wasm module file to compile");
                TRACE(" -d | --debug                       don't perform jit optimizations");
                TRACE("      --log-level <level>           set the log level (0=none, 1=error, 2=warn, 3=info, debug=4, trace=5)");
                TRACE("      --spidir-dump <file>          dump the spidir output into a file (use `-` for stdout)");
                TRACE("      --emit-debug-elf <file>       write a debug ELF reflecting the JIT'd binary");
                TRACE("      --gdb-jit                     register the debug ELF with GDB via the JIT interface");
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

    // The debug ELF / GDB JIT paths both need wasm_module_jit to record
    // per-function layout and resolved relocations into jit->debug. The flag
    // stays off otherwise so a normal run pays nothing for the bookkeeping.
    if (debug_elf_path != nullptr || gdb_jit) {
        config.emit_debug_info = true;
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

    // Emit the debug ELF up front so it reflects the live JIT image (the
    // bytes don't change after this point). We share one buffer between the
    // file dump and the GDB JIT registration since they want identical
    // contents — the buffer is freed in cleanup.
    if (debug_elf_path != nullptr || gdb_jit) {
        RETHROW(wasm_jit_emit_debug_elf(&m_module, &m_module_jit, &debug_elf_data, &debug_elf_size));

        if (debug_elf_path != nullptr) {
            FILE* f = fopen(debug_elf_path, "wb");
            CHECK(f != nullptr, "%s: %s", strerror(errno), debug_elf_path);
            CHECK(fwrite(debug_elf_data, debug_elf_size, 1, f) == 1, "%s", strerror(errno));
            fclose(f);
        }

        if (gdb_jit) {
            // Per the GDB JIT protocol: build a code entry, link it into the
            // descriptor, set action_flag = JIT_REGISTER_FN, then call the
            // breakpoint function. GDB picks the entry up only when it's
            // attached; running without GDB makes this a couple of pointer
            // writes and a no-op function call.
            gdb_jit_entry = calloc(1, sizeof(*gdb_jit_entry));
            CHECK(gdb_jit_entry != nullptr);
            gdb_jit_entry->symfile_addr = debug_elf_data;
            gdb_jit_entry->symfile_size = debug_elf_size;
            gdb_jit_entry->next_entry = __jit_debug_descriptor.first_entry;
            if (__jit_debug_descriptor.first_entry != nullptr) {
                __jit_debug_descriptor.first_entry->prev_entry = gdb_jit_entry;
            }
            __jit_debug_descriptor.first_entry = gdb_jit_entry;
            __jit_debug_descriptor.relevant_entry = gdb_jit_entry;
            __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
            __jit_debug_register_code();
        }
    }

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

    // Apply each active data segment into the freshly-mapped memory.
    // Per spec, the destination range must fit inside the current size,
    // otherwise instantiation traps. We compute end via uint64 to catch
    // u32 overflow without relying on a runtime page-fault.
    for (uint32_t i = 0; i < m_module.data_segments_count; i++) {
        wasm_data_segment_t* seg = &m_module.data_segments[i];
        uint64_t end = (uint64_t)seg->offset + seg->len;
        CHECK(end <= m_memory_size,
              "data segment %u exceeds memory size (%llu > %zu)",
              i, (unsigned long long)end, m_memory_size);
        if (seg->len != 0) {
            memcpy((char*)m_memory_base + seg->offset, seg->data, seg->len);
        }
    }

    // get the entry point and run it
    int64_t index = wasm_find_export(&m_module, "_start");
    CHECK(index >= 0);
    int (*entry)(void* memory, void* state) = m_module_jit.exports[index].func.address;
    status = entry(m_memory_base, state_base);

cleanup:
    // Unregister the GDB JIT entry before freeing the ELF buffer, otherwise
    // GDB would chase a dangling pointer on the next debugger interaction.
    if (gdb_jit_entry != nullptr) {
        __jit_debug_descriptor.action_flag = JIT_UNREGISTER_FN;
        __jit_debug_descriptor.relevant_entry = gdb_jit_entry;
        if (gdb_jit_entry->prev_entry != nullptr) {
            gdb_jit_entry->prev_entry->next_entry = gdb_jit_entry->next_entry;
        } else {
            __jit_debug_descriptor.first_entry = gdb_jit_entry->next_entry;
        }
        if (gdb_jit_entry->next_entry != nullptr) {
            gdb_jit_entry->next_entry->prev_entry = gdb_jit_entry->prev_entry;
        }
        __jit_debug_register_code();
        free(gdb_jit_entry);
    }
    wasm_host_free(debug_elf_data);
    free(debug_elf_path);

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
