#include "util/defs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct wasi_syscall {
    const char* name;
    void* addr;
} wasi_syscall_t;

typedef uint32_t wasi_exitcode_t;

static void wasi_proc_exit(void* memory_base, void* state_base, wasi_exitcode_t rval) {
    exit(rval);
}

static const wasi_syscall_t m_wasip1_syscalls[] = {
    { "proc_exit", wasi_proc_exit },
};

void* wasip1_resolve_import(const char* name) {
    for (int i = 0; i < ARRAY_LENGTH(m_wasip1_syscalls); i++) {
        if (strcmp(m_wasip1_syscalls[i].name, name) == 0) {
            return m_wasip1_syscalls[i].addr;
        }
    }
    return nullptr;
}
