;; Exercises i64.shl / i64.shr_s / i64.shr_u, including the wasm rule that
;; the shift count is taken modulo 64. Returns 0 on success.
(module
  (func $shl   (param i64 i64) (result i64) local.get 0 local.get 1 i64.shl)
  (func $shr_s (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_s)
  (func $shr_u (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_u)

  (func $_start (result i32)
    (local $sum i32)

    ;; shl(1, 40) = 0x10000000000  (i64)
    i64.const 1 i64.const 40 call $shl
    i64.const 0x10000000000
    i64.eq
    local.set $sum                            ;; +1 -> 1

    ;; shr_s(-1024, 4) = -64
    i64.const -1024 i64.const 4 call $shr_s
    i64.const -64
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 2

    ;; shr_u(0x8000000000000000, 1) = 0x4000000000000000
    i64.const 0x8000000000000000 i64.const 1 call $shr_u
    i64.const 0x4000000000000000
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 3

    ;; modulo-64 on the count: shl(7, 65) = shl(7, 1) = 14
    i64.const 7 i64.const 65 call $shl
    i64.const 14
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 4

    ;; modulo-64 on shr_s: shr_s(-1, 64) = shr_s(-1, 0) = -1
    i64.const -1 i64.const 64 call $shr_s
    i64.const -1
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                            ;; +1 -> 5

    local.get $sum
    i32.const 5
    i32.ne)

  (export "_start" (func $_start)))
