#pragma once

#include "wasm/error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct jit_elf_function {
    // If non-null, use this name verbatim. Otherwise the symbol name is
    // formatted as "func<wasm_funcidx>".
    const char* name;
    uint32_t wasm_funcidx;

    // offset of the symbol from the start of the .text section. For exported
    // functions this points at the indirect entry (the endbr64 four bytes
    // before the actual code start); for everything else it's the function
    // code itself.
    uint64_t offset;

    // size in bytes of the region the symbol covers
    uint32_t size;
} jit_elf_function_t;

typedef struct jit_elf_input {
    // the loaded code region
    void* code_addr;
    size_t code_size;

    // the loaded rodata region
    void* rodata_addr;
    size_t rodata_size;

    // function symbols
    jit_elf_function_t* funcs;
    size_t funcs_count;
} jit_elf_input_t;

/**
 * Build an in-memory ELF describing the JIT'd code, suitable for both the
 * GDB JIT interface and for being written to disk for external inspection.
 *
 * The returned buffer is owned by the caller and must be freed via
 * wasm_host_free().
 */
wasm_err_t jit_emit_elf(const jit_elf_input_t* input, void** out_buffer, size_t* out_size);
