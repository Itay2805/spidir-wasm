;; Extra edge cases for i64 shl/shr_s/shr_u/rotl/rotr not covered by
;; i64_shift.wat: negative shift counts (which exercise the explicit
;; `& 63` masking of a sign-extended count), positive shr_s (fills with
;; 0, not the sign bit), shl that drops bits off the high end, large
;; modulo counts that mask down to 0, and rotr/rotl with non-trivial
;; modulo counts. Returns 0 on success.
(module
  (func $shl   (param i64 i64) (result i64) local.get 0 local.get 1 i64.shl)
  (func $shr_s (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_s)
  (func $shr_u (param i64 i64) (result i64) local.get 0 local.get 1 i64.shr_u)
  (func $rotl  (param i64 i64) (result i64) local.get 0 local.get 1 i64.rotl)
  (func $rotr  (param i64 i64) (result i64) local.get 0 local.get 1 i64.rotr)

  (func $_start (result i32)
    (local $sum i32)

    ;; negative count -1 masks to 63: shl(1, 63) = 0x8000000000000000
    i64.const 1 i64.const -1 call $shl  i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; positive shr_s fills with 0: shr_s(0x4000000000000000, 1) = 0x2000000000000000
    i64.const 0x4000000000000000 i64.const 1 call $shr_s  i64.const 0x2000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; shl drops high bits: shl(0x8000000000000001, 1) = 0x0000000000000002
    i64.const 0x8000000000000001 i64.const 1 call $shl  i64.const 0x0000000000000002 i64.eq
    local.get $sum i32.add local.set $sum
    ;; count 128 masks to 0 -> identity
    i64.const 0x123456789ABCDEF0 i64.const 128 call $shl  i64.const 0x123456789ABCDEF0 i64.eq
    local.get $sum i32.add local.set $sum
    ;; shr_u by 63: 0x8000000000000000 -> 1
    i64.const 0x8000000000000000 i64.const 63 call $shr_u  i64.const 1 i64.eq
    local.get $sum i32.add local.set $sum
    ;; rotr by 72 = rotr by 8: 0x123456789ABCDEF0 -> 0xF0123456789ABCDE
    i64.const 0x123456789ABCDEF0 i64.const 72 call $rotr  i64.const 0xF0123456789ABCDE i64.eq
    local.get $sum i32.add local.set $sum
    ;; rotl negative count -8 masks to 56 = rotr by 8: -> 0xF0123456789ABCDE
    i64.const 0x123456789ABCDEF0 i64.const -8 call $rotl  i64.const 0xF0123456789ABCDE i64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 7 successful checks
    local.get $sum i32.const 7 i32.ne)

  (export "_start" (func $_start)))
