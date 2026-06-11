;; memory.copy whose destination range runs past the end of memory must trap
;; (spec 4.6.8 step 8). dst=65534, n=5 -> 65539 > 65536; src in bounds.
(module
  (memory 1)
  (func (export "_start") (result i32)
    i32.const 65534 i32.const 0 i32.const 5 memory.copy
    i32.const 0))
