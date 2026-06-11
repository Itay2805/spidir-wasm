;; i32.const LEB128 decode edge cases: INT_MIN, -1, INT_MAX, and large
;; positive/negative values that exercise sign-extension of the final
;; LEB128 byte. Each check pushes 1 on success; expects $sum == 7.
(module
  (func $_start (result i32)
    (local $sum i32)

    ;; INT_MIN = -2147483648 = 0x80000000
    i32.const -2147483648 i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum

    ;; -1 (5-byte LEB, all continuation, sign bit set)
    i32.const -1 i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum

    ;; INT_MAX = 2147483647 = 0x7FFFFFFF
    i32.const 2147483647 i32.const 0x7FFFFFFF i32.eq
    local.get $sum i32.add local.set $sum

    ;; small negative whose value depends on sign-extension: -64 = 0x40 byte
    i32.const -64 i32.const 0xFFFFFFC0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; -128 (boundary of single 7-bit group)
    i32.const -128 i32.const 0xFFFFFF80 i32.eq
    local.get $sum i32.add local.set $sum

    ;; large positive 0x12345678
    i32.const 0x12345678 i32.const 305419896 i32.eq
    local.get $sum i32.add local.set $sum

    ;; large negative 0x80000001 (INT_MIN+1 as unsigned)
    i32.const 0x80000001 i32.const -2147483647 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 7 i32.ne)
  (export "_start" (func $_start)))
