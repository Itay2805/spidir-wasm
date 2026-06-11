;; Edge cases for f32/f64 unary float ops NOT covered by fp_unary.wat:
;;   abs(NaN)=NaN, abs(-inf)=+inf, abs(-0)=+0
;;   neg(NaN) stays NaN, neg(+inf)=-inf, neg(-inf)=+inf, neg(-0)=+0
;;   ceil/floor/trunc/nearest of NaN=NaN and of +-inf=+-inf
;;   ceil(-0.5)=-0, floor(0.5)=+0, trunc(0.5)=+0, trunc(-0.5)=-0  (signed zero!)
;;   nearest 3.5->4 (ties to even rounds UP), nearest(-0.4)=-0
;;   sqrt(-1)=NaN, sqrt(-inf)=NaN, sqrt(+inf)=+inf, sqrt(-0)=-0, sqrt(NaN)=NaN
;; Signed zero is probed via 1.0/x (= -inf for -0, +inf for +0); NaN via x!=x.
;; Returns 0 iff all 60 checks pass.
(module
  (func $f32_abs     (param f32) (result f32) local.get 0 f32.abs)
  (func $f32_neg     (param f32) (result f32) local.get 0 f32.neg)
  (func $f32_ceil    (param f32) (result f32) local.get 0 f32.ceil)
  (func $f32_floor   (param f32) (result f32) local.get 0 f32.floor)
  (func $f32_trunc   (param f32) (result f32) local.get 0 f32.trunc)
  (func $f32_nearest (param f32) (result f32) local.get 0 f32.nearest)
  (func $f32_sqrt    (param f32) (result f32) local.get 0 f32.sqrt)

  (func $f64_abs     (param f64) (result f64) local.get 0 f64.abs)
  (func $f64_neg     (param f64) (result f64) local.get 0 f64.neg)
  (func $f64_ceil    (param f64) (result f64) local.get 0 f64.ceil)
  (func $f64_floor   (param f64) (result f64) local.get 0 f64.floor)
  (func $f64_trunc   (param f64) (result f64) local.get 0 f64.trunc)
  (func $f64_nearest (param f64) (result f64) local.get 0 f64.nearest)
  (func $f64_sqrt    (param f64) (result f64) local.get 0 f64.sqrt)

  (func $isnan32 (param f32) (result i32) local.get 0 local.get 0 f32.ne)
  (func $isnan64 (param f64) (result i32) local.get 0 local.get 0 f64.ne)
  (func $isneg0_32 (param f32) (result i32)
    f32.const 1.0 local.get 0 f32.div f32.const -inf f32.eq)
  (func $isneg0_64 (param f64) (result i32)
    f64.const 1.0 local.get 0 f64.div f64.const -inf f64.eq)
  (func $ispos0_32 (param f32) (result i32)
    f32.const 1.0 local.get 0 f32.div f32.const inf f32.eq)
  (func $ispos0_64 (param f64) (result i32)
    f64.const 1.0 local.get 0 f64.div f64.const inf f64.eq)

  (func $_start (result i32)
    (local $sum i32)

    ;; ---- f32 ----
    f32.const nan call $f32_abs call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_abs f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.0 call $f32_abs call $ispos0_32
    local.get $sum i32.add local.set $sum

    f32.const nan call $f32_neg call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_neg f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_neg f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.0 call $f32_neg call $ispos0_32
    local.get $sum i32.add local.set $sum

    f32.const nan call $f32_ceil call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_ceil f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_ceil f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.5 call $f32_ceil call $isneg0_32
    local.get $sum i32.add local.set $sum

    f32.const nan call $f32_floor call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_floor f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_floor f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 0.5 call $f32_floor call $ispos0_32
    local.get $sum i32.add local.set $sum

    f32.const nan call $f32_trunc call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_trunc f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_trunc f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 0.5 call $f32_trunc call $ispos0_32
    local.get $sum i32.add local.set $sum
    f32.const -0.5 call $f32_trunc call $isneg0_32
    local.get $sum i32.add local.set $sum

    f32.const nan call $f32_nearest call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_nearest f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_nearest f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 3.5 call $f32_nearest f32.const 4.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.4 call $f32_nearest call $isneg0_32
    local.get $sum i32.add local.set $sum

    f32.const -1.0 call $f32_sqrt call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const -inf call $f32_sqrt call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const nan call $f32_sqrt call $isnan32
    local.get $sum i32.add local.set $sum
    f32.const inf call $f32_sqrt f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.0 call $f32_sqrt call $isneg0_32
    local.get $sum i32.add local.set $sum

    ;; ---- f64 ----
    f64.const nan call $f64_abs call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_abs f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 call $f64_abs call $ispos0_64
    local.get $sum i32.add local.set $sum

    f64.const nan call $f64_neg call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_neg f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_neg f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 call $f64_neg call $ispos0_64
    local.get $sum i32.add local.set $sum

    f64.const nan call $f64_ceil call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_ceil f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_ceil f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.5 call $f64_ceil call $isneg0_64
    local.get $sum i32.add local.set $sum

    f64.const nan call $f64_floor call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_floor f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_floor f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 0.5 call $f64_floor call $ispos0_64
    local.get $sum i32.add local.set $sum

    f64.const nan call $f64_trunc call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_trunc f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_trunc f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 0.5 call $f64_trunc call $ispos0_64
    local.get $sum i32.add local.set $sum
    f64.const -0.5 call $f64_trunc call $isneg0_64
    local.get $sum i32.add local.set $sum

    f64.const nan call $f64_nearest call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_nearest f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_nearest f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.5 call $f64_nearest f64.const 4.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.4 call $f64_nearest call $isneg0_64
    local.get $sum i32.add local.set $sum

    f64.const -1.0 call $f64_sqrt call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const -inf call $f64_sqrt call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const nan call $f64_sqrt call $isnan64
    local.get $sum i32.add local.set $sum
    f64.const inf call $f64_sqrt f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 call $f64_sqrt call $isneg0_64
    local.get $sum i32.add local.set $sum

    local.get $sum
    i32.const 60
    i32.ne)

  (export "_start" (func $_start)))
