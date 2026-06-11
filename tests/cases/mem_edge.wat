;; Edge cases for memory load/store NOT covered by mem.wat:
;;  - narrow stores truncate the stored value to low bytes (i32.store8 of 0x1234 -> 0x34)
;;  - narrow stores do NOT clobber adjacent bytes
;;  - little-endian byte ordering (store i32, read individual bytes)
;;  - i64.store32 truncates high 32 bits
;;  - load8_s of high-bit-clear value is positive (0x7F -> 127)
;;  - load16_s of 0xFFFF -> -1 ; load32_s of 0xFFFFFFFF -> -1 (full-width sign)
;;  - store8/store16 of a negative i32 writes the low bytes (0xFF / 0xFFFF)
;;  - dynamic address combined with static offset
;; Returns 0 on success.
(module
  (memory 1)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- narrow store truncation: i32.store8 writes only low byte ---
    i32.const 0  i32.const 0  i32.store
    i32.const 0  i32.const 0x1234  i32.store8   ;; should write 0x34 only
    i32.const 0  i32.load                        ;; whole word should be 0x00000034
    i32.const 0x34  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- narrow store does not clobber neighbours ---
    ;; write 0xAABBCCDD at 4, then store8 0x00 at byte 5, expect 0xAABB00DD
    i32.const 4  i32.const 0xAABBCCDD  i32.store
    i32.const 5  i32.const 0x00  i32.store8
    i32.const 4  i32.load
    i32.const 0xAABB00DD  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- little-endian: store 0x12345678 at 8, byte[8]=0x78, byte[11]=0x12 ---
    i32.const 8  i32.const 0x12345678  i32.store
    i32.const 8  i32.load8_u
    i32.const 0x78  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 9  i32.load8_u
    i32.const 0x56  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 10 i32.load8_u
    i32.const 0x34  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 11 i32.load8_u
    i32.const 0x12  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- i64.store32 truncates high 32 bits ---
    i32.const 16  i64.const 0x1122334455667788  i64.store32
    i32.const 16  i64.load32_u
    i64.const 0x55667788  i64.eq
    local.get $sum  i32.add  local.set $sum
    ;; and the bytes at 20..23 must be untouched (fresh mem -> 0)
    i32.const 20  i32.load
    i32.const 0  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- load8_s of high-bit-clear is positive (0x7F -> 127) ---
    i32.const 24  i32.const 0x7F  i32.store8
    i32.const 24  i32.load8_s
    i32.const 127  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- load16_s of 0xFFFF -> -1 ---
    i32.const 26  i32.const 0xFFFF  i32.store16
    i32.const 26  i32.load16_s
    i32.const -1  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- i64.load32_s of 0xFFFFFFFF -> -1 ---
    i32.const 28  i32.const 0xFFFFFFFF  i32.store
    i32.const 28  i64.load32_s
    i64.const -1  i64.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- store8 of negative i32 (-1) writes 0xFF ---
    i32.const 32  i32.const -1  i32.store8
    i32.const 32  i32.load8_u
    i32.const 0xFF  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- store16 of negative i32 (-1) writes 0xFFFF ---
    i32.const 34  i32.const -1  i32.store16
    i32.const 34  i32.load16_u
    i32.const 0xFFFF  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- dynamic address (computed) + static offset ---
    ;; base = 30 + 10 (computed) = 40, store via offset=8 -> addr 48
    i32.const 30  i32.const 10  i32.add        ;; dynamic addr 40
    i32.const 0xCAFED00D  i32.store offset=8    ;; effective 48
    i32.const 48  i32.load
    i32.const 0xCAFED00D  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; total checks = 14
    local.get $sum
    i32.const 14
    i32.ne)

  (export "_start" (func $_start)))
