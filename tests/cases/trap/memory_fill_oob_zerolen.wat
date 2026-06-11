;; SPEC-GAP: memory.fill with dst > memory size must trap even when n == 0.
;; Spec 4.6.8 memory.fill: `trap if i + n > size` is checked BEFORE the
;; `otherwise if n = 0` short-circuit. dst = 65537 > 65536 (one 64KiB page),
;; n = 0 -> i+n = 65537 > 65536 -> TRAP. The helper short-circuits on n==0 and
;; does no bounds check, so it returns 0. EXPECTED TO FAIL until bounds are
;; checked before the n==0 fast path.
(module
  (memory 1)
  (func (export "_start") (result i32)
    i32.const 65537  ;; dst, one past end of memory
    i32.const 171    ;; fill byte
    i32.const 0      ;; n = 0
    memory.fill
    i32.const 0))
