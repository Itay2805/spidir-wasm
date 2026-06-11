;; Confirms two properties mem.wat omits:
;;  1. f32/f64 stores write the exact IEEE-754 little-endian byte pattern, readable
;;     back byte-by-byte (1.0f = 0x3F800000 -> bytes 00 00 80 3F).
;;  2. i64.store then two i32.load halves confirm 64-bit little-endian layout
;;     (low word at the base address, high word at +4).
(module
  (memory 1)

  (func $_start (result i32)
    (local $sum i32)

    ;; --- f32 1.0 == 0x3F800000, little-endian bytes 00 00 80 3F ---
    i32.const 0  f32.const 1.0  f32.store
    i32.const 0  i32.load8_u  i32.const 0x00  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 1  i32.load8_u  i32.const 0x00  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 2  i32.load8_u  i32.const 0x80  i32.eq
    local.get $sum  i32.add  local.set $sum
    i32.const 3  i32.load8_u  i32.const 0x3F  i32.eq
    local.get $sum  i32.add  local.set $sum
    ;; and an i32.load over the same 4 bytes gives the whole pattern
    i32.const 0  i32.load  i32.const 0x3F800000  i32.eq
    local.get $sum  i32.add  local.set $sum

    ;; --- f64 1.0 == 0x3FF0000000000000 ; low word 0, high word 0x3FF00000 ---
    i32.const 8  f64.const 1.0  f64.store
    i32.const 8  i32.load  i32.const 0x00000000  i32.eq      ;; low 32 bits
    local.get $sum  i32.add  local.set $sum
    i32.const 12 i32.load  i32.const 0x3FF00000  i32.eq      ;; high 32 bits
    local.get $sum  i32.add  local.set $sum

    ;; --- i64 little-endian half aliasing ---
    i32.const 16  i64.const 0x1122334455667788  i64.store
    i32.const 16  i32.load  i32.const 0x55667788  i32.eq     ;; low word
    local.get $sum  i32.add  local.set $sum
    i32.const 20  i32.load  i32.const 0x11223344  i32.eq     ;; high word
    local.get $sum  i32.add  local.set $sum

    ;; total = 9
    local.get $sum
    i32.const 9
    i32.ne)

  (export "_start" (func $_start)))
