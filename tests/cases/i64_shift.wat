;; Exercises i64.shl / i64.shr_s / i64.shr_u / i64.rotl / i64.rotr, plus
;; the wasm rule that the shift/rotation count is taken modulo 64. The
;; rotates additionally exercise the `(bit_count - n) & mask` term in the
;; JIT lowering — the second `& mask` is what keeps n=0 from rotating by
;; the full word width. Returns 0 on success.
(module
  (func $shl   (param i64 i64) (result i64) local.get 0 local.get 1 i64.shl)
  (func $shr_s (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_s)
  (func $shr_u (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_u)
  (func $rotl  (param i64 i64) (result i64) local.get 0 local.get 1 i64.rotl)
  (func $rotr  (param i64 i64) (result i64) local.get 0 local.get 1 i64.rotr)

  (func $_start (result i32)
    (local $sum i32)

    ;; shl(1, 40) = 0x10000000000  (i64)
    i64.const 1 i64.const 40 call $shl
    i64.const 0x10000000000
    i64.eq
    local.set $sum                            ;; +1 -> 1

    ;; shr_s(-1024, 4) = -64
    i64.const -1024 i64.const 4 call $shr_s
    i64.const -64
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 2

    ;; shr_u(0x8000000000000000, 1) = 0x4000000000000000
    i64.const 0x8000000000000000 i64.const 1 call $shr_u
    i64.const 0x4000000000000000
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 3

    ;; modulo-64 on the count: shl(7, 65) = shl(7, 1) = 14
    i64.const 7 i64.const 65 call $shl
    i64.const 14
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 4

    ;; modulo-64 on shr_s: shr_s(-1, 64) = shr_s(-1, 0) = -1
    i64.const -1 i64.const 64 call $shr_s
    i64.const -1
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 5

    ;; Sanity-check the shift counter before moving on to the rotates.
    block local.get $sum  i32.const 5  i32.eq br_if 0 unreachable end

    ;; --- rotates: each block traps on first mismatch ------------------
    ;; rotl(0x123456789ABCDEF0, 8) = 0x3456789ABCDEF012
    block i64.const 0x123456789ABCDEF0 i64.const 8  call $rotl  i64.const 0x3456789ABCDEF012 i64.eq br_if 0 unreachable end
    ;; rotr(0x123456789ABCDEF0, 4) = 0x0123456789ABCDEF
    block i64.const 0x123456789ABCDEF0 i64.const 4  call $rotr  i64.const 0x0123456789ABCDEF i64.eq br_if 0 unreachable end

    ;; n = 1 wrap edges — the bit that falls off the high/low end has to
    ;; show up at the opposite end.
    block i64.const 0x8000000000000000 i64.const 1  call $rotl  i64.const 0x0000000000000001 i64.eq br_if 0 unreachable end
    block i64.const 0x0000000000000001 i64.const 1  call $rotr  i64.const 0x8000000000000000 i64.eq br_if 0 unreachable end

    ;; Half-word rotation — rotl(_, 32) and rotr(_, 32) collapse to the
    ;; same value, so the same expectation catches a swapped opcode.
    block i64.const 0xCAFEBABEDEADBEEF i64.const 32 call $rotl  i64.const 0xDEADBEEFCAFEBABE i64.eq br_if 0 unreachable end
    block i64.const 0xCAFEBABEDEADBEEF i64.const 32 call $rotr  i64.const 0xDEADBEEFCAFEBABE i64.eq br_if 0 unreachable end

    ;; n = 0 — identity. This is the case the JIT's `(bit_count - n) &
    ;; mask` term is structured around: without the second `& mask`, a
    ;; 0-rotation would shift by the full width and lose the value.
    block i64.const 0x123456789ABCDEF0 i64.const 0  call $rotl  i64.const 0x123456789ABCDEF0 i64.eq br_if 0 unreachable end
    block i64.const 0x123456789ABCDEF0 i64.const 0  call $rotr  i64.const 0x123456789ABCDEF0 i64.eq br_if 0 unreachable end

    ;; modulo-64 behavior: rotl(_, 64) = identity, rotl(_, 72) = rotl(_, 8).
    block i64.const 0x123456789ABCDEF0 i64.const 64 call $rotl  i64.const 0x123456789ABCDEF0 i64.eq br_if 0 unreachable end
    block i64.const 0x123456789ABCDEF0 i64.const 72 call $rotl  i64.const 0x3456789ABCDEF012 i64.eq br_if 0 unreachable end
    block i64.const 0xCAFEBABEDEADBEEF i64.const 64 call $rotr  i64.const 0xCAFEBABEDEADBEEF i64.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
