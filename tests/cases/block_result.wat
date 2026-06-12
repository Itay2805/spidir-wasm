;; Blocks and loops with a single value-type result (0 params). Covers:
;;  - result via fallthrough, via forward br (terminated block), via br_if
;;    (divergent -> a phi is created), nested blocks, a clang-style loop result,
;;    and a non-i32 (f64) result. Returns 0 on success.
(module
  (func $b_fall (result i32)
    (block (result i32) i32.const 42))

  (func $b_br (result i32)
    (block (result i32) i32.const 7 br 0))

  ;; taken (c!=0) -> 100 via br_if; not taken -> 200 via fallthrough. divergent
  ;; values force a result phi.
  (func $b_phi (param $c i32) (result i32)
    (block (result i32)
      i32.const 100
      local.get $c
      br_if 0
      drop
      i32.const 200))

  ;; clang-style loop with a result: result is the value left on the stack when
  ;; the loop falls through. sumto(n) = 0+1+...+(n-1).
  (func $sumto (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    (loop $L (result i32)
      local.get $acc local.get $i i32.add local.set $acc
      local.get $i i32.const 1 i32.add local.set $i
      local.get $i local.get $n i32.lt_s
      br_if $L
      local.get $acc))

  (func $nested (result i32)
    (block (result i32)
      (block (result i32) i32.const 5)
      i32.const 10
      i32.add))

  (func $b_f64 (result f64)
    (block (result f64) f64.const 2.5))

  (func (export "_start") (result i32)
    (local $sum i32)
    call $b_fall i32.const 42 i32.eq local.get $sum i32.add local.set $sum
    call $b_br i32.const 7 i32.eq local.get $sum i32.add local.set $sum
    i32.const 1 call $b_phi i32.const 100 i32.eq local.get $sum i32.add local.set $sum
    i32.const 0 call $b_phi i32.const 200 i32.eq local.get $sum i32.add local.set $sum
    i32.const 4 call $sumto i32.const 6 i32.eq local.get $sum i32.add local.set $sum
    i32.const 5 call $sumto i32.const 10 i32.eq local.get $sum i32.add local.set $sum
    call $nested i32.const 15 i32.eq local.get $sum i32.add local.set $sum
    call $b_f64 f64.const 2.5 f64.eq local.get $sum i32.add local.set $sum
    local.get $sum i32.const 8 i32.ne))
