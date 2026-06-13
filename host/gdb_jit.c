#include "gdb_jit.h"

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN = 1,
    JIT_UNREGISTER_FN = 2,
} jit_actions_t;

/**
 * The field layout of both structs below is dictated by the GDB JIT protocol;
 * GDB reads them by offset, so the members must not be reordered or retyped.
 */
struct gdb_jit_entry {
    struct gdb_jit_entry* next_entry;
    struct gdb_jit_entry* prev_entry;
    const char* symfile_addr;
    uint64_t symfile_size;
};

struct jit_descriptor {
    uint32_t version;
    uint32_t action_flag;
    struct gdb_jit_entry* relevant_entry;
    struct gdb_jit_entry* first_entry;
};

/**
 * GDB watches this exact symbol with a software breakpoint; the noinline/used
 * attributes keep the breakpoint site from being optimized away. Calling it is
 * how we hand control to the debugger after updating the descriptor.
 */
__attribute__((noinline, used))
void __jit_debug_register_code(void) {
    __asm__ __volatile__("");
}

struct jit_descriptor __jit_debug_descriptor = { 1, 0, nullptr, nullptr };

gdb_jit_entry_t* gdb_jit_register(const void* elf_data, size_t elf_size) {
    struct gdb_jit_entry* entry = calloc(1, sizeof(*entry));
    if (entry == nullptr) {
        return nullptr;
    }
    entry->symfile_addr = elf_data;
    entry->symfile_size = elf_size;

    // Push onto the front of the descriptor's list, then announce it.
    entry->next_entry = __jit_debug_descriptor.first_entry;
    if (__jit_debug_descriptor.first_entry != nullptr) {
        __jit_debug_descriptor.first_entry->prev_entry = entry;
    }
    __jit_debug_descriptor.first_entry = entry;
    __jit_debug_descriptor.relevant_entry = entry;
    __jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
    __jit_debug_register_code();

    return entry;
}

void gdb_jit_unregister(gdb_jit_entry_t* entry) {
    if (entry == nullptr) {
        return;
    }

    // Unlink before announcing so GDB never walks a node we're about to free.
    if (entry->prev_entry != nullptr) {
        entry->prev_entry->next_entry = entry->next_entry;
    } else {
        __jit_debug_descriptor.first_entry = entry->next_entry;
    }
    if (entry->next_entry != nullptr) {
        entry->next_entry->prev_entry = entry->prev_entry;
    }

    __jit_debug_descriptor.relevant_entry = entry;
    __jit_debug_descriptor.action_flag = JIT_UNREGISTER_FN;
    __jit_debug_register_code();

    free(entry);
}
