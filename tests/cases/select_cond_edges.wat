;; select condition truthiness edge cases: any nonzero 32-bit value is
;; truthy. Critically tests conditions whose LOW byte is 0 but upper bits
;; are set (0x100, 0x10000, 0x80000000) -- catches a lowering that only
;; tests the low 8 bits. Also tests INT_MAX and that exactly 0 is falsy.
;; Each check adds 1; expects $sum == 8.
(module
  (func $pick (param $cond i32) (param $a i32) (param $b i32) (result i32)
    local.get $a local.get $b local.get $cond select)
  (func $_start (result i32)
    (local $sum i32)

    ;; cond with only bit 8 set (low byte == 0) must be truthy -> picks val1
    i32.const 0x100 i32.const 11 i32.const 22 call $pick i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum

    ;; cond with only bit 16 set
    i32.const 0x10000 i32.const 11 i32.const 22 call $pick i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum

    ;; cond with only the sign bit set (low 31 bits == 0)
    i32.const 0x80000000 i32.const 11 i32.const 22 call $pick i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum

    ;; INT_MAX truthy
    i32.const 2147483647 i32.const 11 i32.const 22 call $pick i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum

    ;; cond == 0 -> val2
    i32.const 0 i32.const 11 i32.const 22 call $pick i32.const 22 i32.eq
    local.get $sum i32.add local.set $sum

    ;; cond derived at runtime: (5 - 5) == 0 -> val2
    i32.const 5 i32.const 5 i32.sub i32.const 11 i32.const 22 call $pick i32.const 22 i32.eq
    local.get $sum i32.add local.set $sum

    ;; cond derived at runtime: (0x100 & 0x100) nonzero -> val1
    i32.const 0x100 i32.const 0x100 i32.and i32.const 11 i32.const 22 call $pick i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum

    ;; select returns the un-narrowed first operand exactly (INT_MIN survives)
    i32.const 1 i32.const 0x80000000 i32.const 0 call $pick i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
