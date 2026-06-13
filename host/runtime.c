#include "runtime.h"
#include "wasi.h"

#include "wasm/host.h"
#include "util/except.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// 8 GiB of address space, reserved up front as required by the JIT's linear
// memory model: the base never moves, and memory.grow only commits more of this
// already-reserved range.
#define MEMORY_RESERVE_SIZE (8ull * 1024ull * 1024ull * 1024ull)

// --- Live instance state -------------------------------------------------
// Set once by runtime_init and read by the host callbacks below. See runtime.h
// for why this is a process-wide singleton rather than a passed-in context.
static wasm_module_t* m_module = nullptr;
static wasm_module_jit_t* m_jit = nullptr;
static void* m_memory_base = nullptr;

// Current committed size of the linear memory, in bytes. memory.size reads this
// lock-free while a concurrent memory.grow may be publishing a new value, so
// it's atomic: the grow mutex only serializes growers against each other, not
// against readers.
static _Atomic size_t m_memory_size = 0;

bool runtime_alloc_state(void** out) {
    if (m_jit->state_size == 0) {
        *out = nullptr;
        return true;
    }
    void* state = malloc(m_jit->state_size);
    if (state == nullptr) {
        return false;
    }
    memcpy(state, m_jit->state_init, m_jit->state_size);
    *out = state;
    return true;
}

// --- wasi-threads thread-spawn -------------------------------------------
// ABI: `thread-spawn(start_arg: i32) -> i32` asks the host to start a thread
// that re-enters the module through the exported
// `wasi_thread_start(thread_id, start_arg)`. It returns a positive thread id on
// success, or a negative value on failure.
//
// Threading model: every thread shares the single linear memory (it's declared
// `shared`, and m_memory_base is a process-global mapping whose base never
// moves). But each thread is its own instance as far as *globals* go: the
// mutable globals __stack_pointer and __tls_base must be private per thread or
// the threads would stomp each other's stacks. In this JIT the globals live in
// the `state` buffer, so each thread gets a fresh copy seeded from state_init;
// wasi_thread_start then installs that thread's stack/TLS out of the start_args
// block (which lives in the shared memory).

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

    // Run the wasm-side thread entry on the shared memory with this thread's own
    // globals. Returns when the thread's start routine does.
    m_wasi_thread_start(m_memory_base, args.state, args.thread_id, args.start_arg);

    free(args.state);
    return nullptr;
}

static int32_t wasi_spawn_thread(void* memory, void* state, int32_t start_arg) {
    (void)memory; (void)state;

    // Per-thread copy of the globals/tables. Sharing the spawner's `state` would
    // mean sharing __stack_pointer — immediate stack corruption.
    void* thread_state = nullptr;
    if (!runtime_alloc_state(&thread_state)) {
        return -1;
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
    // (memory.atomic.wait/notify), never through the host — so nothing here ever
    // pthread_join()s the OS thread. Let the runtime reap it.
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

// --- Test-only host imports under module name "env" ----------------------

/**
 * Test-only host imports used by tests/cases/imports.wat to exercise the JIT's
 * import-call path. Real runtimes would link against WASI or a richer surface;
 * these stay minimal so the test is self-contained.
 *
 * Imported wasm functions are codegen'd with the same hidden (memory, state)
 * prefix as internal functions, so the host signatures must include them too.
 */
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

void* runtime_resolve_import(void* arg, const char* module, const char* name, wasm_type_t* type) {
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

void* runtime_resolve_import_dummy(void* arg, const char* module, const char* name, wasm_type_t* type) {
    (void)arg; (void)module; (void)name; (void)type;
    return runtime_resolve_import_dummy;
}

// --- Lifecycle -----------------------------------------------------------

wasm_err_t runtime_init(wasm_module_t* module, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    m_module = module;
    m_jit = jit;

    // Reserve the full address range up front (PROT_NONE: no pages committed).
    m_memory_base = mmap(
        nullptr,
        MEMORY_RESERVE_SIZE,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
        -1,
        0
    );
    CHECK(m_memory_base != MAP_FAILED);

    // Commit the initial pages. Skipped when the module declares no memory —
    // such modules never touch this region.
    if (module->memory.min != 0) {
        void* mapped = mmap(
            m_memory_base,
            module->memory.min,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );
        CHECK(mapped != MAP_FAILED);
        CHECK(mapped == m_memory_base);
        m_memory_size = module->memory.min;
    }

    // Install the active data segments / table initializers.
    wasm_module_init_memory(module, m_memory_base);

    // Resolve the wasm-side thread entry up front so thread-spawn (first reached
    // from _start onwards) doesn't race to look it up. Absent for non-threaded
    // modules, which simply never call thread-spawn.
    int64_t thread_start_index = wasm_find_export(module, "wasi_thread_start");
    if (thread_start_index >= 0) {
        m_wasi_thread_start = jit->exports[thread_start_index].func.address;
    }

cleanup:
    return err;
}

void runtime_destroy(void) {
    if (m_memory_base != nullptr && m_memory_base != MAP_FAILED) {
        munmap(m_memory_base, MEMORY_RESERVE_SIZE);
    }
    m_memory_base = nullptr;
}

void* runtime_memory_base(void) {
    return m_memory_base;
}

// --- Stateful host callbacks (declared in wasm/host.h) -------------------
// These live here rather than in host_platform.c because they read the live
// instance state above. The remaining wasm_host_* callbacks are stateless and
// live in host_platform.c.

int32_t wasm_host_memory_size(void* memory_base, void* state_base) {
    (void)memory_base; (void)state_base;
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

int32_t wasm_host_memory_grow(void* memory_base, void* state_base, int32_t new_page_count) {
    (void)state_base;
    if (new_page_count < 0) return -1;

    bool shared = m_module->memory.shared;
    if (shared) pthread_mutex_lock(&m_memory_grow_lock);

    // Spec: refuse if growth would exceed the declared max. The size increase
    // happens only along the way to a successful mmap, so the -1 return path
    // leaves m_memory_size untouched. Relaxed is enough here: the mutex already
    // orders this against other growers, and readers don't depend on this load.
    size_t old_count = atomic_load_explicit(&m_memory_size, memory_order_relaxed) / WASM_PAGE_SIZE;
    size_t new_count = old_count + (size_t)new_page_count;
    if (new_count * WASM_PAGE_SIZE > m_module->memory.max) {
        if (shared) pthread_mutex_unlock(&m_memory_grow_lock);
        return -1;
    }

    // Map only the newly-added range. The original pages are already mapped
    // (since startup); using MAP_FIXED over them would zap any data the program
    // has stored there.
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

    // Release store so a lock-free memory.size reader that observes the new size
    // also sees the mmap that backs it.
    atomic_store_explicit(&m_memory_size, new_count * WASM_PAGE_SIZE, memory_order_release);

    if (shared) {
        atomic_thread_fence(memory_order_seq_cst);
        pthread_mutex_unlock(&m_memory_grow_lock);
    }

    return (int32_t)old_count;
}
