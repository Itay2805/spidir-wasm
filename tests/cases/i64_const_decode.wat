;; i64.const LEB128 decode edge cases: INT64_MIN, -1, INT64_MAX, full-width
;; patterns that need the 10th LEB byte, and sign-extension boundaries.
;; Each successful check adds 1; expects $sum == 8.
(module
  (func $_start (result i32)
    (local $sum i32)

    ;; INT64_MIN = -9223372036854775808 = 0x8000000000000000
    i64.const -9223372036854775808 i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum

    ;; -1 (10-byte LEB, all bits set)
    i64.const -1 i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum

    ;; INT64_MAX = 9223372036854775807 = 0x7FFFFFFFFFFFFFFF
    i64.const 9223372036854775807 i64.const 0x7FFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum

    ;; full-width pattern 0x0123456789ABCDEF
    i64.const 0x0123456789ABCDEF i64.const 81985529216486895 i64.eq
    local.get $sum i32.add local.set $sum

    ;; high-bit-set pattern 0xFEDCBA9876543210 (negative)
    i64.const 0xFEDCBA9876543210 i64.const -81985529216486896 i64.eq
    local.get $sum i32.add local.set $sum

    ;; value just above 32 bits to confirm no 32-bit truncation
    i64.const 0x100000000 i64.const 4294967296 i64.eq
    local.get $sum i32.add local.set $sum

    ;; sign-extension boundary: -64
    i64.const -64 i64.const 0xFFFFFFFFFFFFFFC0 i64.eq
    local.get $sum i32.add local.set $sum

    ;; a positive that would be negative if mis-sign-extended at 32 bits
    i64.const 2147483648 i64.const 0x80000000 i64.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
