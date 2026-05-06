#pragma once

#include <stddef.h>

#include "wasm/error.h"
#include "wasm/jit.h"
#include "wasm/wasm.h"

/**
 * Emit a debug ELF that mirrors the JIT'd binary exactly as it sits in memory.
 *
 * The resulting buffer can be:
 *  - written to disk and inspected with objdump / readelf, or
 *  - handed to the GDB JIT interface (the section virtual addresses already
 *    match the runtime addresses, so GDB will resolve backtraces directly).
 *
 * The .text / .rodata bytes are the live JIT memory: every relocation is
 * already applied, so loading the ELF at the recorded VAs reproduces the
 * runtime image. Absolute (R_X86_64_64) relocations are additionally
 * preserved as ELF rela entries so disassemblers can render symbolic targets.
 *
 * Function symbols use the wasm `name` custom section when available.
 * Imports/exports fall back to the module/item names exposed by the loader,
 * matching the user-visible identifiers a programmer would expect to see.
 *
 * On success the buffer is allocated via wasm_host_calloc and ownership
 * transfers to the caller (free with wasm_host_free).
 */
wasm_err_t wasm_jit_emit_debug_elf(
    const wasm_module_t* module,
    const wasm_module_jit_t* jit,
    void** out_data,
    size_t* out_size
);
