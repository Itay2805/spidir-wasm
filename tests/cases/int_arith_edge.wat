;; Edge-case coverage for i32/i64 add/sub/mul: overflow wraparound,
;; underflow, mul overflow (high-bit truncation), identities, negatives.
;; Helpers force the real instruction (no constant folding across call).
;; Returns 0 iff all 22 checks pass.
(module
  (func $add (param i32 i32) (result i32) local.get 0 local.get 1 i32.add)
  (func $sub (param i32 i32) (result i32) local.get 0 local.get 1 i32.sub)
  (func $mul (param i32 i32) (result i32) local.get 0 local.get 1 i32.mul)

  (func $add64 (param i64 i64) (result i64) local.get 0 local.get 1 i64.add)
  (func $sub64 (param i64 i64) (result i64) local.get 0 local.get 1 i64.sub)
  (func $mul64 (param i64 i64) (result i64) local.get 0 local.get 1 i64.mul)

  (func $_start (result i32)
    (local $sum i32)

    ;; ===== i32 ADD =====
    ;; INT_MAX + 1 = INT_MIN (wrap)
    i32.const 0x7fffffff i32.const 1 call $add i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFF + 1 = 0 (wrap to zero)
    i32.const -1 i32.const 1 call $add i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; identity x + 0 = x
    i32.const 12345 i32.const 0 call $add i32.const 12345 i32.eq
    local.get $sum i32.add local.set $sum
    ;; negatives: -5 + -3 = -8
    i32.const -5 i32.const -3 call $add i32.const -8 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i32 SUB =====
    ;; INT_MIN - 1 = INT_MAX (underflow wrap)
    i32.const 0x80000000 i32.const 1 call $sub i32.const 0x7fffffff i32.eq
    local.get $sum i32.add local.set $sum
    ;; 0 - 1 = -1 (0xFFFFFFFF)
    i32.const 0 i32.const 1 call $sub i32.const -1 i32.eq
    local.get $sum i32.add local.set $sum
    ;; identity x - 0 = x
    i32.const 12345 i32.const 0 call $sub i32.const 12345 i32.eq
    local.get $sum i32.add local.set $sum
    ;; x - x = 0
    i32.const -123456 i32.const -123456 call $sub i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i32 MUL =====
    ;; mul overflow with high-bit truncation: 0x10000 * 0x10000 = 2^32 = 0
    i32.const 0x10000 i32.const 0x10000 call $mul i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFF * 0xFFFFFFFF = (2^32-1)^2 mod 2^32 = 1
    i32.const -1 i32.const -1 call $mul i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    ;; INT_MIN * -1 = INT_MIN (0x80000000, since 2^31 mod 2^32 wraps)
    i32.const 0x80000000 i32.const -1 call $mul i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; identity x * 1 = x
    i32.const 0x7fffffff i32.const 1 call $mul i32.const 0x7fffffff i32.eq
    local.get $sum i32.add local.set $sum
    ;; x * 0 = 0
    i32.const 0x7fffffff i32.const 0 call $mul i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    ;; negative * positive: -7 * 9 = -63
    i32.const -7 i32.const 9 call $mul i32.const -63 i32.eq
    local.get $sum i32.add local.set $sum
    ;; mid-range overflow: 0x40000000 * 4 = 2^32 = 0
    i32.const 0x40000000 i32.const 4 call $mul i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64 ADD =====
    ;; INT64_MAX + 1 = INT64_MIN
    i64.const 0x7fffffffffffffff i64.const 1 call $add64 i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; -1 + 1 = 0
    i64.const -1 i64.const 1 call $add64 i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64 SUB =====
    ;; INT64_MIN - 1 = INT64_MAX
    i64.const 0x8000000000000000 i64.const 1 call $sub64 i64.const 0x7fffffffffffffff i64.eq
    local.get $sum i32.add local.set $sum
    ;; 0 - 1 = -1
    i64.const 0 i64.const 1 call $sub64 i64.const -1 i64.eq
    local.get $sum i32.add local.set $sum

    ;; ===== i64 MUL =====
    ;; high-bit truncation: 2^32 * 2^32 = 2^64 = 0
    i64.const 0x100000000 i64.const 0x100000000 call $mul64 i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF = 1 mod 2^64
    i64.const -1 i64.const -1 call $mul64 i64.const 1 i64.eq
    local.get $sum i32.add local.set $sum
    ;; INT64_MIN * -1 = INT64_MIN
    i64.const 0x8000000000000000 i64.const -1 call $mul64 i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 22 checks
    local.get $sum
    i32.const 22
    i32.ne)

  (export "_start" (func $_start)))
