;; Edge: storing -0.0 / +inf / NaN into a mutable f32/f64 global must preserve
;; the exact bit pattern through the store+load. .eq cannot see the sign of
;; zero, so probe via 1.0/x; detect NaN via x x f32.ne.
(module
  (global $gf (mut f32) (f32.const 0))
  (global $gd (mut f64) (f64.const 0))
  (func $_start (result i32)
    (local $sum i32)
    f32.const -0.0 global.set $gf
    f32.const 1.0 global.get $gf f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 global.set $gd
    f64.const 1.0 global.get $gd f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f32.const inf global.set $gf
    global.get $gf f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const nan global.set $gf
    global.get $gf global.get $gf f32.ne
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 4
    i32.ne)
  (export "_start" (func $_start)))
