;; Loop whose CONTINUE is a conditional br_if 0 (back-edge) and whose exit is
;; a natural fall-through off the bottom of the loop with a live $acc phi.
;; Distinct from loops.wat (which uses br 0 continue + br_if 1 exit). PASSES.
(module
  (func $sum (param $n i32) (result i32)
    (local $i i32) (local $acc i32)
    loop
      local.get $acc
      local.get $i
      i32.add
      local.set $acc
      local.get $i
      i32.const 1
      i32.add
      local.set $i
      local.get $i
      local.get $n
      i32.lt_s
      br_if 0          ;; i<n -> conditional back-edge (continue)
    end                ;; i>=n -> fall through, exit loop
    local.get $acc)
  (func $_start (result i32)
    block i32.const 1  call $sum i32.const 0    i32.eq br_if 0 unreachable end
    block i32.const 5  call $sum i32.const 10   i32.eq br_if 0 unreachable end
    block i32.const 10 call $sum i32.const 45   i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
