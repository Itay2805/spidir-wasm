;; Exercises the f32/f64 min, max and copysign opcodes against the spec
;; (WebAssembly 3.0, 4.3.3 fmin/fmax/fcopysign):
;;   f32.min 0x96, f32.max 0x97, f32.copysign 0x98
;;   f64.min 0xA4, f64.max 0xA5, f64.copysign 0xA6
;;
;; Ordinary inputs/results are exactly representable, so those are exact .eq
;; comparisons. The interesting spec corners are also covered:
;;   - min/max on opposite-sign zeros: min(±0,∓0) = -0, max(±0,∓0) = +0,
;;     order-independent. .eq treats +0 == -0, so the sign is probed via
;;     1.0 / x (= -inf for -0, +inf for +0).
;;   - min/max with a NaN operand must yield a NaN (the result is detected
;;     with x != x, which is 1 only for NaN).
;;   - copysign takes the sign of its second operand (even from a signed zero)
;;     and can produce -0.
;; Returns 0 on success.
(module
  (func $f32_min      (param f32 f32) (result f32) local.get 0 local.get 1 f32.min)
  (func $f32_max      (param f32 f32) (result f32) local.get 0 local.get 1 f32.max)
  (func $f32_copysign (param f32 f32) (result f32) local.get 0 local.get 1 f32.copysign)

  (func $f64_min      (param f64 f64) (result f64) local.get 0 local.get 1 f64.min)
  (func $f64_max      (param f64 f64) (result f64) local.get 0 local.get 1 f64.max)
  (func $f64_copysign (param f64 f64) (result f64) local.get 0 local.get 1 f64.copysign)

  (func $_start (result i32)
    (local $sum i32)   ;; counts successful checks (zero-initialised)
    (local $t f32)     ;; scratch for NaN detection
    (local $td f64)

    ;; ---- f32 ----------------------------------------------------------------

    ;; min — both operand orders, mixed signs
    f32.const 2.0 f32.const 3.0 call $f32_min f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 3.0 f32.const 2.0 call $f32_min f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -1.0 f32.const 1.0 call $f32_min f32.const -1.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; min on opposite-sign zeros is -0 regardless of order (probe 1/-0 = -inf)
    f32.const 1.0 f32.const -0.0 f32.const 0.0 call $f32_min f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 1.0 f32.const 0.0 f32.const -0.0 call $f32_min f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; min with NaN must be NaN
    f32.const nan f32.const 1.0 call $f32_min local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum
    f32.const 1.0 f32.const nan call $f32_min local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; max
    f32.const 2.0 f32.const 3.0 call $f32_max f32.const 3.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 3.0 f32.const 2.0 call $f32_max f32.const 3.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -1.0 f32.const 1.0 call $f32_max f32.const 1.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; max on opposite-sign zeros is +0 regardless of order (probe 1/+0 = +inf)
    f32.const 1.0 f32.const -0.0 f32.const 0.0 call $f32_max f32.div f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 1.0 f32.const 0.0 f32.const -0.0 call $f32_max f32.div f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; max with NaN must be NaN
    f32.const nan f32.const 1.0 call $f32_max local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum
    f32.const 1.0 f32.const nan call $f32_max local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; copysign — magnitude of a, sign of b
    f32.const 3.0 f32.const 1.0 call $f32_copysign f32.const 3.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 3.0 f32.const -1.0 call $f32_copysign f32.const -3.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -3.0 f32.const 1.0 call $f32_copysign f32.const 3.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -3.0 f32.const -1.0 call $f32_copysign f32.const -3.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; sign is taken even from a signed zero: copysign(3, -0) = -3
    f32.const 3.0 f32.const -0.0 call $f32_copysign f32.const -3.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; copysign can produce -0: copysign(0, -1) = -0 (probe 1/-0 = -inf)
    f32.const 1.0 f32.const 0.0 f32.const -1.0 call $f32_copysign f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f64 ----------------------------------------------------------------

    ;; min
    f64.const 2.0 f64.const 3.0 call $f64_min f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.0 f64.const 2.0 call $f64_min f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.0 f64.const 1.0 call $f64_min f64.const -1.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const -0.0 f64.const 0.0 call $f64_min f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const 0.0 f64.const -0.0 call $f64_min f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan f64.const 1.0 call $f64_min local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const nan call $f64_min local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum

    ;; max
    f64.const 2.0 f64.const 3.0 call $f64_max f64.const 3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.0 f64.const 2.0 call $f64_max f64.const 3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.0 f64.const 1.0 call $f64_max f64.const 1.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const -0.0 f64.const 0.0 call $f64_max f64.div f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const 0.0 f64.const -0.0 call $f64_max f64.div f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan f64.const 1.0 call $f64_max local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const nan call $f64_max local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum

    ;; copysign
    f64.const 3.0 f64.const 1.0 call $f64_copysign f64.const 3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.0 f64.const -1.0 call $f64_copysign f64.const -3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -3.0 f64.const 1.0 call $f64_copysign f64.const 3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -3.0 f64.const -1.0 call $f64_copysign f64.const -3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.0 f64.const -0.0 call $f64_copysign f64.const -3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const 0.0 f64.const -1.0 call $f64_copysign f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 40 successful checks -> 40
    local.get $sum
    i32.const 40
    i32.ne)

  (export "_start" (func $_start)))
