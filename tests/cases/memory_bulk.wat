;; Exercises the wasm bulk-memory ops `memory.copy` and `memory.fill`,
;; which the JIT lowers to a call into the C helpers in src/jit/helpers.c
;; rather than emitting an open-coded loop in spidir IR.
;;
;; Each check traps on first mismatch — exit-0 means every individual
;; case matched. Cross-validated against `wasm-interp`.
(module
  (memory 1)

  (func $_start (result i32)
    ;; --- memory.fill basics ---------------------------------------------
    ;; Fill 32 bytes at offset 0 with 0xAB. Spot-check a few positions.
    i32.const 0     i32.const 0xAB  i32.const 32  memory.fill

    block i32.const 0  i32.load8_u  i32.const 0xAB i32.eq br_if 0 unreachable end
    block i32.const 15 i32.load8_u  i32.const 0xAB i32.eq br_if 0 unreachable end
    block i32.const 31 i32.load8_u  i32.const 0xAB i32.eq br_if 0 unreachable end
    ;; The byte after the filled range must remain zero (initial mem state).
    block i32.const 32 i32.load8_u  i32.const 0    i32.eq br_if 0 unreachable end

    ;; memory.fill takes an i32 value but only the low 8 bits matter — verify
    ;; with a value whose upper bits are non-zero.
    i32.const 100   i32.const 0xDEADBE42  i32.const 4  memory.fill
    block i32.const 100 i32.load8_u i32.const 0x42 i32.eq br_if 0 unreachable end
    block i32.const 103 i32.load8_u i32.const 0x42 i32.eq br_if 0 unreachable end

    ;; n = 0 must be a no-op even when the destination is at exactly mem_size.
    ;; Memory has 1 page = 65536 bytes, so addr 65536 is one-past-the-end.
    i32.const 65536  i32.const 0xFF  i32.const 0  memory.fill   ;; spec: ok

    ;; --- memory.copy: non-overlapping --------------------------------------
    ;; Seed [200..208) with a known pattern using stores, then copy to [300..308).
    i32.const 200  i32.const 0x44332211  i32.store
    i32.const 204  i32.const 0x88776655  i32.store
    i32.const 300  i32.const 200  i32.const 8  memory.copy
    block i32.const 300 i32.load  i32.const 0x44332211 i32.eq br_if 0 unreachable end
    block i32.const 304 i32.load  i32.const 0x88776655 i32.eq br_if 0 unreachable end

    ;; --- memory.copy: forward-overlap (src < dst) -------------------------
    ;; The spec says memory.copy must behave like memmove, i.e. it works
    ;; correctly even when the regions overlap. With src < dst the naive
    ;; left-to-right loop would clobber its own input.
    ;; Seed [400..405): 1 2 3 4 5
    i32.const 400 i32.const 1 i32.store8
    i32.const 401 i32.const 2 i32.store8
    i32.const 402 i32.const 3 i32.store8
    i32.const 403 i32.const 4 i32.store8
    i32.const 404 i32.const 5 i32.store8
    ;; Copy [400..403) to [402..405). Expected after: 1 2 1 2 3.
    i32.const 402 i32.const 400 i32.const 3 memory.copy
    block i32.const 400 i32.load8_u i32.const 1 i32.eq br_if 0 unreachable end
    block i32.const 401 i32.load8_u i32.const 2 i32.eq br_if 0 unreachable end
    block i32.const 402 i32.load8_u i32.const 1 i32.eq br_if 0 unreachable end
    block i32.const 403 i32.load8_u i32.const 2 i32.eq br_if 0 unreachable end
    block i32.const 404 i32.load8_u i32.const 3 i32.eq br_if 0 unreachable end

    ;; --- memory.copy: backward-overlap (dst < src) ------------------------
    ;; Seed [500..505): 10 20 30 40 50
    i32.const 500 i32.const 10 i32.store8
    i32.const 501 i32.const 20 i32.store8
    i32.const 502 i32.const 30 i32.store8
    i32.const 503 i32.const 40 i32.store8
    i32.const 504 i32.const 50 i32.store8
    ;; Copy [502..505) to [500..503). Expected after: 30 40 50 40 50.
    i32.const 500 i32.const 502 i32.const 3 memory.copy
    block i32.const 500 i32.load8_u i32.const 30 i32.eq br_if 0 unreachable end
    block i32.const 501 i32.load8_u i32.const 40 i32.eq br_if 0 unreachable end
    block i32.const 502 i32.load8_u i32.const 50 i32.eq br_if 0 unreachable end
    block i32.const 503 i32.load8_u i32.const 40 i32.eq br_if 0 unreachable end
    block i32.const 504 i32.load8_u i32.const 50 i32.eq br_if 0 unreachable end

    ;; --- memory.copy: n = 0 is a no-op even at boundary -------------------
    i32.const 65536  i32.const 65536  i32.const 0  memory.copy   ;; spec: ok

    i32.const 0)

  (export "_start" (func $_start)))
