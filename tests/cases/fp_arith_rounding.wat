;; Rounding: results that are NOT exactly representable must round to nearest,
;; ties-to-even. fp_arith.wat uses only exact values, so rounding is untested.
;; 0.1 + 0.2 in f64 = 0x3FD3333333333334 (0.30000000000000004), one ULP above 0.3.
;; 1/3 in f32 = 0x3EAAAAAB; 2/3 in f64 = 0x3FE5555555555555 (bit patterns verified).
(module
  (func $f64add (param f64 f64) (result f64) local.get 0 local.get 1 f64.add)
  (func $f32div (param f32 f32) (result f32) local.get 0 local.get 1 f32.div)
  (func $f64div (param f64 f64) (result f64) local.get 0 local.get 1 f64.div)

  (func $_start (result i32)
    (local $sum i32)

    ;; 0.1 + 0.2 rounds to 0.30000000000000004, NOT 0.3
    f64.const 0.1 f64.const 0.2 call $f64add f64.const 0.30000000000000004 f64.eq
    local.get $sum i32.add local.set $sum
    ;; and it is NOT bit-equal to 0.3
    f64.const 0.1 f64.const 0.2 call $f64add f64.const 0.3 f64.ne
    local.get $sum i32.add local.set $sum

    ;; 1.0 / 3.0 in f32 rounds to 0x3eaaaaab
    f32.const 1.0 f32.const 3.0 call $f32div f32.const 0x1.555556p-2 f32.eq
    local.get $sum i32.add local.set $sum

    ;; 2.0 / 3.0 in f64 rounds to 0x3fe5555555555555
    f64.const 2.0 f64.const 3.0 call $f64div f64.const 0x1.5555555555555p-1 f64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 4 i32.ne)
  (export "_start" (func $_start)))
