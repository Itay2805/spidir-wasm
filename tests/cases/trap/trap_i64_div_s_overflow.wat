;; TRAP test: i64.div_s of INT64_MIN / -1 overflows and MUST trap.
;; EXPECTED non-zero exit.
(module
  (func $_start (result i32)
    i64.const -9223372036854775808 i64.const -1 i64.div_s
    i64.const 0 i64.ne)
  (export "_start" (func $_start)))
