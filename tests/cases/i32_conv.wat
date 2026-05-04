;; Exercises every conversion the JIT supports that produces an i32:
;;   i32.wrap_i64
;;   i32.extend8_s, i32.extend16_s
;; Returns 0 on success.
(module
  (func $wrap     (param i64) (result i32) local.get 0 i32.wrap_i64)
  (func $ext8_s   (param i32) (result i32) local.get 0 i32.extend8_s)
  (func $ext16_s  (param i32) (result i32) local.get 0 i32.extend16_s)

  (func $_start (result i32)
    (local $sum i32)

    ;; wrap(0x123456789ABCDEF0) keeps low 32 bits = 0x9ABCDEF0
    i64.const 0x123456789ABCDEF0
    call $wrap
    i32.const 0x9ABCDEF0
    i32.eq
    local.set $sum                              ;; +1

    ;; extend8_s(0xFF) = -1
    i32.const 0xFF
    call $ext8_s
    i32.const -1
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 2

    ;; extend8_s(0x7F) = 127  (sign bit not set)
    i32.const 0x7F
    call $ext8_s
    i32.const 127
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 3

    ;; extend16_s(0x8001) = -32767
    i32.const 0x8001
    call $ext16_s
    i32.const -32767
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                              ;; +1 -> 4

    local.get $sum
    i32.const 4
    i32.ne)

  (export "_start" (func $_start)))
