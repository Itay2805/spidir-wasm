;; Direct self-recursion (factorial) and mutual recursion (is_even/is_odd),
;; using a guarded early `return` for the base case so the recursive call is
;; only reached when n>0. (if/else is unsupported, so block+br_if+return.)
;; Returns 0 on success.
(module
  (func $fact (param $n i32) (result i32)
    block
      local.get $n
      br_if 0            ;; n!=0 -> skip the early return
      i32.const 1
      return
    end
    local.get $n
    local.get $n i32.const 1 i32.sub
    call $fact
    i32.mul)

  (func $is_even (param $n i32) (result i32)
    block
      local.get $n
      br_if 0
      i32.const 1        ;; is_even(0) = 1
      return
    end
    local.get $n i32.const 1 i32.sub
    call $is_odd)
  (func $is_odd (param $n i32) (result i32)
    block
      local.get $n
      br_if 0
      i32.const 0        ;; is_odd(0) = 0
      return
    end
    local.get $n i32.const 1 i32.sub
    call $is_even)

  (func $_start (result i32)
    (local $sum i32)
    i32.const 5 call $fact i32.const 120 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 0 call $fact i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 6 call $fact i32.const 720 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 10 call $is_even i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 7 call $is_even i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 7 call $is_odd i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    local.get $sum i32.const 6 i32.ne)
  (export "_start" (func $_start)))
