;; Subtler IEEE sign rules for f32/f64 sub & mul that the spec pins exactly:
;;  fsub(x,x)=+0 (same value -> positive zero), fsub(-0,-0)=+0 (equal-sign zeroes),
;;  fsub(+0,-0)=+0, fmul infinity signs.
(module
  (func $f32sub (param f32 f32) (result f32) local.get 0 local.get 1 f32.sub)
  (func $f32mul (param f32 f32) (result f32) local.get 0 local.get 1 f32.mul)
  (func $f64sub (param f64 f64) (result f64) local.get 0 local.get 1 f64.sub)
  (func $f64mul (param f64 f64) (result f64) local.get 0 local.get 1 f64.mul)

  (func $_start (result i32)
    (local $sum i32)

    ;; f32: 3.0 - 3.0 = +0  => 1/(+0) = +inf
    f32.const 1.0  f32.const 3.0 f32.const 3.0 call $f32sub  f32.div  f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; f32: -0 - -0 = +0  => 1/(+0) = +inf
    f32.const 1.0  f32.const -0.0 f32.const -0.0 call $f32sub  f32.div  f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; f32: +0 - -0 = +0  => +inf
    f32.const 1.0  f32.const 0.0 f32.const -0.0 call $f32sub  f32.div  f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; f32: inf * -1 = -inf
    f32.const inf f32.const -1.0 call $f32mul f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; f32: -inf * -inf = +inf
    f32.const -inf f32.const -inf call $f32mul f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; f64: 3.0 - 3.0 = +0
    f64.const 1.0  f64.const 3.0 f64.const 3.0 call $f64sub  f64.div  f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    ;; f64: -0 - -0 = +0
    f64.const 1.0  f64.const -0.0 f64.const -0.0 call $f64sub  f64.div  f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    ;; f64: +0 - -0 = +0
    f64.const 1.0  f64.const 0.0 f64.const -0.0 call $f64sub  f64.div  f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    ;; f64: inf * -1 = -inf
    f64.const inf f64.const -1.0 call $f64mul f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    ;; f64: -inf * -inf = +inf
    f64.const -inf f64.const -inf call $f64mul f64.const inf f64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 10 i32.ne)
  (export "_start" (func $_start)))
