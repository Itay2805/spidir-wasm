;; call_indirect that dispatches to an IMPORTED (host) function placed in the
;; table via an elem segment. The existing tests only call imports directly
;; and only call internal funcs indirectly; this covers the indirect-extern
;; combination (host pointer stored in a table slot, invoked via callind).
;; "env.add_i32" is provided by the host (host/main.c). exit-0 == correct.
(module
  (type $ii_i (func (param i32 i32) (result i32)))
  (import "env" "add_i32" (func $add_i32 (type $ii_i)))
  (func $sub (param $a i32) (param $b i32) (result i32) local.get $a local.get $b i32.sub)
  (table 2 funcref)
  (elem (i32.const 0) $add_i32 $sub)
  (func $_start (result i32)
    block i32.const 20 i32.const 22 i32.const 0 call_indirect (type $ii_i) i32.const 42 i32.eq br_if 0 unreachable end
    block i32.const 50 i32.const 8 i32.const 1 call_indirect (type $ii_i) i32.const 42 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
