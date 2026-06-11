;; Edge cases for int->float conversions NOT covered by f32_conv.wat / f64_conv.wat:
;;   - rounding when the integer exceeds float mantissa precision (ties-to-even)
;;   - i64_u / i32_u with the top bit set (must be treated unsigned, not signed)
;;   - 2^64-1 rounding up to 2^64
;;   - negative signed ties-to-even
;;   - signed/unsigned giving opposite-sign results for the same bit pattern
;; Returns 0 on success.
(module
  (func $f32_i32_s (param i32) (result f32) local.get 0 f32.convert_i32_s)
  (func $f32_i32_u (param i32) (result f32) local.get 0 f32.convert_i32_u)
  (func $f64_i32_u (param i32) (result f64) local.get 0 f64.convert_i32_u)
  (func $f32_i64_s (param i64) (result f32) local.get 0 f32.convert_i64_s)
  (func $f32_i64_u (param i64) (result f32) local.get 0 f32.convert_i64_u)
  (func $f64_i64_s (param i64) (result f64) local.get 0 f64.convert_i64_s)
  (func $f64_i64_u (param i64) (result f64) local.get 0 f64.convert_i64_u)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- rounding: integer exceeds f32 24-bit mantissa, round-to-nearest-even ---
    ;; 0x7FFFFFFF = 2147483647 rounds up to 2147483648.0
    i32.const 2147483647 call $f32_i32_s f32.const 2147483648.0 f32.eq
    local.set $sum
    ;; 16777217 (2^24+1) ties down to even 16777216.0
    i32.const 16777217 call $f32_i32_u f32.const 16777216.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; 16777219 (2^24+3) ties up to even 16777220.0
    i32.const 16777219 call $f32_i32_u f32.const 16777220.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; negative tie: -16777217 ties to even -16777216.0
    i32.const -16777217 call $f32_i32_s f32.const -16777216.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- f64 rounding past 53-bit mantissa, ties-to-even ---
    ;; 2^53+1 = 9007199254740993 ties down to 9007199254740992.0
    i64.const 9007199254740993 call $f64_i64_u f64.const 9007199254740992.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; 2^53+3 = 9007199254740995 ties up to 9007199254740996.0
    i64.const 9007199254740995 call $f64_i64_u f64.const 9007199254740996.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; f64.convert_i64_s(0x7FFFFFFFFFFFFFFF) rounds up to 2^63
    i64.const 9223372036854775807 call $f64_i64_s f64.const 9223372036854775808.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- top bit set must be UNSIGNED for the _u forms ---
    ;; i32: 0x80000000 unsigned = 2147483648.0
    i32.const 0x80000000 call $f64_i32_u f64.const 2147483648.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; i64: 0x8000000000000000 unsigned = 2^63 = 9223372036854775808.0
    i64.const 0x8000000000000000 call $f32_i64_u f32.const 9223372036854775808.0 f32.eq
    local.get $sum i32.add local.set $sum
    i64.const 0x8000000000000000 call $f64_i64_u f64.const 9223372036854775808.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- same bits, signed form is negative -2^63 ---
    i64.const 0x8000000000000000 call $f32_i64_s f32.const -9223372036854775808.0 f32.eq
    local.get $sum i32.add local.set $sum
    i64.const 0x8000000000000000 call $f64_i64_s f64.const -9223372036854775808.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- 0xFFFFFFFFFFFFFFFF (u64 max) rounds up to 2^64 in both widths ---
    i64.const -1 call $f32_i64_u f32.const 18446744073709551616.0 f32.eq
    local.get $sum i32.add local.set $sum
    i64.const -1 call $f64_i64_u f64.const 18446744073709551616.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- negative small i64 signed -> exact ---
    i64.const -1 call $f32_i64_s f32.const -1.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; i64 MIN exact in f64
    i64.const -9223372036854775808 call $f64_i64_s f64.const -9223372036854775808.0 f64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum
    i32.const 16
    i32.ne)

  (export "_start" (func $_start)))
