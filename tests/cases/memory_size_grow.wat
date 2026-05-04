;; Exercises wasm `memory.size` and `memory.grow` (lowered to host
;; helpers via the JIT helper infrastructure). Each check traps on
;; first mismatch — exit-0 means every individual case matched.
;; Cross-validated against `wasm-interp`.
(module
  ;; Declared bounds: min 1 page, max 4 pages = 64 KiB ... 256 KiB.
  ;; A bounded max is needed so we can verify that memory.grow refuses
  ;; to extend past it (returning -1, the spec sentinel).
  (memory 1 4)

  (func $_start (result i32)
    ;; --- memory.size at startup returns the declared min (1 page) ---
    block memory.size i32.const 1 i32.eq br_if 0 unreachable end

    ;; Seed the original page with sentinels so we can prove later grows
    ;; don't wipe data that was already there. This catches the
    ;; regression where memory.grow re-mmap'd the original range with
    ;; MAP_FIXED and zeroed it.
    i32.const 0     i32.const 0xDEADBEEF i32.store
    i32.const 32768 i32.const 0xFEEDFACE i32.store

    ;; --- memory.grow 0 must return the current size (1) and not
    ;; observably change anything; memory.size should still report 1
    ;; AND the sentinels above must still be intact.
    block i32.const 0 memory.grow  i32.const 1 i32.eq br_if 0 unreachable end
    block memory.size              i32.const 1 i32.eq br_if 0 unreachable end
    block i32.const 0     i32.load i32.const 0xDEADBEEF i32.eq br_if 0 unreachable end
    block i32.const 32768 i32.load i32.const 0xFEEDFACE i32.eq br_if 0 unreachable end

    ;; --- memory.grow 2 must return the OLD size (1) and bring the
    ;; total to 3 pages.
    block i32.const 2 memory.grow  i32.const 1 i32.eq br_if 0 unreachable end
    block memory.size              i32.const 3 i32.eq br_if 0 unreachable end

    ;; Sentinels in the original page must still be intact after a real
    ;; (non-zero) grow.
    block i32.const 0     i32.load i32.const 0xDEADBEEF i32.eq br_if 0 unreachable end
    block i32.const 32768 i32.load i32.const 0xFEEDFACE i32.eq br_if 0 unreachable end

    ;; --- The newly-grown pages must be accessible. Page 1 starts at
    ;; byte offset 65536. Write a sentinel and read it back.
    i32.const 65536  i32.const 0xCAFEBABE  i32.store
    block i32.const 65536 i32.load  i32.const 0xCAFEBABE i32.eq br_if 0 unreachable end

    ;; A higher byte still inside the freshly-grown range must also be
    ;; writable. Page 2 starts at byte 131072 and ends at 196607; 150000
    ;; is comfortably inside it.
    i32.const 150000  i32.const 0x12345678  i32.store
    block i32.const 150000 i32.load  i32.const 0x12345678 i32.eq br_if 0 unreachable end

    ;; --- Per spec, freshly-grown pages start zero. Verify by reading
    ;; an untouched offset within the newly added pages.
    block i32.const 100000 i32.load  i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- memory.grow that would exceed the declared max must return
    ;; -1 and leave the size unchanged. Current size is 3, max is 4,
    ;; so trying to add 2 must fail.
    block i32.const 2 memory.grow  i32.const -1 i32.eq br_if 0 unreachable end
    block memory.size              i32.const 3  i32.eq br_if 0 unreachable end

    ;; --- A grow that exactly hits the max must succeed, and once
    ;; more verify the original page is still intact.
    block i32.const 1 memory.grow  i32.const 3 i32.eq br_if 0 unreachable end
    block memory.size              i32.const 4 i32.eq br_if 0 unreachable end
    block i32.const 0     i32.load i32.const 0xDEADBEEF i32.eq br_if 0 unreachable end
    block i32.const 32768 i32.load i32.const 0xFEEDFACE i32.eq br_if 0 unreachable end

    ;; --- Already at max: any further non-zero grow returns -1.
    block i32.const 1 memory.grow  i32.const -1 i32.eq br_if 0 unreachable end
    block memory.size              i32.const 4  i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
