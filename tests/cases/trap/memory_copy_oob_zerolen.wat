;; SPEC-GAP: memory.copy with an out-of-bounds offset must trap even when n == 0.
;; Spec 4.6.8 memory.copy steps 8-9 (trap if i1+n > size or i2+n > size) run
;; BEFORE step 10 (`if n = 0 do nothing`). dst = 65537 > 65536, n = 0 -> TRAP.
;; The helper short-circuits on n==0 -> returns 0. EXPECTED TO FAIL.
(module
  (memory 1)
  (func (export "_start") (result i32)
    i32.const 65537  ;; dst out of bounds
    i32.const 0      ;; src
    i32.const 0      ;; n = 0
    memory.copy
    i32.const 0))
