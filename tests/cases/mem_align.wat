;; Alignment hint must not affect semantics: a misaligned access succeeds, and an
;; over-stated alignment annotation on a misaligned address must still succeed.
(module
  (memory 1)
  (func $_start (result i32)
    (local $sum i32)
    ;; misaligned i32 round-trip at odd address 1 (default/natural align hint)
    i32.const 1  i32.const 0x0BADF00D  i32.store
    i32.const 1  i32.load  i32.const 0x0BADF00D  i32.eq
    local.get $sum  i32.add  local.set $sum
    ;; same misaligned address with an over-stated align=4 (4-byte) hint
    i32.const 3  i32.const 0xFEEDFACE  i32.store align=4
    i32.const 3  i32.load align=4  i32.const 0xFEEDFACE  i32.eq
    local.get $sum  i32.add  local.set $sum
    ;; misaligned i64 at address 7 with align=8
    i32.const 7  i64.const 0x0123456789ABCDEF  i64.store align=8
    i32.const 7  i64.load align=8  i64.const 0x0123456789ABCDEF  i64.eq
    local.get $sum  i32.add  local.set $sum
    local.get $sum  i32.const 3  i32.ne)
  (export "_start" (func $_start)))
