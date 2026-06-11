;; memory.init whose destination range runs past the end of memory must trap
;; (spec 4.6.8 step 8). The 5-byte source segment is fully in range, so the trap
;; comes from the memory bound: dst=65534, n=5 -> 65539 > 65536.
(module
  (memory 1)
  (data "abcde")
  (func (export "_start") (result i32)
    i32.const 65534 i32.const 0 i32.const 5 memory.init 0
    i32.const 0))
