;; call_indirect on a NULL table element must TRAP cleanly.
;; Table has 2 slots; the elem segment only fills slot 0, so slot 1 is a
;; null funcref (table state is zero-initialized). Index 1 is in bounds, so
;; the bounds check passes, but the slot is null. Per spec, call_indirect
;; lowers to ...(call_ref y), which TRAPS on a ref.null reference.
;;
;; OBSERVED: instead of a controlled trap, the JIT emits callind through a
;; null target -> a wild call to address 0x0 (SIGSEGV at the zero page).
;; The exit is non-zero so a bare "must not return 0" assertion passes, but
;; the mechanism is an uncontrolled null-pointer jump, not a wasm trap.
(module
  (type $i_i (func (param i32) (result i32)))
  (func $inc (param $x i32) (result i32) local.get $x i32.const 1 i32.add)
  (table 2 funcref)
  (elem (i32.const 0) $inc)
  (func $_start (result i32)
    i32.const 0          ;; arg
    i32.const 1          ;; in-bounds, but the slot is NULL
    call_indirect (type $i_i))
  (export "_start" (func $_start)))
