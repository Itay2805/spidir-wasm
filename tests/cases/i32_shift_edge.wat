;; Extra edge cases for i32 shl/shr_s/shr_u/rotl/rotr not covered by
;; i32_shift.wat: negative shift counts (which exercise the explicit
;; `& 31` masking of a sign-extended count), positive shr_s (fills with
;; 0, not the sign bit), shl that drops bits off the high end, large
;; modulo counts that mask down to 0, and rotr/rotl with non-trivial
;; modulo counts. Returns 0 on success.
(module
  (func $shl   (param i32 i32) (result i32) local.get 0 local.get 1 i32.shl)
  (func $shr_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_s)
  (func $shr_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.shr_u)
  (func $rotl  (param i32 i32) (result i32) local.get 0 local.get 1 i32.rotl)
  (func $rotr  (param i32 i32) (result i32) local.get 0 local.get 1 i32.rotr)

  (func $_start (result i32)
    (local $sum i32)

    ;; negative count -1 masks to 31: shl(1, 31) = 0x80000000
    i32.const 1 i32.const -1 call $shl  i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; positive shr_s fills with 0: shr_s(0x40000000, 1) = 0x20000000
    i32.const 0x40000000 i32.const 1 call $shr_s  i32.const 0x20000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; shl drops high bits: shl(0x80000001, 1) = 0x00000002
    i32.const 0x80000001 i32.const 1 call $shl  i32.const 0x00000002 i32.eq
    local.get $sum i32.add local.set $sum
    ;; count 64 masks to 0 -> identity
    i32.const 0x12345678 i32.const 64 call $shl  i32.const 0x12345678 i32.eq
    local.get $sum i32.add local.set $sum
    ;; count 0x105 masks to 5: shl(1, 5) = 32
    i32.const 1 i32.const 0x105 call $shl  i32.const 32 i32.eq
    local.get $sum i32.add local.set $sum
    ;; shr_u by 31: 0x80000000 -> 1
    i32.const 0x80000000 i32.const 31 call $shr_u  i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    ;; rotr by 36 = rotr by 4: 0x12345678 -> 0x81234567
    i32.const 0x12345678 i32.const 36 call $rotr  i32.const 0x81234567 i32.eq
    local.get $sum i32.add local.set $sum
    ;; rotl negative count -4 masks to 28 = rotr by 4: 0x12345678 -> 0x81234567
    i32.const 0x12345678 i32.const -4 call $rotl  i32.const 0x81234567 i32.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 8 successful checks
    local.get $sum i32.const 8 i32.ne)

  (export "_start" (func $_start)))
