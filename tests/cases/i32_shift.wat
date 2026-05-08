;; Exercises i32.shl / i32.shr_s / i32.shr_u / i32.rotl / i32.rotr, plus
;; the wasm rule that the shift/rotation count is taken modulo 32. The
;; rotates additionally exercise the `(bit_count - n) & mask` term in the
;; JIT lowering — the second `& mask` is what keeps n=0 from rotating by
;; the full word width. Returns 0 on success.
(module
  (func $shl   (param i32 i32) (result i32) local.get 0 local.get 1 i32.shl)
  (func $shr_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_s)
  (func $shr_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_u)
  (func $rotl  (param i32 i32) (result i32) local.get 0 local.get 1 i32.rotl)
  (func $rotr  (param i32 i32) (result i32) local.get 0 local.get 1 i32.rotr)

  (func $_start (result i32)
    (local $sum i32)

    ;; shl(1, 5) = 32
    i32.const 1   i32.const 5  call $shl
    local.set $sum                            ;; sum = 32

    ;; shr_s(-128, 3) = -16 (sign-extended)
    i32.const -128 i32.const 3  call $shr_s
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 16

    ;; shr_u(0x80000000, 1) = 0x40000000 (logical)
    i32.const 0x80000000 i32.const 1 call $shr_u
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 0x40000010

    ;; modulo-32 behavior: shl(7, 33) = shl(7, 1) = 14
    i32.const 7   i32.const 33 call $shl
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 0x4000001E

    ;; modulo-32 behavior on shr_u: shr_u(0xF0000000, 32) = 0xF0000000
    i32.const 0xF0000000 i32.const 32 call $shr_u
    local.get $sum
    i32.add                                   ;; 0x4000001E + 0xF0000000 = 0x3000001E (wraps)
    local.set $sum

    ;; Sanity-check the shift sum before moving on to the rotate cases.
    block local.get $sum  i32.const 0x3000001E  i32.eq br_if 0 unreachable end

    ;; --- rotates: each block traps on first mismatch ------------------
    ;; rotl(0x12345678, 4) = 0x23456781
    block i32.const 0x12345678 i32.const 4  call $rotl  i32.const 0x23456781 i32.eq br_if 0 unreachable end
    ;; rotr(0x12345678, 4) = 0x81234567
    block i32.const 0x12345678 i32.const 4  call $rotr  i32.const 0x81234567 i32.eq br_if 0 unreachable end

    ;; n = 1 wrap edges — the bit that falls off the high/low end has to
    ;; show up at the opposite end.
    block i32.const 0x80000000 i32.const 1  call $rotl  i32.const 0x00000001 i32.eq br_if 0 unreachable end
    block i32.const 0x00000001 i32.const 1  call $rotr  i32.const 0x80000000 i32.eq br_if 0 unreachable end

    ;; Half-word rotation — rotl(_, 16) and rotr(_, 16) collapse to the
    ;; same value, so the same expectation catches a swapped opcode.
    block i32.const 0xCAFEBABE i32.const 16 call $rotl  i32.const 0xBABECAFE i32.eq br_if 0 unreachable end
    block i32.const 0xCAFEBABE i32.const 16 call $rotr  i32.const 0xBABECAFE i32.eq br_if 0 unreachable end

    ;; n = 0 — identity. This is the case the JIT's `(bit_count - n) &
    ;; mask` term is structured around: without the second `& mask`, a
    ;; 0-rotation would shift by the full width and lose the value.
    block i32.const 0x12345678 i32.const 0  call $rotl  i32.const 0x12345678 i32.eq br_if 0 unreachable end
    block i32.const 0x12345678 i32.const 0  call $rotr  i32.const 0x12345678 i32.eq br_if 0 unreachable end

    ;; modulo-32 behavior: rotl(_, 32) = identity, rotl(_, 36) = rotl(_, 4).
    block i32.const 0x12345678 i32.const 32 call $rotl  i32.const 0x12345678 i32.eq br_if 0 unreachable end
    block i32.const 0x12345678 i32.const 36 call $rotl  i32.const 0x23456781 i32.eq br_if 0 unreachable end
    block i32.const 0xCAFEBABE i32.const 32 call $rotr  i32.const 0xCAFEBABE i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
