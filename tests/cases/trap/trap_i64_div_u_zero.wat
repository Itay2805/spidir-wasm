;; TRAP test: i64.div_u by zero must trap. EXPECTED non-zero exit.
(module
  (func $_start (result i32)
    i64.const 1 i64.const 0 i64.div_u
    i64.const 0 i64.ne)
  (export "_start" (func $_start)))
