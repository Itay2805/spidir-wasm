;; call_indirect with an out-of-bounds table index must TRAP.
;; Table has 2 slots; we dispatch with index 5. call_indirect bounds-checks the
;; index (brcond to a trap block) and aborts with a non-zero exit (SIGILL).
(module
  (type $i_i (func (param i32) (result i32)))
  (func $inc (param $x i32) (result i32) local.get $x i32.const 1 i32.add)
  (table 2 funcref)
  (elem (i32.const 0) $inc)
  (func $_start (result i32)
    i32.const 0
    i32.const 5
    call_indirect (type $i_i))
  (export "_start" (func $_start)))
