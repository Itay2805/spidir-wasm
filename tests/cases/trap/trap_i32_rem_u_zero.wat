;; TRAP test: i32.rem_u by zero must trap. EXPECTED non-zero exit.
(module
  (func $_start (result i32)
    i32.const 1 i32.const 0 i32.rem_u)
  (export "_start" (func $_start)))
