#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <wasm/wasm.h>
#include <wasm/jit.h>
#include <wasm/debug_elf.h>

#include <util/except.h>
#include <spidir/log.h>

#include "wasi.h"

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
// Current committed size of the linear memory, in bytes. memory.size reads
// this lock-free while a concurrent memory.grow may be publishing a new value,
// so it's atomic: the grow mutex only serializes growers against each other,
// not against readers.
static _Atomic size_t m_memory_size = 0;

// --- wasi-threads thread-spawn -------------------------------------------
// ABI: `thread-spawn(start_arg: i32) -> i32` asks the host to start a thread
// that re-enters the module through the exported
// `wasi_thread_start(thread_id, start_arg)`. It returns a positive thread id
// on success, or a negative value on failure.
//
// Threading model: every thread shares the single linear memory (it's declared
// `shared`, and m_memory_base is a process-global mapping whose base never
// moves — memory.grow only commits more of the reserved range). But each
// thread is its own instance as far as *globals* go: the mutable globals
// __stack_pointer and __tls_base must be private per thread or the threads
// would stomp each other's stacks. In this JIT the globals live in the `state`
// buffer, so each thread gets a fresh copy seeded from state_init;
// wasi_thread_start then installs that thread's stack/TLS out of the
// start_args block (which lives in the shared memory).

typedef void (*wasi_thread_start_fn_t)(void* memory, void* state, int32_t thread_id, int32_t start_arg);
static wasi_thread_start_fn_t m_wasi_thread_start = nullptr;

// Thread ids must be positive and unique; 0 is the implicit main thread.
static _Atomic int32_t m_next_thread_id = 1;

typedef struct thread_spawn_args {
    int32_t thread_id;
    int32_t start_arg;
    void* state;
} thread_spawn_args_t;

static void* thread_trampoline(void* arg) {
    thread_spawn_args_t args = *(thread_spawn_args_t*)arg;
    free(arg);

    // Run the wasm-side thread entry on the shared memory with this thread's
    // own globals. Returns when the thread's start routine does.
    m_wasi_thread_start(m_memory_base, args.state, args.thread_id, args.start_arg);

    free(args.state);
    return nullptr;
}

