;; Exercises block + br + br_if. Computes |x| using a block to skip the
;; negate path when x is non-negative.
;; Returns 0 on success.
(module
  (func $abs (param $x i32) (result i32)
    block
      ;; if x < 0, exit the block to fall through to the negate path
      local.get $x
      i32.const 0
      i32.lt_s
      br_if 0
      ;; non-negative: return x as-is
      local.get $x
      return
    end
    ;; negate path
    i32.const 0
    local.get $x
    i32.sub)

  (func $first_pos (param $a i32) (param $b i32) (result i32)
    block
      ;; if $a is non-negative, return it
      local.get $a
      i32.const 0
      i32.lt_s
      br_if 0
      local.get $a
      return
    end
    ;; otherwise return $b unconditionally via br 0 out of the inner block
    block
      br 0
    end
    local.get $b)

  (func $_start (result i32)
    ;; abs(-37) + first_pos(-1, 5) = 37 + 5 = 42
    i32.const -37
    call $abs
    i32.const -1
    i32.const 5
    call $first_pos
    i32.add
    i32.const 42
    i32.ne)

  (export "_start" (func $_start)))
