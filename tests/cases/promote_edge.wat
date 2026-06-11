;; Edge cases for f64.promote_f32 (0xBB): every f32 promotes exactly to f64,
;; NaN preserved, signed zero preserved, infinity preserved. Returns 0 on success.
(module
  (func $promote (param f32) (result f64) local.get 0 f64.promote_f32)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- exactness: a full-mantissa f32 promotes to the identical f64 value ---
    f32.const 0x1.800002p0 call $promote f64.const 0x1.800002p0 f64.eq
    local.set $sum

    ;; smallest f32 subnormal (2^-149) promotes exactly
    f32.const 0x1p-149 call $promote f64.const 0x1p-149 f64.eq
    local.get $sum i32.add local.set $sum

    ;; largest finite f32 promotes exactly
    f32.const 0x1.fffffep127 call $promote f64.const 0x1.fffffep127 f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- NaN in -> NaN out (f64.eq with itself is 0 only for NaN) ---
    f32.const nan call $promote f64.const nan f64.eq i32.eqz
    local.get $sum i32.add local.set $sum

    ;; --- infinity preserved ---
    f32.const inf call $promote f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $promote f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- signed zero preserved ---
    ;; magnitude zero
    f32.const -0.0 call $promote f64.const 0.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; sign of promote(-0.0): 1.0 / -0 = -inf
    f64.const 1.0 f32.const -0.0 call $promote f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    ;; sign of promote(+0.0): 1.0 / +0 = +inf
    f64.const 1.0 f32.const 0.0 call $promote f64.div f64.const inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 9 successful checks
    local.get $sum
    i32.const 9
    i32.ne)

  (export "_start" (func $_start)))
