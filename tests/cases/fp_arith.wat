;; Exercises f32 and f64 arithmetic that spidir natively supports:
;; add, sub, mul, div. Each result is checked against an exact-representable
;; expected value via .eq, so the test doesn't depend on rounding. Returns 0
;; on success.
(module
  (func $f32_add (param f32 f32) (result f32) local.get 0 local.get 1 f32.add)
  (func $f32_sub (param f32 f32) (result f32) local.get 0 local.get 1 f32.sub)
  (func $f32_mul (param f32 f32) (result f32) local.get 0 local.get 1 f32.mul)
  (func $f32_div (param f32 f32) (result f32) local.get 0 local.get 1 f32.div)

  (func $f64_add (param f64 f64) (result f64) local.get 0 local.get 1 f64.add)
  (func $f64_sub (param f64 f64) (result f64) local.get 0 local.get 1 f64.sub)
  (func $f64_mul (param f64 f64) (result f64) local.get 0 local.get 1 f64.mul)
  (func $f64_div (param f64 f64) (result f64) local.get 0 local.get 1 f64.div)

  (func $_start (result i32)
    (local $sum i32)

    ;; f32 ops — use exact-representable values (powers of two, small ints)
    f32.const 1.5 f32.const 2.5 call $f32_add f32.const 4.0 f32.eq
    local.set $sum
    f32.const 5.0 f32.const 1.5 call $f32_sub f32.const 3.5 f32.eq
    local.get $sum
    i32.add
    local.set $sum
    f32.const 2.0 f32.const 3.5 call $f32_mul f32.const 7.0 f32.eq
    local.get $sum
    i32.add
    local.set $sum
    f32.const 12.0 f32.const 4.0 call $f32_div f32.const 3.0 f32.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; f64 ops
    f64.const 1.5 f64.const 2.5 call $f64_add f64.const 4.0 f64.eq
    local.get $sum
    i32.add
    local.set $sum
    f64.const 5.0 f64.const 1.5 call $f64_sub f64.const 3.5 f64.eq
    local.get $sum
    i32.add
    local.set $sum
    f64.const 2.0 f64.const 3.5 call $f64_mul f64.const 7.0 f64.eq
    local.get $sum
    i32.add
    local.set $sum
    f64.const 100.0 f64.const 8.0 call $f64_div f64.const 12.5 f64.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; expected: 8 successful comparisons -> 8
    local.get $sum
    i32.const 8
    i32.ne)

  (export "_start" (func $_start)))
