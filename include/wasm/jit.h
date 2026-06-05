#pragma once

#include <stdbool.h>
#include <spidir/codegen.h>
#include "wasm/wasm.h"

#include "error.h"

typedef struct wasm_jit_config {
    /**
     * For dumping the spidir module, can be set to
     * null to not dump it at all
     */
    spidir_dump_callback_t dump_callback;
    void* dump_arg;

    /**
     * For resolving imports from the jit, return nullptr if not found
     */
    void* (*resolve_import)(void* arg, const char* module, const char* name, wasm_type_t* type);
    void* resolve_import_arg;

    /**
     * Should we run the spidir optimizations
     */
    bool optimize;

    /**
     * Capture per-function layout and resolved relocations into
     * wasm_module_jit_t::debug. Off by default — the bookkeeping is only
     * useful for the debug ELF / GDB JIT path and otherwise pays a per-reloc
     * allocation that the runtime doesn't read.
     */
    bool emit_debug_info;
} wasm_jit_config_t;

typedef union wasm_jit_export {
    struct {
        void* address;
    } func;
    struct {
        size_t offset;
    } global;
} wasm_jit_export_t;

// Per-internal-function code layout, captured during JIT and exposed to the
// debug ELF emitter (and anyone else who needs to map a runtime address back
// to a wasm function).
typedef struct wasm_jit_func_layout {
    // wasm-funcidx (already includes the imports offset). This is the index
    // into wasm_module.function_names, code, etc.
    uint32_t funcidx;

    // Address in the live JIT binary. Same value reachable through the export
    // table for exported functions; for non-exported functions this is the
    // only handle the host has.
    void* address;

    // Code size in bytes. The 4-byte endbr64 prologue (when present) is *not*
    // counted here — it sits at `address - 4`.
    size_t code_size;

    // Constant-pool location for this function. const_size == 0 means the
    // function has no constpool entry and const_address is undefined.
    void* const_address;
    size_t const_size;
} wasm_jit_func_layout_t;

// Resolved relocation for the debug ELF. After JIT linking the bytes already
// hold the final value; we keep the per-reloc record around so the ELF can
// re-express absolute references symbolically (so GDB / a disassembler will
// label `mov rax, &func` rather than just printing the literal).
typedef enum wasm_jit_reloc_kind {
    WASM_JIT_RELOC_X64_PC32,
    WASM_JIT_RELOC_X64_ABS64,
} wasm_jit_reloc_kind_t;

typedef enum wasm_jit_reloc_target_kind {
    // The relocation points at a wasm-level function (internal or import).
    // `target_funcidx` is its wasm funcidx; `target_address` is the runtime
    // address (the function entry — endbr64 not included).
    WASM_JIT_RELOC_TARGET_FUNC,

    // The relocation points at the function's own constant pool. The pool's
    // runtime address is in `target_address`.
    WASM_JIT_RELOC_TARGET_CONSTPOOL,

    // The relocation points at a runtime helper or anything else we didn't
    // round-trip through a wasm-level symbol. `target_address` is the raw
    // host address; the ELF emitter synthesises an absolute symbol for it.
    WASM_JIT_RELOC_TARGET_EXTERNAL,
} wasm_jit_reloc_target_kind_t;

typedef struct wasm_jit_reloc {
    void* address;
    int64_t addend;
    wasm_jit_reloc_kind_t kind;
    wasm_jit_reloc_target_kind_t target_kind;
    uint32_t target_funcidx;
    void* target_address;

    // Owning function (the function whose code carries this relocation). We
    // keep this so the ELF emitter can attach the reloc to the right symbol /
    // section without a second pass to figure out the containing function.
    uint32_t owner_funcidx;
} wasm_jit_reloc_t;

typedef struct wasm_jit_debug_info {
    wasm_jit_func_layout_t* funcs;
    size_t funcs_count;

    wasm_jit_reloc_t* relocs;
    size_t relocs_count;

    // Bounds of the RX (.text) and RO (.rodata) regions of the JIT binary,
    // recorded once so the ELF emitter doesn't have to recompute them from
    // page counts.
    void* code_base;
    size_t code_size;
    void* rodata_base;
    size_t rodata_size;
} wasm_jit_debug_info_t;

typedef struct wasm_module_jit {
    void* binary;
    size_t rx_page_count;
    size_t ro_page_count;

    // the addresses of exported functions
    wasm_jit_export_t* exports;

    // the size in bytes needed for the runtime state buffer. The host 
    // is expected to allocate this much state when setting up a new 
    // instance of the jitted module
    size_t state_size;

    // initializer for the state buffer. the host must memcpy this into the 
    // allocated state before calling into the code
    void* state_init;

    // Optional debug info for the JIT'd binary, captured during codegen.
    // Always present after a successful jit (even if there are no functions —
    // the bounds are still meaningful).
    wasm_jit_debug_info_t debug;

    // the jitted start function
    void (*start_func)(void* memory_base, void* state_base);
} wasm_module_jit_t;

wasm_err_t wasm_module_jit(wasm_module_t* module, wasm_module_jit_t* jitted_module, wasm_jit_config_t* config);

void wasm_module_jit_free(wasm_module_jit_t* jit);
