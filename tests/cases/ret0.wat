;; Smoke test for the very first JIT-able commit: a function whose body
;; is `i32.const 0; end`. If this passes, the JIT can lower a constant
;; and return it through the implicit end-of-function fallthrough.
(module
  (func $_start (result i32)
    i32.const 0)
  (export "_start" (func $_start)))
