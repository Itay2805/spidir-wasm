;; Exercises wasm i32.and / i32.or / i32.xor through real call boundaries.
;; Returns 0 on success.
(module
  (func $and (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.and)

  (func $or (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.or)

  (func $xor (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.xor)

  (func $_start (result i32)
    ;; a = 0xFE & 0x4F = 0x4E
    ;; b = 0x10 | 0x07 = 0x17
    ;; c = 0xAA ^ 0xFF = 0x55
    ;; result = a ^ b ^ c = 0x0C = 12
    i32.const 0xFE
    i32.const 0x4F
    call $and
    i32.const 0x10
    i32.const 0x07
    call $or
    call $xor
    i32.const 0xAA
    i32.const 0xFF
    call $xor
    call $xor
    i32.const 12
    i32.ne)

  (export "_start" (func $_start)))
