;; Exercises wasm i32.and / i32.or / i32.xor through real call boundaries,
;; plus the unary bit-counting ops i32.clz / i32.ctz / i32.popcnt.
;; Returns 0 on success.
(module
  (func $and (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.and)

  (func $or (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.or)

  (func $xor (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.xor)

  (func $clz    (param i32) (result i32) local.get 0 i32.clz)
  (func $ctz    (param i32) (result i32) local.get 0 i32.ctz)
  (func $popcnt (param i32) (result i32) local.get 0 i32.popcnt)

  (func $_start (result i32)
    ;; a = 0xFE & 0x4F = 0x4E
    ;; b = 0x10 | 0x07 = 0x17
    ;; c = 0xAA ^ 0xFF = 0x55
    ;; result = a ^ b ^ c = 0x0C = 12
    block
      i32.const 0xFE
      i32.const 0x4F
      call $and
      i32.const 0x10
      i32.const 0x07
      call $or
      call $xor
      i32.const 0xAA
      i32.const 0xFF
      call $xor
      call $xor
      i32.const 12
      i32.eq br_if 0 unreachable
    end

    ;; --- clz: count leading zeros (most-significant zero bits) --------
    ;; Spec: clz(0) returns the bit width.
    block i32.const 0          call $clz     i32.const 32 i32.eq br_if 0 unreachable end
    block i32.const 1          call $clz     i32.const 31 i32.eq br_if 0 unreachable end
    block i32.const 0x80000000 call $clz     i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const 0x00FF0000 call $clz     i32.const 8  i32.eq br_if 0 unreachable end
    block i32.const 0xFFFFFFFF call $clz     i32.const 0  i32.eq br_if 0 unreachable end

    ;; --- ctz: count trailing zeros (least-significant zero bits) ------
    ;; Spec: ctz(0) returns the bit width.
    block i32.const 0          call $ctz     i32.const 32 i32.eq br_if 0 unreachable end
    block i32.const 1          call $ctz     i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const 0x80000000 call $ctz     i32.const 31 i32.eq br_if 0 unreachable end
    block i32.const 0x00010000 call $ctz     i32.const 16 i32.eq br_if 0 unreachable end
    block i32.const 0xFFFFFFFF call $ctz     i32.const 0  i32.eq br_if 0 unreachable end

    ;; --- popcnt: number of set bits -----------------------------------
    block i32.const 0          call $popcnt  i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const 0xFFFFFFFF call $popcnt  i32.const 32 i32.eq br_if 0 unreachable end
    ;; Alternating-bit patterns — catch a swapped 0x55 / 0xAA mask in the
    ;; popcnt lowering (each must hit exactly half the bits).
    block i32.const 0xAAAAAAAA call $popcnt  i32.const 16 i32.eq br_if 0 unreachable end
    block i32.const 0x55555555 call $popcnt  i32.const 16 i32.eq br_if 0 unreachable end
    ;; Both endpoints set, nothing in between.
    block i32.const 0x80000001 call $popcnt  i32.const 2  i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
