;; Exercises i32.shl / i32.shr_s / i32.shr_u, including the wasm rule that
;; the shift count is taken modulo 32. Returns 0 on success.
(module
  (func $shl   (param i32 i32) (result i32) local.get 0 local.get 1 i32.shl)
  (func $shr_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_s)
  (func $shr_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_u)

  (func $_start (result i32)
    (local $sum i32)

    ;; shl(1, 5) = 32
    i32.const 1   i32.const 5  call $shl
    local.set $sum                            ;; sum = 32

    ;; shr_s(-128, 3) = -16 (sign-extended)
    i32.const -128 i32.const 3  call $shr_s
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 16

    ;; shr_u(0x80000000, 1) = 0x40000000 (logical)
    i32.const 0x80000000 i32.const 1 call $shr_u
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 0x40000010

    ;; modulo-32 behavior: shl(7, 33) = shl(7, 1) = 14
    i32.const 7   i32.const 33 call $shl
    local.get $sum
    i32.add
    local.set $sum                            ;; sum = 0x4000001E

    ;; modulo-32 behavior on shr_u: shr_u(0xF0000000, 32) = 0xF0000000
    i32.const 0xF0000000 i32.const 32 call $shr_u
    local.get $sum
    i32.add                                   ;; 0x4000001E + 0xF0000000 = 0x3000001E (wraps)
    local.set $sum

    local.get $sum
    i32.const 0x3000001E
    i32.ne)

  (export "_start" (func $_start)))
