;; Exercises every conversion that produces an f64:
;;   f64.convert_i32_s, f64.convert_i32_u
;;   f64.convert_i64_s, f64.convert_i64_u
;;   f64.promote_f32
;; Returns 0 on success.
(module
  (func $cvt_i32_s (param i32) (result f64) local.get 0 f64.convert_i32_s)
  (func $cvt_i32_u (param i32) (result f64) local.get 0 f64.convert_i32_u)
  (func $cvt_i64_s (param i64) (result f64) local.get 0 f64.convert_i64_s)
  (func $cvt_i64_u (param i64) (result f64) local.get 0 f64.convert_i64_u)
  (func $promote   (param f32) (result f64) local.get 0 f64.promote_f32)

  (func $_start (result i32)
    (local $sum i32)

    ;; convert_i32_s(-1) = -1.0 (exact)
    i32.const -1
    call $cvt_i32_s
    f64.const -1.0
    f64.eq
    local.set $sum

    ;; convert_i32_u(-1) = 4294967295.0 (exact in f64; 32-bit fits)
    i32.const -1
    call $cvt_i32_u
    f64.const 4294967295.0
    f64.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; convert_i64_s(-1024) = -1024.0
    i64.const -1024
    call $cvt_i64_s
    f64.const -1024.0
    f64.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; convert_i64_u(0x100000000) = 4294967296.0 (exact)
    i64.const 0x100000000
    call $cvt_i64_u
    f64.const 4294967296.0
    f64.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; promote_f32(2.5) = 2.5 (exact)
    f32.const 2.5
    call $promote
    f64.const 2.5
    f64.eq
    local.get $sum
    i32.add
    local.set $sum

    local.get $sum
    i32.const 5
    i32.ne)

  (export "_start" (func $_start)))
