;; Edge-case coverage for i32/i64 and/or/xor.
;; Covers: all-zeros, all-ones, sign bit, and the algebraic identities
;;   x&0=0, x&~0=x, x|0=x, x|~0=~0, x^x=0, x^0=x, x^~0=~x,
;; plus full cross-word 64-bit patterns.
;; Returns 0 iff all checks pass.
(module
  (func $i32and (param i32 i32) (result i32) local.get 0 local.get 1 i32.and)
  (func $i32or  (param i32 i32) (result i32) local.get 0 local.get 1 i32.or)
  (func $i32xor (param i32 i32) (result i32) local.get 0 local.get 1 i32.xor)
  (func $i64and (param i64 i64) (result i64) local.get 0 local.get 1 i64.and)
  (func $i64or  (param i64 i64) (result i64) local.get 0 local.get 1 i64.or)
  (func $i64xor (param i64 i64) (result i64) local.get 0 local.get 1 i64.xor)

  (func $_start (result i32)
    (local $sum i32)

    ;; ===== i32.and =====
    ;; x & 0 = 0
    i32.const 0xDEADBEEF i32.const 0 call $i32and i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; x & 0xFFFFFFFF = x  (identity element)
    i32.const 0xDEADBEEF i32.const 0xFFFFFFFF call $i32and i32.const 0xDEADBEEF i32.eq
    local.get $sum i32.add local.set $sum
    ;; sign-bit isolation: 0x80000000 & 0xFFFFFFFF = 0x80000000
    i32.const 0x80000000 i32.const 0xFFFFFFFF call $i32and i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; all-ones & all-ones = all-ones
    i32.const 0xFFFFFFFF i32.const 0xFFFFFFFF call $i32and i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    ;; disjoint masks -> 0
    i32.const 0xAAAAAAAA i32.const 0x55555555 call $i32and i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i32.or =====
    ;; x | 0 = x  (identity element)
    i32.const 0xDEADBEEF i32.const 0 call $i32or i32.const 0xDEADBEEF i32.eq
    local.get $sum i32.add local.set $sum
    ;; x | 0xFFFFFFFF = 0xFFFFFFFF
    i32.const 0xDEADBEEF i32.const 0xFFFFFFFF call $i32or i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    ;; sign bit set via or: 0 | 0x80000000 = 0x80000000
    i32.const 0 i32.const 0x80000000 call $i32or i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; complementary halves cover the full word
    i32.const 0xAAAAAAAA i32.const 0x55555555 call $i32or i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i32.xor =====
    ;; x ^ x = 0  (sign-bit value too)
    i32.const 0x80000000 i32.const 0x80000000 call $i32xor i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; x ^ 0 = x  (identity element)
    i32.const 0xDEADBEEF i32.const 0 call $i32xor i32.const 0xDEADBEEF i32.eq
    local.get $sum i32.add local.set $sum
    ;; x ^ 0xFFFFFFFF = ~x : 0xDEADBEEF ^ FFFFFFFF = 0x21524110
    i32.const 0xDEADBEEF i32.const 0xFFFFFFFF call $i32xor i32.const 0x21524110 i32.eq
    local.get $sum i32.add local.set $sum
    ;; all-ones ^ all-ones = 0
    i32.const 0xFFFFFFFF i32.const 0xFFFFFFFF call $i32xor i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; sign bit toggled by xor with sign mask, no borrow into other bits
    i32.const 0x00000001 i32.const 0x80000000 call $i32xor i32.const 0x80000001 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64.and ===== (cross-word patterns)
    ;; x & 0 = 0
    i64.const 0xDEADBEEFCAFEBABE i64.const 0 call $i64and i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    ;; x & all-ones = x  (identity)
    i64.const 0xDEADBEEFCAFEBABE i64.const 0xFFFFFFFFFFFFFFFF call $i64and i64.const 0xDEADBEEFCAFEBABE i64.eq
    local.get $sum i32.add local.set $sum
    ;; 64-bit sign bit isolation
    i64.const 0x8000000000000000 i64.const 0xFFFFFFFFFFFFFFFF call $i64and i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; mask only the high word; verifies low 32 bits are cleared, no truncation
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0xFFFFFFFF00000000 call $i64and i64.const 0xFFFFFFFF00000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; disjoint 64-bit halves -> 0
    i64.const 0xAAAAAAAAAAAAAAAA i64.const 0x5555555555555555 call $i64and i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64.or =====
    ;; x | 0 = x  (identity)
    i64.const 0xDEADBEEFCAFEBABE i64.const 0 call $i64or i64.const 0xDEADBEEFCAFEBABE i64.eq
    local.get $sum i32.add local.set $sum
    ;; x | all-ones = all-ones
    i64.const 0xDEADBEEFCAFEBABE i64.const 0xFFFFFFFFFFFFFFFF call $i64or i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum
    ;; or only the high word; verifies high 32 bits set without disturbing low
    i64.const 0x00000000DEADBEEF i64.const 0xFFFFFFFF00000000 call $i64or i64.const 0xFFFFFFFFDEADBEEF i64.eq
    local.get $sum i32.add local.set $sum
    ;; complementary 64-bit halves cover the whole word
    i64.const 0xAAAAAAAAAAAAAAAA i64.const 0x5555555555555555 call $i64or i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64.xor =====
    ;; x ^ x = 0  (full 64-bit pattern)
    i64.const 0xDEADBEEFCAFEBABE i64.const 0xDEADBEEFCAFEBABE call $i64xor i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    ;; x ^ 0 = x  (identity)
    i64.const 0xDEADBEEFCAFEBABE i64.const 0 call $i64xor i64.const 0xDEADBEEFCAFEBABE i64.eq
    local.get $sum i32.add local.set $sum
    ;; x ^ all-ones = ~x : ~0xDEADBEEFCAFEBABE = 0x2152411035014541
    i64.const 0xDEADBEEFCAFEBABE i64.const 0xFFFFFFFFFFFFFFFF call $i64xor i64.const 0x2152411035014541 i64.eq
    local.get $sum i32.add local.set $sum
    ;; 64-bit sign bit toggle, isolated to bit 63
    i64.const 0x0000000000000001 i64.const 0x8000000000000000 call $i64xor i64.const 0x8000000000000001 i64.eq
    local.get $sum i32.add local.set $sum
    ;; xor across the word boundary: flips both high and low words independently
    i64.const 0xFFFFFFFF00000000 i64.const 0x00000000FFFFFFFF call $i64xor i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 28 successful checks (i32 5+4+5, i64 5+4+5)
    local.get $sum
    i32.const 28
    i32.ne)

  (export "_start" (func $_start)))
