#pragma once

#include "wasm/wasm.h"
#include "wasm/jit.h"

// The host-side runtime backing a single JIT'd module instance. It owns the
// linear memory mapping and holds the live module/jit handles that the stateful
// wasm_host_* callbacks (memory.size / memory.grow) and wasi-threads read.
//
// A process runs exactly one module, so this state is a singleton rather than an
// explicit context: generated code calls back with only (memory, state), so
// there is nowhere to thread a per-instance handle through. The stateless host
// callbacks (allocation, atomics, logging, JIT code mapping) live in
// host_platform.c instead.

/**
 * Bind the runtime to a freshly loaded and JIT'd module: reserve and commit the
 * linear memory, install its initial contents, and resolve the wasi-threads
 * entry point if the module exports one. Call once before running.
 */
wasm_err_t runtime_init(wasm_module_t* module, wasm_module_jit_t* jit);

/**
 * Release the linear memory mapping. Safe even if runtime_init failed partway.
 */
void runtime_destroy(void);

/**
 * Base of the linear memory, passed to every generated function as `memory`.
 */
void* runtime_memory_base(void);

/**
 * Allocate and seed a per-instance state buffer (globals + tables) from the JIT
 * initializer. On success writes the buffer to *out (NULL when the module
 * declares no state) and returns true; returns false only on allocation
 * failure. The caller frees the buffer with free().
 */
bool runtime_alloc_state(void** out);

/**
 * Resolve an import to the host function backing it, linking the guest against
 * the host's import surface: a handful of test functions under "env",
 * wasi_snapshot_preview1, and the wasi-threads thread-spawn. Returns NULL when
 * the import is unknown. Matches wasm_jit_config_t.resolve_import.
 */
void* runtime_resolve_import(void* arg, const char* module, const char* name, wasm_type_t* type);

/**
 * Map every import to a stub. Used by --jit-only runs, which compile the module
 * but never execute it, so the imports only need to link, not work.
 */
void* runtime_resolve_import_dummy(void* arg, const char* module, const char* name, wasm_type_t* type);
