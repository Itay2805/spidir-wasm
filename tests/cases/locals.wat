;; Exercises local.set and local.tee on declared (non-param) locals.
;; Returns 0 on success.
(module
  (func $square_plus_one (param $a i32) (param $b i32) (result i32)
    (local $sum i32)
    local.get $a
    local.get $b
    i32.add
    local.set $sum         ;; sum = a + b
    local.get $sum
    local.get $sum
    i32.mul                ;; sum * sum
    i32.const 1
    i32.add)               ;; (a+b)^2 + 1

  (func $double_via_tee (param $x i32) (result i32)
    (local $t i32)
    local.get $x
    local.tee $t           ;; t = x, leaves x on stack
    local.get $t
    i32.add)               ;; x + t = 2*x

  (func $_start (result i32)
    ;; (3+4)^2 + 1 = 50; double_via_tee(-4) = -8 ; sum = 42
    i32.const 3
    i32.const 4
    call $square_plus_one
    i32.const -4
    call $double_via_tee
    i32.add
    i32.const 42
    i32.ne)

  (export "_start" (func $_start)))
