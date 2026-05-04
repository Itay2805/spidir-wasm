;; Covers the parametric handlers nop, drop, and unreachable.
;;   - nop and drop are exercised inline in _start.
;;   - unreachable would trap if executed; we put it in a separately exported
;;     function that the JIT compiles (because it's exported) but the host
;;     never calls (the host only runs `_start`).
;; Returns 0 on success.
(module
  (func $never_called (result i32)
    unreachable)

  (func $_start (result i32)
    nop
    nop
    i32.const 99
    drop                        ;; pops the 99
    i32.const 0)                ;; success

  (export "never" (func $never_called))
  (export "_start" (func $_start)))
