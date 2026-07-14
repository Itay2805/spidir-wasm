// libFuzzer entry point for the wasm load + JIT pipeline.
//
// The contract this target exercises: loading and JIT-compiling an arbitrary
// byte string must always either fail cleanly (return a wasm error) or succeed —
// it must never crash, read/write out of bounds, or otherwise corrupt state.
// We deliberately do NOT run the compiled code: executing arbitrary guest code
// can legitimately trap, loop forever, or fault, none of which is what this
// target is checking. So we stop right after codegen and tear everything down.
//
// Build (see host/Makefile + top-level FUZZ flag):
//     make FUZZ=y
//     ./build/fuzz [libfuzzer-args...] [corpus-dir]
//
// Environment knobs:
//     FUZZ_VERBOSE     keep the loader/JIT's stdout logging (off by default so
//                      libFuzzer's own stderr output stays readable)
//     FUZZ_NO_OPTIMIZE compile without spidir optimizations (default: optimize)

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wasm/wasm.h>
#include <wasm/jit.h>
#include <wasm/error.h>

#include <spidir/codegen.h>
#include <spidir/x64.h>

#include <cpuid.h>

// Created once in LLVMFuzzerInitialize and reused for every input. The JIT
// caches the first machine handle it sees process-wide anyway, so handing it a
// stable one keeps a long run from churning allocations.
static spidir_codegen_machine_handle_t m_machine = nullptr;

// Whether to run spidir's optimizer. Both settings are worth fuzzing; pick once
// per process from the environment so each input stays deterministic.
static bool m_optimize = true;

// Imports never get called (we don't run the module), but the JIT still needs a
// non-NULL address to bind each import's relocation against. Any valid code
// pointer does; hand back this stub so import-bearing modules reach codegen
// instead of failing to link.
static void fuzz_import_stub(void) {}

static void* fuzz_resolve_import(void* arg, const char* module, const char* name, wasm_type_t* type) {
    (void)arg; (void)module; (void)name; (void)type;
    return (void*)fuzz_import_stub;
}

int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc; (void)argv;

    // Mirror the host's machine setup: large-abs extern / small-pic internal
    // code models, with popcnt gated on the running CPU.
    spidir_x64_machine_config_t machine_config = {
        .extern_code_model = SPIDIR_X64_CM_LARGE_ABS,
        .internal_code_model = SPIDIR_X64_CM_SMALL_PIC,
    };
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        machine_config.cpu_features.popcnt = ecx & bit_POPCNT;
    }
    m_machine = spidir_codegen_create_x64_machine_with_config(&machine_config);

    m_optimize = getenv("FUZZ_NO_OPTIMIZE") == nullptr;

    // The loader/JIT log every rejected input to stdout via wasm_host_log, which
    // buries libFuzzer's output and slows the run to a crawl. Silence stdout
    // (crash reports from libFuzzer/ASan/UBSan go to stderr) unless asked not to.
    if (getenv("FUZZ_VERBOSE") == nullptr) {
        FILE* devnull = freopen("/dev/null", "w", stdout);
        (void)devnull;
    }

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Copy the input into an exact-size heap buffer: ASan red-zones then flank
    // it, so any over-read by the parser is caught precisely at the boundary,
    // and we honor libFuzzer's const contract (the loader takes a mutable ptr).
    void* buf = malloc(size != 0 ? size : 1);
    if (buf == nullptr) {
        return 0;
    }
    memcpy(buf, data, size);

    // wasm_module_free is safe on a zero-initialized or partially-loaded module
    // (that's how host/main.c cleans up after a failed load), so a single free
    // at the end covers every exit path below.
    wasm_module_t module = {};
    if (wasm_load_module(&module, buf, size) == WASM_NO_ERROR) {
        wasm_module_jit_t jit = {};
        wasm_jit_config_t config = {
            .machine_handle = m_machine,
            .resolve_import = fuzz_resolve_import,
            .optimize = m_optimize,
        };
        if (wasm_module_jit(&module, &jit, &config) == WASM_NO_ERROR) {
            wasm_module_jit_free(&jit);
        }
    }
    wasm_module_free(&module);

    free(buf);
    return 0;
}

// --- Linker glue ---------------------------------------------------------
// The JIT records the addresses of these two host callbacks in its helper table
// (src/jit/helpers.c) even though a non-running module never invokes them. Their
// real implementations live in host/runtime.c alongside the live linear-memory
// state, which this lean fuzz binary deliberately doesn't link. Stubs satisfy
// the reference; they can never actually be called here.

int32_t wasm_host_memory_size(void* memory_base, void* state_base) {
    (void)memory_base; (void)state_base;
    return 0;
}

int32_t wasm_host_memory_grow(void* memory_base, void* state_base, int32_t new_page_count) {
    (void)memory_base; (void)state_base; (void)new_page_count;
    return -1;
}