static int32_t wasi_spawn_thread(void* memory, void* state, int32_t start_arg) {
    (void)memory; (void)state;

    // Per-thread copy of the globals/tables. Sharing the spawner's `state`
    // would mean sharing __stack_pointer — immediate stack corruption.
    void* thread_state = nullptr;
    if (m_module_jit.state_size != 0) {
        thread_state = malloc(m_module_jit.state_size);
        if (thread_state == nullptr) return -1;
        memcpy(thread_state, m_module_jit.state_init, m_module_jit.state_size);
    }

    thread_spawn_args_t* args = malloc(sizeof(*args));
    if (args == nullptr) {
        free(thread_state);
        return -1;
    }
    int32_t thread_id = atomic_fetch_add_explicit(&m_next_thread_id, 1, memory_order_relaxed);
    args->thread_id = thread_id;
    args->start_arg = start_arg;
    args->state = thread_state;

    // Detached: the guest joins via a futex it parks on in linear memory
    // (memory.atomic.wait/notify), never through the host — so nothing here
    // ever pthread_join()s the OS thread. Let the runtime reap it.
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        free(args);
        free(thread_state);
        return -1;
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // After a successful create, `args` (and thread_state) belong to the
    // trampoline — the new thread may have already freed them, so read nothing
    // through `args` past this point; return the id we stashed in a local.
    pthread_t thread;
    int rc = pthread_create(&thread, &attr, thread_trampoline, args);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        free(args);
        free(thread_state);
        return -1;
    }

    return thread_id;
}

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
    } else if (strcmp(module, "wasi_snapshot_preview1") == 0) {
        return wasip1_resolve_import(name);
    } else if (strcmp(module, "wasi") == 0) {
        if (strcmp(name, "thread-spawn") == 0) return wasi_spawn_thread;
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
        memcpy(state_base, m_module_jit.state_init, m_module_jit.state_size);
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
    if (m_module.memory.min != 0) {
        m_memory_size = m_module.memory.min;
        void* mapped = mmap(
            m_memory_base,
            m_module.memory.min,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );
        CHECK(mapped != MAP_FAILED);
        CHECK(mapped == m_memory_base);
    }

    // setup the memory of the new module
    wasm_module_init_memory(&m_module, m_memory_base);

    // Resolve the wasm-side thread entry up front so thread-spawn (first reached
    // from _start onwards) doesn't race to look it up. Absent for non-threaded
    // modules, which simply never call thread-spawn.
    int64_t thread_start_index = wasm_find_export(&m_module, "wasi_thread_start");
    if (thread_start_index >= 0) {
        m_wasi_thread_start = m_module_jit.exports[thread_start_index].func.address;
    }

    // start with running the start section, it should always run no matter what
    if (m_module.start_func >= 0) {
        m_module_jit.start_func(m_memory_base, state_base);
    }
    
    // find the real entry point
    uint64_t index = wasm_find_export(&m_module, "_start");
    CHECK(index >= 0);
    int (*entry)(void* memory, void* state) = m_module_jit.exports[index].func.address;

    // and run it
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host memory instance handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t wasm_host_memory_size(void* memory_base) {
    // Lock-free, race-free read. memory.size only has to observe a valid size,
    // not block: the value grows monotonically and grow publishes it with a
    // release store, so a concurrent grow can at worst make this a stale lower
    // bound (a legal observation), never a torn or over-reported one. The
    // acquire pairs with that release so a caller that reads the size and then
    // touches the new pages sees them mapped.
    return atomic_load_explicit(&m_memory_size, memory_order_acquire) / WASM_PAGE_SIZE;
}

// Serializes concurrent memory.grow on a shared memory. A grow is a
// read-modify-write of m_memory_size plus the mmap that commits the new pages,
// so two threads growing at once would race on both. Private memories are only
// ever touched by their single thread, so the lock is taken only when shared.
static pthread_mutex_t m_memory_grow_lock = PTHREAD_MUTEX_INITIALIZER;

int32_t wasm_host_memory_grow(void* memory_base, int32_t new_page_count) {
    if (new_page_count < 0) return -1;

    bool shared = m_module.memory.shared;
    if (shared) pthread_mutex_lock(&m_memory_grow_lock);

    // Spec: refuse if growth would exceed the declared max. The size
    // increase happens only along the way to a successful mmap, so the
    // -1 return path leaves m_memory_size untouched.
    // Relaxed is enough here: the mutex already orders this against other
    // growers, and readers don't depend on this particular load.
    size_t old_count = atomic_load_explicit(&m_memory_size, memory_order_relaxed) / WASM_PAGE_SIZE;
    size_t new_count = old_count + (size_t)new_page_count;
    if (new_count * WASM_PAGE_SIZE > m_module.memory.max) {
        if (shared) pthread_mutex_unlock(&m_memory_grow_lock);
        return -1;
    }

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
        if (added == MAP_FAILED) {
            if (shared) pthread_mutex_unlock(&m_memory_grow_lock);
            return -1;
        }
    }

    // Release store so a lock-free memory.size reader that observes the new
    // size also sees the mmap that backs it.
    atomic_store_explicit(&m_memory_size, new_count * WASM_PAGE_SIZE, memory_order_release);

    if (shared) {
        atomic_thread_fence(memory_order_seq_cst);
        pthread_mutex_unlock(&m_memory_grow_lock);
    }

    return (int32_t)old_count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host allocator functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Host atomic notify/wait implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void futex_deadline(int64_t timeout_ns, struct timespec* out) {
    clock_gettime(CLOCK_MONOTONIC, out);
    out->tv_sec += timeout_ns / 1000000000;
    long ns = out->tv_nsec + timeout_ns % 1000000000;
    if (ns >= 1000000000) {
        out->tv_sec += 1;
        ns -= 1000000000;
    }
    out->tv_nsec = ns;
}

uint32_t wasm_host_atomic_notify(void* ptr, uint32_t count) {
    if (count == 0) return 0;
    if (count > INT32_MAX) count = INT32_MAX;
    long rc = syscall(SYS_futex, ptr, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
    return rc < 0 ? 0 : (uint32_t)rc;
}

uint32_t wasm_host_atomic_wait_4(_Atomic(uint32_t)* value, uint32_t expected, int64_t timeout) {
    struct timespec deadline;
    struct timespec* dp = nullptr;
    if (timeout >= 0) {
        futex_deadline(timeout, &deadline);
        dp = &deadline;
    }
    for (;;) {
        long rc = syscall(SYS_futex, value, FUTEX_WAIT_BITSET_PRIVATE, expected, dp, nullptr, FUTEX_BITSET_MATCH_ANY);
        if (rc == 0) return 0;
        if (errno == EAGAIN) return 1;
        if (errno == ETIMEDOUT) return 2;
        // EINTR: the deadline is absolute, so just re-issue.
    }
}

uint32_t wasm_host_atomic_wait_8(_Atomic(uint64_t)* value, uint64_t expected, int64_t timeout) {
    ASSERT(!"TODO: this");
}

