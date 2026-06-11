;; f64.const special-value decoding: +inf, -inf, nan, -0.0, denormal.
;; Verifies BUFFER_PULL(double) preserves the exact 64-bit pattern.
;; Signed zero probed via 1.0/x = -inf; NaN via x x f64.ne. Expects $sum == 8.
(module
  (func $_start (result i32)
    (local $sum i32)
    (local $t f64)

    f64.const inf f64.const inf f64.eq
    local.get $sum i32.add local.set $sum

    f64.const -inf f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    f64.const inf f64.const -inf f64.ne
    local.get $sum i32.add local.set $sum

    f64.const nan local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    f64.const nan:0x8000000000000 local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    f64.const -0.0 f64.const 0.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const -0.0 f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; smallest positive denormal 0x0000000000000001 = 4.9e-324, nonzero
    f64.const 0x1p-1074 f64.const 0.0 f64.ne
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
