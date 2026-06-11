;; Comprehensive i32.div_u / i32.rem_u coverage: unsigned interpretation of
;; high-bit-set operands, div-by-max, and the boundary remainder. The existing
;; i32_div_u.wat only checks 200/6 and 123%50, missing every unsigned/edge case.
;; Returns 0 iff all 12 checks pass.
(module
  (func $div_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.div_u)
  (func $rem_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.rem_u)
  (func $_start (result i32)
    (local $sum i32)
    i32.const 200 i32.const 6 call $div_u i32.const 33 i32.eq local.get $sum i32.add local.set $sum
    ;; div_u must treat operands as UNSIGNED: 0xFFFFFFFF / 0xFFFFFFFE = 1
    i32.const -1 i32.const -2 call $div_u i32.const 1 i32.eq local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFF / 2 = 0x7FFFFFFF (not -1/2=0)
    i32.const -1 i32.const 2 call $div_u i32.const 2147483647 i32.eq local.get $sum i32.add local.set $sum
    ;; high bit numerator: 0x80000000 / 1 = 0x80000000
    i32.const -2147483648 i32.const 1 call $div_u i32.const -2147483648 i32.eq local.get $sum i32.add local.set $sum
    ;; 0x80000000 / 0x80000000 = 1
    i32.const -2147483648 i32.const -2147483648 call $div_u i32.const 1 i32.eq local.get $sum i32.add local.set $sum
    ;; smaller / 0xFFFFFFFF = 0 (unsigned)
    i32.const 12345 i32.const -1 call $div_u i32.const 0 i32.eq local.get $sum i32.add local.set $sum
    i32.const 123 i32.const 50 call $rem_u i32.const 23 i32.eq local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFF % 0xFFFFFFFE = 1
    i32.const -1 i32.const -2 call $rem_u i32.const 1 i32.eq local.get $sum i32.add local.set $sum
    ;; 0xFFFFFFFF % 2 = 1
    i32.const -1 i32.const 2 call $rem_u i32.const 1 i32.eq local.get $sum i32.add local.set $sum
    ;; x % 0xFFFFFFFF = x  for x < 0xFFFFFFFF (unsigned)
    i32.const 12345 i32.const -1 call $rem_u i32.const 12345 i32.eq local.get $sum i32.add local.set $sum
    ;; 0x80000000 % 7 = 2
    i32.const -2147483648 i32.const 7 call $rem_u i32.const 2 i32.eq local.get $sum i32.add local.set $sum
    ;; 0x80000000 % 0x40000000 = 0
    i32.const -2147483648 i32.const 1073741824 call $rem_u i32.const 0 i32.eq local.get $sum i32.add local.set $sum
    local.get $sum i32.const 12 i32.ne)
  (export "_start" (func $_start)))
