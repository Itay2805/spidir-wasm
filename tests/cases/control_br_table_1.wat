;; Forces prepare_branch's backfill loop (inst.c:248) to run with inputs>=2:
;; the outer-end phi for $r materializes only on the 3rd/4th incoming edge,
;; after two earlier edges left $r unchanged (=0). Existing block_phi only
;; exercises the 2-edge, materialize-on-first-edge path. PASSES (exit 0):
;; confirms backfill is correct, but it is currently UNCOVERED.
(module
  (func $f (param $sel i32) (result i32)
    (local $r i32)
    block $end
      block $b3
        block $b2
          block $b1
            local.get $sel
            br_table $b1 $b2 $b3 $b3   ;; 0->b1 1->b2 else->b3
          end
          br 2                          ;; edge1: $r unchanged (0)
        end
        br 1                            ;; edge2: $r unchanged (0)
      end
      i32.const 99
      local.set $r                      ;; edge3: $r=99 -> phi materializes here
    end
    local.get $r)
  (func $_start (result i32)
    block i32.const 0 call $f i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const 1 call $f i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const 2 call $f i32.const 99 i32.eq br_if 0 unreachable end
    block i32.const 7 call $f i32.const 99 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
