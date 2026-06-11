;; Edge-case audit for conv_int_int family.
;; Each check adds 1 to $sum; module returns 0 iff all 20 checks pass.
(module
  (func $wrap     (param i64) (result i32) local.get 0 i32.wrap_i64)
  (func $ext_s    (param i32) (result i64) local.get 0 i64.extend_i32_s)
  (func $ext_u    (param i32) (result i64) local.get 0 i64.extend_i32_u)
  (func $i32ext8  (param i32) (result i32) local.get 0 i32.extend8_s)
  (func $i32ext16 (param i32) (result i32) local.get 0 i32.extend16_s)
  (func $i64ext8  (param i64) (result i64) local.get 0 i64.extend8_s)
  (func $i64ext16 (param i64) (result i64) local.get 0 i64.extend16_s)
  (func $i64ext32 (param i64) (result i64) local.get 0 i64.extend32_s)

  (func $_start (result i32)
    (local $sum i32)

    ;; 1. wrap(0x1_00000000) = 0  (pure high bits dropped)
    i64.const 0x1_00000000
    call $wrap
    i32.eqz
    local.set $sum

    ;; 2. wrap(0xFFFFFFFF_80000000) = -2147483648 (low 32 preserved, sign bit set)
    i64.const 0xFFFFFFFF_80000000
    call $wrap
    i32.const -2147483648
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 3. wrap(0xFFFFFFFF_FFFFFFFF) = -1
    i64.const -1
    call $wrap
    i32.const -1
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 4. extend_i32_s(0x80000000) = -2147483648 (i64)
    i32.const 0x80000000
    call $ext_s
    i64.const -2147483648
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 5. extend_i32_u(0x80000000) = 0x80000000 (NOT sign extended)
    i32.const 0x80000000
    call $ext_u
    i64.const 0x80000000
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 6. extend_i32_u(0) = 0
    i32.const 0
    call $ext_u
    i64.eqz
    local.get $sum i32.add local.set $sum

    ;; 7. i32.extend8_s(0x80) = -128 (exact sign-bit boundary)
    i32.const 0x80
    call $i32ext8
    i32.const -128
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 8. i32.extend8_s(0xFFFFFF7F) = 127 (high bits must be IGNORED, low byte 0x7F)
    i32.const 0xFFFFFF7F
    call $i32ext8
    i32.const 127
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 9. i32.extend8_s(0x12345680) = -128 (low byte 0x80, high garbage ignored)
    i32.const 0x12345680
    call $i32ext8
    i32.const -128
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 10. i32.extend16_s(0x8000) = -32768 (exact i16 min)
    i32.const 0x8000
    call $i32ext16
    i32.const -32768
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 11. i32.extend16_s(0xFFFF7FFF) = 32767 (high bits ignored, low half 0x7FFF)
    i32.const 0xFFFF7FFF
    call $i32ext16
    i32.const 32767
    i32.eq
    local.get $sum i32.add local.set $sum

    ;; 12. i32.extend16_s(0) = 0
    i32.const 0
    call $i32ext16
    i32.eqz
    local.get $sum i32.add local.set $sum

    ;; 13. i64.extend8_s(0x7F) = 127 (positive boundary)
    i64.const 0x7F
    call $i64ext8
    i64.const 127
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 14. i64.extend8_s(0x80) = -128
    i64.const 0x80
    call $i64ext8
    i64.const -128
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 15. i64.extend8_s(0xFFFFFFFF_FFFFFF80) = -128 (high 56 bits ignored)
    i64.const -128
    call $i64ext8
    i64.const -128
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 16. i64.extend16_s(0x8000) = -32768
    i64.const 0x8000
    call $i64ext16
    i64.const -32768
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 17. i64.extend16_s(0x7FFF) = 32767
    i64.const 0x7FFF
    call $i64ext16
    i64.const 32767
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 18. i64.extend32_s(0xFFFFFFFF) = -1 (all ones in low 32)
    i64.const 0xFFFFFFFF
    call $i64ext32
    i64.const -1
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 19. i64.extend32_s(0x7FFFFFFF) = 2147483647 (positive i32 max)
    i64.const 0x7FFFFFFF
    call $i64ext32
    i64.const 2147483647
    i64.eq
    local.get $sum i32.add local.set $sum

    ;; 20. i64.extend32_s(0x12345678_80000000) = -2147483648 (high 32 ignored)
    i64.const 0x12345678_80000000
    call $i64ext32
    i64.const -2147483648
    i64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum
    i32.const 20
    i32.ne)

  (export "_start" (func $_start)))
