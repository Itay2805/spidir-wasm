;; call_indirect coverage the existing test misses:
;;   - zero-argument indirect call (type ()->i32)
;;   - i64 param & i64 result through the indirect boundary
;;   - f64 param & f64 result through the indirect boundary
;;   - many params (5 i32 args -> i32) so args spill past registers
;; Each block traps on mismatch, so exit-0 means every indirect dispatch
;; produced the right value.
(module
  (type $void_i (func (result i32)))
  (type $l_l    (func (param i64) (result i64)))
  (type $d_d    (func (param f64) (result f64)))
  (type $5i_i   (func (param i32 i32 i32 i32 i32) (result i32)))

  (func $forty2 (result i32) i32.const 42)
  (func $i64dbl (param $x i64) (result i64) local.get $x i64.const 1 i64.shl)
  (func $f64inc (param $x f64) (result f64) local.get $x f64.const 1.5 f64.add)
  (func $sum5 (param $a i32) (param $b i32) (param $c i32) (param $d i32) (param $e i32) (result i32)
    local.get $a local.get $b i32.add
    local.get $c i32.add local.get $d i32.add local.get $e i32.add)

  (table 4 funcref)
  (elem (i32.const 0) $forty2 $i64dbl $f64inc $sum5)

  (func $_start (result i32)
    block i32.const 0 call_indirect (type $void_i) i32.const 42 i32.eq br_if 0 unreachable end
    block i64.const 21 i32.const 1 call_indirect (type $l_l) i64.const 42 i64.eq br_if 0 unreachable end
    block f64.const 40.5 i32.const 2 call_indirect (type $d_d) f64.const 42.0 f64.eq br_if 0 unreachable end
    block i32.const 1 i32.const 2 i32.const 3 i32.const 4 i32.const 5 i32.const 3
          call_indirect (type $5i_i) i32.const 15 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
