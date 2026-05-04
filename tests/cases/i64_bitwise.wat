;; Exercises i64.and / i64.or / i64.xor.
;; Returns 0 on success.
(module
  (func $and (param i64 i64) (result i64) local.get 0 local.get 1 i64.and)
  (func $or  (param i64 i64) (result i64) local.get 0 local.get 1 i64.or)
  (func $xor (param i64 i64) (result i64) local.get 0 local.get 1 i64.xor)

  (func $_start (result i32)
    ;; a = 0xF0F0_F0F0_F0F0_F0F0 & 0x00FF_00FF_00FF_00FF = 0x00F0_00F0_00F0_00F0
    ;; b = 0x1234_5678_0000_0000 | 0x0000_0000_9ABC_DEF0 = 0x1234_5678_9ABC_DEF0
    ;; c = 0xAAAA_AAAA_AAAA_AAAA ^ 0xFFFF_FFFF_FFFF_FFFF = 0x5555_5555_5555_5555
    ;; expected = a ^ b ^ c = 0x4791_03DD_CF19_8B55
    i64.const 0xF0F0F0F0F0F0F0F0
    i64.const 0x00FF00FF00FF00FF
    call $and
    i64.const 0x1234567800000000
    i64.const 0x000000009ABCDEF0
    call $or
    call $xor
    i64.const 0xAAAAAAAAAAAAAAAA
    i64.const 0xFFFFFFFFFFFFFFFF
    call $xor
    call $xor
    i64.const 0x479103DDCF198B55
    i64.ne)

  (export "_start" (func $_start)))
