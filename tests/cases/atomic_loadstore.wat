(module
  (memory 1 1 shared)
  (func $_start (result i32)
    (local $sum i32)

    ;; --- i32.atomic round-trip @0 ---
    i32.const 0
    i32.const 0x12345678
    i32.atomic.store
    i32.const 0
    i32.atomic.load
    i32.const 0x12345678
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; --- i64.atomic round-trip @8 ---
    i32.const 8
    i64.const 0x0011223344556677
    i64.atomic.store
    i32.const 8
    i64.atomic.load
    i64.const 0x0011223344556677
    i64.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; --- i32.atomic.store8 / load8_u @16 : narrow store + ZERO-EXTEND load ---
    i32.const 16
    i32.const 0xFF
    i32.atomic.store8
    i32.const 16
    i32.atomic.load8_u             ;; must zero-extend -> 255, never -1
    i32.const 255
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; --- i32.atomic.store16 / load16_u @18 : zero-extend ---
    i32.const 18
    i32.const 0x8001
    i32.atomic.store16
    i32.const 18
    i32.atomic.load16_u            ;; zero-extend -> 32769, never negative
    i32.const 32769
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; --- store8 writes ONLY one byte: high bytes stay 0 ---
    i32.const 24
    i32.const 0
    i32.atomic.store
    i32.const 24
    i32.const 0xAB
    i32.atomic.store8
    i32.const 24
    i32.atomic.load
    i32.const 0xAB
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; --- non-zero memarg offset round-trip ---
    i32.const 28
    i32.const 0xCAFEBABE
    i32.atomic.store offset=4      ;; effective addr 32
    i32.const 32
    i32.atomic.load
    i32.const 0xCAFEBABE
    i32.eq
    local.get $sum
    i32.add
    local.set $sum                 ;; +1

    ;; expect 6 passing checks
    local.get $sum
    i32.const 6
    i32.ne)
  (export "_start" (func $_start)))
