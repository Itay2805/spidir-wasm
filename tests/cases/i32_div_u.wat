;; Exercises wasm i32.div_u and i32.rem_u through real call boundaries.
;; Returns 0 on success.
(module
  (func $div_u (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.div_u)

  (func $rem_u (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.rem_u)

  (func $_start (result i32)
    ;; q = 200 / 6   = 33
    ;; r = 123 % 50  = 23
    ;; result = q + r = 56
    i32.const 200
    i32.const 6
    call $div_u
    i32.const 123
    i32.const 50
    call $rem_u
    i32.add
    i32.const 56
    i32.ne)

  (export "_start" (func $_start)))
