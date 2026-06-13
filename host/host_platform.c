// Stateless implementations of the wasm/host.h callbacks: allocation, JIT code
// mapping, atomic wait/notify, and logging. These are thin wrappers over libc
// and the OS and hold no per-instance state. The stateful callbacks
// (memory.size / memory.grow) live in runtime.c, next to the instance state
// they read.

#include "wasm/host.h"
#include "util/except.h"

#include <errno.h>
#include <linux/futex.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// The OS page size the JIT allocator works in. Distinct from WASM_PAGE_SIZE
// (the 64 KiB linear-memory page); these are the host's RX/RO mapping pages.
#define HOST_PAGE_SIZE 4096

// --- Logging -------------------------------------------------------------

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

// --- Allocation ----------------------------------------------------------

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
    return HOST_PAGE_SIZE;
}

// --- JIT code mapping ----------------------------------------------------
// The JIT allocates one contiguous region (RX pages followed by RO pages),
// initially writable, fills it in, then calls wasm_host_jit_lock to drop write
// access and grant execute to the RX half.

void* wasm_host_jit_alloc(size_t rx_page_count, size_t ro_page_count) {
    void* ptr = mmap(nullptr, (rx_page_count + ro_page_count) * HOST_PAGE_SIZE,
                     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;
    return ptr;
}

bool wasm_host_jit_lock(void* ptr, size_t rx_page_count, size_t ro_page_count) {
    size_t rx_size = rx_page_count * HOST_PAGE_SIZE;
    size_t ro_size = ro_page_count * HOST_PAGE_SIZE;

    if (mprotect(ptr, rx_size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect(PROT_READ | PROT_EXEC)");
        return false;
    }

    if (mprotect((char*)ptr + rx_size, ro_size, PROT_READ) != 0) {
        perror("mprotect(PROT_READ)");
        return false;
    }

    return true;
}

void wasm_host_jit_free(void* ptr, size_t rx_page_count, size_t ro_page_count) {
    munmap(ptr, (rx_page_count + ro_page_count) * HOST_PAGE_SIZE);
}

// --- Atomic wait / notify ------------------------------------------------
// Backed by Linux futexes: linear-memory addresses double as futex words, so a
// guest memory.atomic.wait/notify maps straight onto FUTEX_WAIT/FUTEX_WAKE.

/**
 * Convert a relative nanosecond timeout into the absolute CLOCK_MONOTONIC
 * deadline that FUTEX_WAIT_BITSET expects.
 */
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
    (void)value; (void)expected; (void)timeout;
    ASSERT(!"TODO: 64-bit atomic wait");
}
