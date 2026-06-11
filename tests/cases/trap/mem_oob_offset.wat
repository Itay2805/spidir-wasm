;; The static memarg offset participates in the bounds check: with dynamic address
;; 0 and a static offset of 65533, i32.load reads bytes 65533..65536, one past the
;; end -> TRAP. Confirms the offset is added before the bounds check. Aborts with a
;; non-zero exit (trap); handle separately from exit-0 success tests.
(module
  (memory 1)
  (func $_start (result i32)
    i32.const 0
    i32.load offset=65533   ;; effective range 65533..65536 -> traps
    drop
    i32.const 0)
  (export "_start" (func $_start)))
