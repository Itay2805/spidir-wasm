;; Loop with TWO distinct back-edges into the header (br_if 0 continue plus a
;; later br 0), so the header phis for $i/$acc receive >2 control inputs.
;; Cross-checked against wasm-interp (=0). PASSES.
(module
  (func $f (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    block $exit
      loop $hdr
        local.get $i
        local.get $n
        i32.ge_s
        br_if 1
        local.get $acc
        local.get $i
        i32.add
        local.set $acc
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        local.get $i
        i32.const 1
        i32.sub
        i32.const 1
        i32.and
        i32.eqz
        br_if 0            ;; even -> back-edge #1
        local.get $acc
        local.get $i
        i32.const 1
        i32.sub
        i32.add
        local.set $acc
        br 0               ;; back-edge #2
      end
    end
    local.get $acc)
  (func $_start (result i32)
    ;; n=4: i=0->+0, i=1(odd)->+1+1=2, i=2->+2, i=3(odd)->+3+3=6 => 10
    block i32.const 4 call $f i32.const 10 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
