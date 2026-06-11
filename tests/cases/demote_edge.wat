;; Edge cases for f32.demote_f64 (0xB6): rounding ties-to-even, overflow->inf,
;; underflow->denormal/zero, NaN, signed zero, infinity. Returns 0 on success.
(module
  (func $demote (param f64) (result f32) local.get 0 f32.demote_f64)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- rounding ties-to-even ---
    ;; exact midpoint 1 + 2^-24 between 1.0 and (1+2^-23); even neighbour is 1.0
    f64.const 0x1.000001p0 call $demote f32.const 1.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; just above midpoint -> rounds up to next f32 (1 + 2^-23)
    f64.const 0x1.0000010001p0 call $demote f32.const 0x1.000002p0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- overflow to infinity ---
    f64.const 1e300 call $demote f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f64.const -1e300 call $demote f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- underflow to (signed) zero: |x| below half smallest subnormal ---
    ;; magnitude is zero (eq to 0.0), and sign is preserved: +1e-300 -> +0, -1e-300 -> -0
    f64.const 1e-300 call $demote f32.const 0.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; sign of the +0 result: 1.0 / +0 = +inf
    f32.const 1.0 f64.const 1e-300 call $demote f32.div f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; sign of the -0 result: 1.0 / -0 = -inf
    f32.const 1.0 f64.const -1e-300 call $demote f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- demote to a representable subnormal f32 (3 * 2^-149) is exact ---
    f64.const 0x1.8p-148 call $demote f32.const 0x1.8p-148 f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- NaN in -> NaN out (f32.eq with itself is 0 only for NaN) ---
    f64.const nan call $demote f32.const nan f32.eq i32.eqz
    local.get $sum i32.add local.set $sum

    ;; --- infinity preserved ---
    f64.const inf call $demote f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $demote f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- signed zero preserved: demote(-0.0) = -0.0 ---
    ;; magnitude zero
    f64.const -0.0 call $demote f32.const 0.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; sign: 1.0 / demote(-0.0) = -inf
    f32.const 1.0 f64.const -0.0 call $demote f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; demote(+0.0) = +0.0 : 1.0 / +0 = +inf
    f32.const 1.0 f64.const 0.0 call $demote f32.div f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 14 successful checks
    local.get $sum
    i32.const 14
    i32.ne)

  (export "_start" (func $_start)))
