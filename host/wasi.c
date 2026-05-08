#include "util/defs.h"
#include <stdint.h>
#include <string.h>

typedef struct wasi_syscall {
    const char* name;
    void* addr;
} wasi_syscall_t;

static const wasi_syscall_t m_wasip1_syscalls[] = {
    
};

void* wasip1_resolve_import(const char* name) {
    for (int i = 0; i < ARRAY_LENGTH(m_wasip1_syscalls); i++) {
        if (strcmp(m_wasip1_syscalls[i].name, name) == 0) {
            return m_wasip1_syscalls[i].addr;
        }
    }
    return nullptr;
}
