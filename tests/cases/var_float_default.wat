;; SPEC: an unwritten declared local has its type's default value; for f32/f64
;; that is +0.0. Reading an unwritten f32/f64 local must yield +0.0.
;; Probe sign via 1.0/x: +0.0 -> +inf.
;; NOTE: currently CRASHES the JIT (function.c:178 builds an iconst with a
;; float type -> invalid IR). See suspected bug. This is the regression test.
(module
  (func $f32_default (result f32)
    (local $x f32)
    local.get $x)
  (func $f64_default (result f64)
    (local $x f64)
    local.get $x)
  (func $_start (result i32)
    (local $sum i32)
    ;; f32 default compares equal to +0.0 (or -0.0) by .eq
    f32.const 0.0 call $f32_default f32.eq
    local.get $sum i32.add local.set $sum
    ;; and 1.0/default == +inf (confirms it is +0.0, not -0.0 or garbage)
    f32.const 1.0 call $f32_default f32.div f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; f64 default == +0.0
    f64.const 0.0 call $f64_default f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 1.0 call $f64_default f64.div f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 4
    i32.ne)
  (export "_start" (func $_start)))
