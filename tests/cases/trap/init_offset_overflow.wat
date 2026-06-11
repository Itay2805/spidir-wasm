(module
  (memory 1)
  (data $d "ABCDE")
  (func $_start (result i32)
    ;; SPEC: memory.init with src_offset + n > data_len MUST TRAP (step 9).
    ;; src_offset=0xFFFFFFFF, n=2 => j+n = 0x100000001 > 5 => trap.
    ;; OBSERVED: jit_helper_memory_init computes (uint32_t)(0xFFFFFFFF+2)=1 in 32-bit,
    ;; ASSERT(1 <= 5) passes, then memcpy reads data+0xFFFFFFFF -> wild OOB read.
    ;; Crashes (ASan SEGV, exit 1) instead of a clean wasm trap; on a non-ASan
    ;; build the wild read could copy host bytes into wasm memory (info leak).
    i32.const 0           ;; dst (in-bounds)
    i32.const 0xFFFFFFFF  ;; src_offset
    i32.const 2           ;; n
    memory.init $d
    i32.const 0)
  (export "_start" (func $_start)))
