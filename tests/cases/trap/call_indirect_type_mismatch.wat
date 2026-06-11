;; SPEC-GAP (out of spec): call_indirect must trap on a signature mismatch.
;; Spec 4.6.2 lowers call_indirect to table.get + ref.cast to the expected type;
;; the cast fails -> trap. Slot 0 holds a ()->i32 function but it is invoked
;; with the (i32)->i32 signature, so this MUST trap. The JIT performs no runtime
;; type check (a fast CFI scheme is TBD), so it calls the wrong function and
;; returns 0 instead. EXPECTED TO FAIL until the type check lands.
(module
  (type $i_i (func (param i32) (result i32)))
  (func $ret0 (result i32) i32.const 0)
  (table 1 funcref)
  (elem (i32.const 0) $ret0)
  (func (export "_start") (result i32)
    i32.const 99      ;; bogus arg for the (i32)->i32 signature
    i32.const 0       ;; table index 0 -> $ret0, whose type is ()->i32
    call_indirect (type $i_i)))
