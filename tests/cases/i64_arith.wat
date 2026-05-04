;; Exercises i64 arithmetic. Helpers operate on i64 and the final result is
;; collapsed to i32 via i64.eq so we don't need i64.wrap_i32.
;; Returns 0 on success.
(module
  (func $add64 (param i64 i64) (result i64)
    local.get 0
    local.get 1
    i64.add)

  (func $sub64 (param i64 i64) (result i64)
    local.get 0
    local.get 1
    i64.sub)

  (func $mul64 (param i64 i64) (result i64)
    local.get 0
    local.get 1
    i64.mul)

  (func $_start (result i32)
    ;; x = 1_000_000_000 + 2_000_000_000   = 3_000_000_000
    ;; y = 50_000 * 60_000                 = 3_000_000_000
    ;; z = x - y                           = 0
    i64.const 1000000000
    i64.const 2000000000
    call $add64
    i64.const 50000
    i64.const 60000
    call $mul64
    call $sub64
    i64.const 0
    i64.ne)

  (export "_start" (func $_start)))
