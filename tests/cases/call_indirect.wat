;; Exercises wasm function pointers (the `call_indirect` opcode) and the
;; supporting table + elem-segment infrastructure. Returns 0 on success.
;;
;; What this exercises end-to-end:
;;   1. Module section parsing: `table 0 funcref` (table section, 0x04)
;;      and the `(elem ...)` segment that fills it (element section, 0x09).
;;   2. The `call_indirect` opcode (0x11): pop an i32 table index, look up
;;      the funcref, type-check it against the immediate typeidx, and call.
;;   3. Indirect dispatch over multiple type signatures so the typeidx
;;      check actually matters.
(module
  ;; Two signatures so call_indirect's typeidx immediate is meaningful.
  (type $i_i (func (param i32) (result i32)))
  (type $ii_i (func (param i32) (param i32) (result i32)))

  ;; Three (i32) -> i32 ops, plus one (i32, i32) -> i32 to differentiate
  ;; the two type signatures.
  (func $inc  (param $x i32) (result i32) local.get $x i32.const 1 i32.add)
  (func $dbl  (param $x i32) (result i32) local.get $x i32.const 1 i32.shl)
  (func $neg  (param $x i32) (result i32) i32.const 0 local.get $x i32.sub)
  (func $add2 (param $a i32) (param $b i32) (result i32) local.get $a local.get $b i32.add)

  ;; Table of 4 funcrefs, populated at link time by the elem segment below.
  (table 4 funcref)
  (elem (i32.const 0) $inc $dbl $neg $add2)

  ;; Dispatch (i32)->i32 ops by table index.
  (func $apply1 (param $idx i32) (param $x i32) (result i32)
    local.get $x
    local.get $idx
    call_indirect (type $i_i))

  ;; Dispatch (i32,i32)->i32 ops by table index — separate typeidx.
  (func $apply2 (param $idx i32) (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    local.get $idx
    call_indirect (type $ii_i))

  ;; Each check traps on first mismatch — exit-0 means every individual
  ;; case actually went through call_indirect with the right result.
  (func $_start (result i32)
    ;; inc(10) via table[0]
    block i32.const 0 i32.const 10 call $apply1   i32.const 11   i32.eq br_if 0 unreachable end
    ;; dbl(7)  via table[1]
    block i32.const 1 i32.const 7  call $apply1   i32.const 14   i32.eq br_if 0 unreachable end
    ;; neg(5)  via table[2]
    block i32.const 2 i32.const 5  call $apply1   i32.const -5   i32.eq br_if 0 unreachable end
    ;; add2(3, 4) via table[3]
    block i32.const 3 i32.const 3 i32.const 4 call $apply2  i32.const 7 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
