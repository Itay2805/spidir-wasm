;; OOB store must TRAP. i32.store at 65533 writes bytes 65533..65536 -> 1 past end.
;; Aborts with a non-zero exit (trap); handle separately from exit-0 success tests.
;; The in-bounds counterpart (store @ 65532) returns 0.
(module
  (memory 1)
  (func $_start (result i32)
    i32.const 65533
    i32.const 1
    i32.store         ;; traps: crosses end of memory
    i32.const 0)
  (export "_start" (func $_start)))
