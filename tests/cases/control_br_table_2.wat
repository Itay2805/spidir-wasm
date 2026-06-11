;; br_table index treated as UNSIGNED: INT32_MIN and INT32_MAX (not just -1)
;; must route to default; exact boundary index==table_size -> default.
;; Strengthens br_table.wat's negative-index coverage. PASSES.
(module
  (func $sw (param $x i32) (result i32)
    block $d
      block $c1
        block $c0
          local.get $x
          br_table $c0 $c1 $d
        end
        i32.const 10 return
      end
      i32.const 20 return
    end
    i32.const 99)
  (func $_start (result i32)
    block i32.const 0           call $sw i32.const 10 i32.eq br_if 0 unreachable end
    block i32.const 1           call $sw i32.const 20 i32.eq br_if 0 unreachable end
    block i32.const 2           call $sw i32.const 99 i32.eq br_if 0 unreachable end
    block i32.const -1          call $sw i32.const 99 i32.eq br_if 0 unreachable end
    block i32.const -2147483648 call $sw i32.const 99 i32.eq br_if 0 unreachable end
    block i32.const 2147483647  call $sw i32.const 99 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
