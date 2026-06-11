;; float_arith edge cases for f32 add/sub/mul/div.
;; NaN detected via (tee;get;f32.ne) -> i32 1 iff NaN. Signed zero probed via 1/x
;; (1/+0 = +inf, 1/-0 = -inf).
(module
  (func $add (param f32 f32) (result f32) local.get 0 local.get 1 f32.add)
  (func $sub (param f32 f32) (result f32) local.get 0 local.get 1 f32.sub)
  (func $mul (param f32 f32) (result f32) local.get 0 local.get 1 f32.mul)
  (func $div (param f32 f32) (result f32) local.get 0 local.get 1 f32.div)

  (func $_start (result i32)
    (local $sum i32)
    (local $t f32)

    ;; --- division by zero ---
    f32.const 1.0  f32.const 0.0  call $div  f32.const inf  f32.eq
    local.get $sum i32.add local.set $sum                          ;; 1/0 = +inf
    f32.const -1.0 f32.const 0.0  call $div  f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum                          ;; -1/0 = -inf
    f32.const 1.0  f32.const -0.0 call $div  f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum                          ;; 1/-0 = -inf
    f32.const -1.0 f32.const -0.0 call $div  f32.const inf  f32.eq
    local.get $sum i32.add local.set $sum                          ;; -1/-0 = +inf

    ;; --- 0/0 = NaN ---
    f32.const 0.0 f32.const 0.0 call $div local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf - inf = NaN ---
    f32.const inf f32.const inf call $sub local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf + (-inf) = NaN ---
    f32.const inf f32.const -inf call $add local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- inf + inf = +inf ---
    f32.const inf f32.const inf call $add f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- inf * 0 = NaN ---
    f32.const inf f32.const 0.0 call $mul local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- 1 / inf = +0  (probe: 1/(1/inf) = +inf) ---
    f32.const 1.0  f32.const 1.0 f32.const inf call $div  f32.div  f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- NaN propagation: NaN + 1 = NaN ---
    f32.const nan f32.const 1.0 call $add local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- NaN propagation: 1 * NaN = NaN ---
    f32.const 1.0 f32.const nan call $mul local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; --- signed zero: -0 + -0 = -0  (1/(-0) = -inf) ---
    f32.const 1.0  f32.const -0.0 f32.const -0.0 call $add  f32.div  f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- signed zero: +0 + -0 = +0  (1/(+0) = +inf) ---
    f32.const 1.0  f32.const 0.0 f32.const -0.0 call $add  f32.div  f32.const inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- signed zero: 2 * -0 = -0  (1/(2*-0) = -inf) ---
    f32.const 1.0  f32.const 2.0 f32.const -0.0 call $mul  f32.div  f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; --- signed zero: -0 - +0 = -0  (1/(-0 - 0) = -inf) ---
    f32.const 1.0  f32.const -0.0 f32.const 0.0 call $sub  f32.div  f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum

    ;; expected 16 successful checks
    local.get $sum
    i32.const 16
    i32.ne)

  (export "_start" (func $_start)))
