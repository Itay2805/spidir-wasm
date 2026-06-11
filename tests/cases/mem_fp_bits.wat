;; f32/f64 load/store are bit-exact (bytes_nt) and must preserve NaN, infinities
;; and the sign of zero, none of which mem.wat exercises. NaN is detected with
;; the self-inequality trick (x x f.ne == 1 only for NaN); signed zero via 1/x sign.
(module
  (memory 1)

  (func $f32_is_nan (param f32) (result i32)
    local.get 0 local.get 0 f32.ne)
  (func $f64_is_nan (param f64) (result i32)
    local.get 0 local.get 0 f64.ne)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- f32 NaN survives a store/load round trip ---
    i32.const 0  f32.const nan  f32.store
    i32.const 0  f32.load  call $f32_is_nan        ;; 1 if still NaN
    local.get $sum  i32.add  local.set $sum

    ;; --- f64 NaN survives a store/load round trip ---
    i32.const 8  f64.const nan  f64.store
    i32.const 8  f64.load  call $f64_is_nan        ;; 1 if still NaN
    local.get $sum  i32.add  local.set $sum

    ;; --- f32 +inf / -inf survive ---
    i32.const 16  f32.const inf  f32.store
    i32.const 16  f32.load  f32.const inf  f32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 20  f32.const -inf  f32.store
    i32.const 20  f32.load  f32.const -inf  f32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- f32 -0.0 survives with its sign (probe 1.0 / loaded == -inf) ---
    i32.const 24  f32.const -0.0  f32.store
    f32.const 1.0  i32.const 24  f32.load  f32.div  f32.const -inf  f32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- f64 -0.0 survives with its sign ---
    i32.const 32  f64.const -0.0  f64.store
    f64.const 1.0  i32.const 32  f64.load  f64.div  f64.const -inf  f64.eq
    local.get $sum  i32.add  local.set $sum

    ;; total = 6
    local.get $sum
    i32.const 6
    i32.ne)

  (export "_start" (func $_start)))
