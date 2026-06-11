;; OOB load must TRAP. Memory is 1 page (65536 bytes); valid addresses 0..65535.
;; i32.load at 65533 reads bytes 65533..65536, one byte past the end -> trap.
;; A trap aborts the module with a NON-ZERO exit (SIGSEGV via the PROT_NONE guard
;; region), so this case is NOT an exit-0 success test: the harness must assert an
;; abnormal / non-zero exit. The in-bounds counterpart (load @ 65532) returns 0.
(module
  (memory 1)
  (func $_start (result i32)
    i32.const 65533
    i32.load          ;; traps: crosses end of memory
    drop
    i32.const 0)      ;; unreachable; module never returns normally
  (export "_start" (func $_start)))
