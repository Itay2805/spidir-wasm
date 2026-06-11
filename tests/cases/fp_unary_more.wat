;; Negative ties-to-even and large-magnitude nearest, complementing the
;; positive ties already in fp_unary.wat. Verifies the tie direction on the
;; negative side: -1.5->-2, -2.5->-2 (even), -3.5->-4, and that an already-
;; integral large value is unchanged. Returns 0 iff all 8 checks pass.
(module
  (func $f32_nearest (param f32) (result f32) local.get 0 f32.nearest)
  (func $f64_nearest (param f64) (result f64) local.get 0 f64.nearest)
  (func $_start (result i32)
    (local $sum i32)
    f32.const -1.5 call $f32_nearest f32.const -2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -2.5 call $f32_nearest f32.const -2.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const -3.5 call $f32_nearest f32.const -4.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 8388608.0 call $f32_nearest f32.const 8388608.0 f32.eq
    local.get $sum i32.add local.set $sum
    f64.const -1.5 call $f64_nearest f64.const -2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -2.5 call $f64_nearest f64.const -2.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -3.5 call $f64_nearest f64.const -4.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 4503599627370496.0 call $f64_nearest f64.const 4503599627370496.0 f64.eq
    local.get $sum i32.add local.set $sum
    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
