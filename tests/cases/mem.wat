;; Exercises the full set of wasm memory load/store opcodes the JIT supports:
;;   i32.load / i32.store
;;   i32.load8_s, i32.load8_u, i32.store8
;;   i32.load16_s, i32.load16_u, i32.store16
;;   i64.load / i64.store
;;   i64.load8_s, i64.load8_u, i64.store8
;;   i64.load16_s, i64.load16_u, i64.store16
;;   i64.load32_s, i64.load32_u, i64.store32
;;   f32.load / f32.store
;;   f64.load / f64.store
;; Each round-trip contributes to a running sum that's compared at the end.
;; Returns 0 on success.
(module
  (memory 1)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- i32 round-trip at offset 0 ---
    i32.const 0
    i32.const 0x12345678
    i32.store
    i32.const 0
    i32.load
    local.set $sum                 ;; sum = 0x12345678

    ;; --- i32 narrow stores/loads at offset 4 ---
    i32.const 4
    i32.const 0xFF
    i32.store8
    i32.const 4
    i32.load8_s                    ;; -1
    local.get $sum
    i32.add
    local.set $sum

    i32.const 4
    i32.load8_u                    ;; 255
    local.get $sum
    i32.add
    local.set $sum

    i32.const 8
    i32.const 0x8001
    i32.store16
    i32.const 8
    i32.load16_s                   ;; -32767
    local.get $sum
    i32.add
    local.set $sum

    i32.const 8
    i32.load16_u                   ;; 32769
    local.get $sum
    i32.add
    local.set $sum

    ;; --- i64 round-trip at offset 16 ---
    i32.const 16
    i64.const 0x0011223344556677
    i64.store
    i32.const 16
    i64.load
    i64.const 0x0011223344556677
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- i64 byte-narrow store + sign/zero-extending loads at offset 32 ---
    i32.const 32
    i64.const 0xFF
    i64.store8
    i32.const 32
    i64.load8_s
    i64.const -1
    i64.eq                         ;; +1 (sign-extend to i64)
    local.get $sum
    i32.add
    local.set $sum

    i32.const 32
    i64.load8_u
    i64.const 255
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- i64 halfword-narrow store + loads at offset 40 ---
    i32.const 40
    i64.const 0x8001
    i64.store16
    i32.const 40
    i64.load16_s
    i64.const -32767
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    i32.const 40
    i64.load16_u
    i64.const 32769
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- i64 word-narrow store + loads at offset 48 ---
    i32.const 48
    i64.const 0x80000001
    i64.store32
    i32.const 48
    i64.load32_s
    i64.const -2147483647          ;; 0xFFFFFFFF80000001
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    i32.const 48
    i64.load32_u
    i64.const 2147483649           ;; 0x80000001
    i64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- f32 round-trip at offset 56 ---
    i32.const 56
    f32.const 3.5
    f32.store
    i32.const 56
    f32.load
    f32.const 3.5
    f32.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- f64 round-trip at offset 64 ---
    i32.const 64
    f64.const 12345.6789
    f64.store
    i32.const 64
    f64.load
    f64.const 12345.6789
    f64.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- non-zero memarg offset: covers the offset-add path in the JIT.
    ;; store at base 70 + immediate 6 = 76; read back from 76 directly.
    i32.const 70
    i32.const 0xCAFEBABE
    i32.store offset=6
    i32.const 76
    i32.load                        ;; read back via direct addr
    i32.const 0xCAFEBABE
    i32.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; --- non-zero memarg on load: store at 86, load with base 80 + imm 6
    i32.const 86
    i32.const 0xDEADBEEF
    i32.store
    i32.const 80
    i32.load offset=6              ;; effective addr 86
    i32.const 0xDEADBEEF
    i32.eq                         ;; +1
    local.get $sum
    i32.add
    local.set $sum

    ;; expected total:
    ;;   0x12345678  =     305419896
    ;; +    -1       =>    305419895
    ;; +   255       =>    305420150
    ;; + -32767      =>    305387383
    ;; + 32769       =>    305420152
    ;; +     1       =>    305420153   (i64 round-trip)
    ;; +     8 * 1   =>    305420161   (8 narrow / fp round-trips)
    ;; +     2 * 1   =>    305420163   (2 non-zero memarg offset checks)
    local.get $sum
    i32.const 305420163
    i32.ne)

  (export "_start" (func $_start)))
