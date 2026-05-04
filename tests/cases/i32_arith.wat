;; Exercises i32.add / i32.sub / i32.mul through real call boundaries so
;; each instruction is guaranteed to be present (not folded by an optimizer).
;; Returns 0 on success.
(module
  (func $add (param i32 i32) (result i32) local.get 0 local.get 1 i32.add)
  (func $sub (param i32 i32) (result i32) local.get 0 local.get 1 i32.sub)
  (func $mul (param i32 i32) (result i32) local.get 0 local.get 1 i32.mul)

  (func $_start (result i32)
    ;; (40 + 2) + (150 - 100) - (7 * 9) = 42 + 50 - 63 = 29
    i32.const 40  i32.const 2   call $add
    i32.const 150 i32.const 100 call $sub
    i32.add
    i32.const 7   i32.const 9   call $mul
    i32.sub
    i32.const 29
    i32.ne)

  (export "_start" (func $_start)))
