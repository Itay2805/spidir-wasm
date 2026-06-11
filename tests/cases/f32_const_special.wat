;; f32.const special-value decoding: +inf, -inf, nan, -0.0, denormal.
;; These verify BUFFER_PULL(float) preserves the exact 32-bit pattern.
;; Signed zero can't be told apart by f32.eq, so probe via 1.0/x = -inf.
;; NaN is detected via x x f32.ne (true only for NaN). Expects $sum == 8.
(module
  (func $_start (result i32)
    (local $sum i32)
    (local $t f32)

    ;; +inf
    f32.const inf f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; -inf
    f32.const -inf f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; +inf and -inf are distinct
    f32.const inf f32.const -inf f32.ne
    local.get $sum i32.add local.set $sum

    ;; nan: a NaN is not equal to itself
    f32.const nan local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; nan with explicit payload still decodes as a NaN
    f32.const nan:0x400000 local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; -0.0: equals +0.0 by f32.eq...
    f32.const -0.0 f32.const 0.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; ...but its sign bit is preserved: 1.0 / -0.0 = -inf
    f32.const 1.0 f32.const -0.0 f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; smallest positive denormal 0x00000001 = 1.4e-45, nonzero & finite
    f32.const 0x1p-149 f32.const 0.0 f32.ne
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
