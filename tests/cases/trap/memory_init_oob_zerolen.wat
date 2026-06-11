;; SPEC-GAP: memory.init with an out-of-bounds memory offset must trap even when
;; n == 0. Spec 4.6.8 memory.init steps 8-9 (trap if i+n > mem size or
;; j+n > data size) run BEFORE step 10 (`if n = 0 do nothing`). dst = 65537 >
;; 65536, n = 0 -> TRAP. The helper short-circuits on n==0 -> returns 0.
;; EXPECTED TO FAIL.
(module
  (memory 1)
  (data "abc")        ;; passive data segment 0
  (func (export "_start") (result i32)
    i32.const 65537   ;; dst out of bounds
    i32.const 0       ;; src offset into the segment
    i32.const 0       ;; n = 0
    memory.init 0
    i32.const 0))
