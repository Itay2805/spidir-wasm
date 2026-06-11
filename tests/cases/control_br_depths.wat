;; br to depths 0/1/2 with per-path locals, converging on a multi-input phi
;; at the outermost block end. Exercises the recursive (br l-1) lowering and
;; phi assembly across three exit depths. PASSES.
(module
  (func $pick (param $sel i32) (result i32)
    (local $r i32)
    block        ;; L2
      block      ;; L1
        block    ;; L0
          local.get $sel
          i32.eqz
          br_if 0                 ;; sel==0 exits L0
          local.get $sel
          i32.const 1
          i32.eq
          br_if 1                 ;; sel==1 exits L1
          i32.const 300
          local.set $r
          br 2                    ;; else exit L2 with r=300
        end
        i32.const 100
        local.set $r
        br 1                      ;; exit L2 with r=100
      end
      i32.const 200
      local.set $r                ;; fall through to L2 end with r=200
    end
    local.get $r)
  (func $_start (result i32)
    block i32.const 0 call $pick i32.const 100 i32.eq br_if 0 unreachable end
    block i32.const 1 call $pick i32.const 200 i32.eq br_if 0 unreachable end
    block i32.const 2 call $pick i32.const 300 i32.eq br_if 0 unreachable end
    block i32.const 9 call $pick i32.const 300 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
