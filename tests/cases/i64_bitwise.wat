;; Exercises i64.and / i64.or / i64.xor, plus the unary bit-counting ops
;; i64.clz / i64.ctz / i64.popcnt.
;; Returns 0 on success.
(module
  (func $and (param i64 i64) (result i64) local.get 0 local.get 1 i64.and)
  (func $or  (param i64 i64) (result i64) local.get 0 local.get 1 i64.or)
  (func $xor (param i64 i64) (result i64) local.get 0 local.get 1 i64.xor)

  (func $clz    (param i64) (result i64) local.get 0 i64.clz)
  (func $ctz    (param i64) (result i64) local.get 0 i64.ctz)
  (func $popcnt (param i64) (result i64) local.get 0 i64.popcnt)

  (func $_start (result i32)
    ;; a = 0xF0F0_F0F0_F0F0_F0F0 & 0x00FF_00FF_00FF_00FF = 0x00F0_00F0_00F0_00F0
    ;; b = 0x1234_5678_0000_0000 | 0x0000_0000_9ABC_DEF0 = 0x1234_5678_9ABC_DEF0
    ;; c = 0xAAAA_AAAA_AAAA_AAAA ^ 0xFFFF_FFFF_FFFF_FFFF = 0x5555_5555_5555_5555
    ;; expected = a ^ b ^ c = 0x4791_03DD_CF19_8B55
    block
      i64.const 0xF0F0F0F0F0F0F0F0
      i64.const 0x00FF00FF00FF00FF
      call $and
      i64.const 0x1234567800000000
      i64.const 0x000000009ABCDEF0
      call $or
      call $xor
      i64.const 0xAAAAAAAAAAAAAAAA
      i64.const 0xFFFFFFFFFFFFFFFF
      call $xor
      call $xor
      i64.const 0x479103DDCF198B55
      i64.eq br_if 0 unreachable
    end

    ;; --- clz: count leading zeros (most-significant zero bits) --------
    ;; Spec: clz(0) returns the bit width.
    block i64.const 0                  call $clz     i64.const 64 i64.eq br_if 0 unreachable end
    block i64.const 1                  call $clz     i64.const 63 i64.eq br_if 0 unreachable end
    block i64.const 0x8000000000000000 call $clz     i64.const 0  i64.eq br_if 0 unreachable end
    ;; Word boundary — verifies the i64 lowering doesn't truncate to 32.
    block i64.const 0x0000000080000000 call $clz     i64.const 32 i64.eq br_if 0 unreachable end
    block i64.const 0xFFFFFFFFFFFFFFFF call $clz     i64.const 0  i64.eq br_if 0 unreachable end

    ;; --- ctz: count trailing zeros (least-significant zero bits) ------
    ;; Spec: ctz(0) returns the bit width.
    block i64.const 0                  call $ctz     i64.const 64 i64.eq br_if 0 unreachable end
    block i64.const 1                  call $ctz     i64.const 0  i64.eq br_if 0 unreachable end
    block i64.const 0x8000000000000000 call $ctz     i64.const 63 i64.eq br_if 0 unreachable end
    ;; Word boundary — same purpose as the clz case above.
    block i64.const 0x0000000100000000 call $ctz     i64.const 32 i64.eq br_if 0 unreachable end
    block i64.const 0xFFFFFFFFFFFFFFFF call $ctz     i64.const 0  i64.eq br_if 0 unreachable end

    ;; --- popcnt: number of set bits -----------------------------------
    block i64.const 0                  call $popcnt  i64.const 0  i64.eq br_if 0 unreachable end
    block i64.const 0xFFFFFFFFFFFFFFFF call $popcnt  i64.const 64 i64.eq br_if 0 unreachable end
    ;; Alternating-bit patterns — must each hit exactly half the bits.
    block i64.const 0xAAAAAAAAAAAAAAAA call $popcnt  i64.const 32 i64.eq br_if 0 unreachable end
    block i64.const 0x5555555555555555 call $popcnt  i64.const 32 i64.eq br_if 0 unreachable end
    ;; Both endpoints set, nothing in between.
    block i64.const 0x8000000000000001 call $popcnt  i64.const 2  i64.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
