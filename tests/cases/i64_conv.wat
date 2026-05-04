;; Exercises every conversion the JIT supports that produces an i64:
;;   i64.extend_i32_s, i64.extend_i32_u
;;   i64.extend8_s, i64.extend16_s, i64.extend32_s
;; Returns 0 on success.
(module
  (func $ext_s   (param i32) (result i64) local.get 0 i64.extend_i32_s)
  (func $ext_u   (param i32) (result i64) local.get 0 i64.extend_i32_u)
  (func $ext8_s  (param i64) (result i64) local.get 0 i64.extend8_s)
  (func $ext16_s (param i64) (result i64) local.get 0 i64.extend16_s)
  (func $ext32_s (param i64) (result i64) local.get 0 i64.extend32_s)

  (func $_start (result i32)
    (local $sum i32)

    ;; extend_i32_s(-1) = -1
    i32.const -1
    call $ext_s
    i64.const -1
    i64.eq
    local.set $sum                              ;; +1

    ;; extend_i32_u(-1) = 0xFFFFFFFF (zero-extended)
    i32.const -1
    call $ext_u
    i64.const 0xFFFFFFFF
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 2

    ;; extend_i32_s(123) = 123
    i32.const 123
    call $ext_s
    i64.const 123
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 3

    ;; extend8_s on i64: take low byte 0xFF -> -1
    i64.const 0x12345678_900000FF
    call $ext8_s
    i64.const -1
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 4

    ;; extend16_s on i64: low halfword 0x8001 -> -32767
    i64.const 0x12345678_90008001
    call $ext16_s
    i64.const -32767
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 5

    ;; extend32_s on i64: low word 0x80000000 -> -2147483648
    i64.const 0x12345678_80000000
    call $ext32_s
    i64.const -2147483648
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 6

    local.get $sum
    i32.const 6
    i32.ne)

  (export "_start" (func $_start)))
