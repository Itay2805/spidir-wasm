;; Edge: -0.0 must round-trip through local.set/get and local.tee unchanged.
;; .eq cannot tell +0 from -0, so probe the sign via 1.0/x == -inf.
(module
  (func $_start (result i32)
    (local $sum i32)
    (local $f f32)
    (local $d f64)
    f32.const -0.0 local.set $f
    f32.const 1.0 local.get $f f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; tee leaves -0.0 on the stack
    f32.const 1.0
    f32.const -0.0 local.tee $f
    f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; and the copy stored by tee is also -0.0
    f32.const 1.0 local.get $f f32.div f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 local.set $d
    f64.const 1.0 local.get $d f64.div f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 4
    i32.ne)
  (export "_start" (func $_start)))
