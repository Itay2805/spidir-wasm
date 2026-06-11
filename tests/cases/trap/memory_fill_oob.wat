;; memory.fill with a non-zero length that runs past the end of memory must trap
;; (spec 4.6.8: trap if dst + n > memory size). dst=65534, n=5 -> 65539 > 65536.
(module
  (memory 1)
  (func (export "_start") (result i32)
    i32.const 65534 i32.const 1 i32.const 5 memory.fill
    i32.const 0))
