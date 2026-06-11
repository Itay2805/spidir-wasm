;; float_arith edge cases for f64 add/sub/mul/div (opcodes 0xA0-0xA3).
;; NaN detected via (tee;get;f64.ne); signed zero probed via 1/x.
(module
  (func $add (param f64 f64) (result f64) local.get 0 local.get 1 f64.add)
  (func $sub (param f64 f64) (result f64) local.get 0 local.get 1 f64.sub)
  (func $mul (param f64 f64) (result f64) local.get 0 local.get 1 f64.mul)
  (func $div (param f64 f64) (result f64) local.get 0 local.get 1 f64.div)

  (func $_start (result i32)
    (local $sum i32)
    (local $t f64)

    ;; --- division by zero ---
    f64.const 1.0  f64.const 0.0  call $div  f64.const inf  f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.0 f64.const 0.0  call $div  f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0  f64.const -0.0 call $div  f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.0 f64.const -0.0 call $div  f64.const inf  f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- 0/0 = NaN ---
    f64.const 0.0 f64.const 0.0 call $div local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf - inf = NaN ---
    f64.const inf f64.const inf call $sub local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf + (-inf) = NaN ---
    f64.const inf f64.const -inf call $add local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf + inf = +inf ---
    f64.const inf f64.const inf call $add f64.const inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- inf * 0 = NaN ---
    f64.const inf f64.const 0.0 call $mul local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    ;; --- 1 / inf = +0  (probe: 1/(1/inf) = +inf) ---
    f64.const 1.0  f64.const 1.0 f64.const inf call $div  f64.div  f64.const inf f64.eq
    local.get $sum i32.add local.set $sum

    ;; --- NaN propagation ---
    f64.const nan f64.const 1.0 call $add local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum
    f64.const 1.0 f64.const nan call $mul local.tee $t local.get $t f64.ne
    local.get $sum i32.add local.set $sum

    ;; --- signed zero results ---
    f64.const 1.0  f64.const -0.0 f64.const -0.0 call $add  f64.div  f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0  f64.const 0.0 f64.const -0.0 call $add  f64.div  f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0  f64.const 2.0 f64.const -0.0 call $mul  f64.div  f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0  f64.const -0.0 f64.const 0.0 call $sub  f64.div  f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 16 i32.ne)
  (export "_start" (func $_start)))
