;; Exercises every conversion that produces an f32:
;;   f32.convert_i32_s, f32.convert_i32_u
;;   f32.convert_i64_s, f32.convert_i64_u
;;   f32.demote_f64
;; Returns 0 on success.
(module
  (func $cvt_i32_s (param i32) (result f32) local.get 0 f32.convert_i32_s)
  (func $cvt_i32_u (param i32) (result f32) local.get 0 f32.convert_i32_u)
  (func $cvt_i64_s (param i64) (result f32) local.get 0 f32.convert_i64_s)
  (func $cvt_i64_u (param i64) (result f32) local.get 0 f32.convert_i64_u)
  (func $demote    (param f64) (result f32) local.get 0 f32.demote_f64)

  (func $_start (result i32)
    (local $sum i32)

    ;; convert_i32_s(-256) = -256.0
    i32.const -256
    call $cvt_i32_s
    f32.const -256.0
    f32.eq
    local.set $sum

    ;; convert_i32_u(-1) = 4294967296.0 (treated as 0xFFFFFFFF)
    ;; f32 rounds: 4294967295 -> 4294967296.0 (next f32)
    i32.const -1
    call $cvt_i32_u
    f32.const 4294967296.0
    f32.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; convert_i64_s(1024) = 1024.0
    i64.const 1024
    call $cvt_i64_s
    f32.const 1024.0
    f32.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; convert_i64_u(0) = 0.0
    i64.const 0
    call $cvt_i64_u
    f32.const 0.0
    f32.eq
    local.get $sum
    i32.add
    local.set $sum

    ;; demote_f64(1.5) = 1.5 (exact)
    f64.const 1.5
    call $demote
    f32.const 1.5
    f32.eq
    local.get $sum
    i32.add
    local.set $sum

    local.get $sum
    i32.const 5
    i32.ne)

  (export "_start" (func $_start)))
