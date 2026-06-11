;; TRAP test: i32.div_u by zero must trap (spec idiv_u(i,0)={}). This module
;; never returns normally and is EXPECTED to terminate non-zero. (Currently the
;; JIT realizes this as a hardware SIGFPE rather than a graceful WASM trap, but
;; it does exit non-zero, satisfying the trap contract.)
(module
  (func $_start (result i32)
    i32.const 1 i32.const 0 i32.div_u)
  (export "_start" (func $_start)))
