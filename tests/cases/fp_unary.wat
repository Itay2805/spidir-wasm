;; Exercises every f32/f64 unary float opcode that the JIT implements:
;;   abs, neg, ceil, floor, trunc, nearest, sqrt
;; (f32: 0x8B..0x91, f64: 0x99..0x9F)
;;
;; All inputs and expected results are chosen to be exactly representable, so
;; every check is an exact .eq comparison that doesn't depend on rounding.
;; `nearest` is round-to-nearest, ties-to-even, so the .5 ties round to the
;; even neighbour (0.5->0, 1.5->2, 2.5->2). Returns 0 on success.
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

  (func $_start (result i32)
    (local $sum i32)   ;; counts successful checks (zero-initialised)

    ;; ---- f32 ----------------------------------------------------------------

    ;; abs
    f32.const -3.5 call $f32_abs f32.const 3.5 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 3.5 call $f32_abs f32.const 3.5 f32.eq
    local.get $sum i32.add local.set $sum

    ;; neg
    f32.const 2.5 call $f32_neg f32.const -2.5 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -4.0 call $f32_neg f32.const 4.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; neg is a true sign flip, so neg(0.0) = -0.0. .eq can't tell +0 from -0,
    ;; so probe the sign via 1.0 / -0.0 = -inf (1.0 / +0.0 would be +inf).
    f32.const 1.0 f32.const 0.0 call $f32_neg f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; ceil
    f32.const 1.5 call $f32_ceil f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -1.5 call $f32_ceil f32.const -1.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; floor
    f32.const 1.5 call $f32_floor f32.const 1.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -1.5 call $f32_floor f32.const -2.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; trunc
    f32.const 1.5 call $f32_trunc f32.const 1.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -1.5 call $f32_trunc f32.const -1.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; nearest (ties to even)
    f32.const 0.5 call $f32_nearest f32.const 0.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 1.5 call $f32_nearest f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 2.5 call $f32_nearest f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; sqrt
    f32.const 4.0 call $f32_sqrt f32.const 2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 2.25 call $f32_sqrt f32.const 1.5 f32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f64 ----------------------------------------------------------------

    ;; abs
    f64.const -3.5 call $f64_abs f64.const 3.5 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 3.5 call $f64_abs f64.const 3.5 f64.eq
    local.get $sum i32.add local.set $sum

    ;; neg
    f64.const 2.5 call $f64_neg f64.const -2.5 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -4.0 call $f64_neg f64.const 4.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; neg(0.0) = -0.0, probed via 1.0 / -0.0 = -inf (see f32 note above).
    f64.const 1.0 f64.const 0.0 call $f64_neg f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; ceil
    f64.const 1.5 call $f64_ceil f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.5 call $f64_ceil f64.const -1.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; floor
    f64.const 1.5 call $f64_floor f64.const 1.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.5 call $f64_floor f64.const -2.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; trunc
    f64.const 1.5 call $f64_trunc f64.const 1.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.5 call $f64_trunc f64.const -1.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; nearest (ties to even)
    f64.const 0.5 call $f64_nearest f64.const 0.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.5 call $f64_nearest f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 2.5 call $f64_nearest f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; sqrt
    f64.const 9.0 call $f64_sqrt f64.const 3.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 6.25 call $f64_sqrt f64.const 2.5 f64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 32 successful checks -> 32
    local.get $sum
    i32.const 32
    i32.ne)

  (export "_start" (func $_start)))
