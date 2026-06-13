#pragma once

#include <stddef.h>

// Minimal client of the GDB JIT interface
// (https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html). Publishing an
// in-memory ELF lets an attached debugger map generated code back to wasm-level
// function names. The protocol is passive: with no debugger attached,
// register/unregister cost just a few pointer writes and a no-op call.

typedef struct gdb_jit_entry gdb_jit_entry_t;

/**
 * Publish an in-memory ELF image to the debugger. The bytes must stay alive and
 * unmodified until the returned handle is passed to gdb_jit_unregister.
 *
 * Returns the entry handle, or NULL on allocation failure.
 */
gdb_jit_entry_t* gdb_jit_register(const void* elf_data, size_t elf_size);

/**
 * Withdraw a previously registered entry and free its handle. NULL is a no-op,
 * so this is safe to call unconditionally during cleanup.
 */
void gdb_jit_unregister(gdb_jit_entry_t* entry);
