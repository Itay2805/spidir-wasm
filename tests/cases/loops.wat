;; Exercises the wasm `loop` opcode and its phi-at-loop-header semantics.
;; A `loop` label's branch target is the loop *header* (not the post-loop
;; block), so `br 0` inside a loop is a back-edge. This forces the JIT to
;; eagerly create a phi for every local at the loop header so that values
;; written inside the body can flow back via the back-edge.
;; Returns 0 on success.
(module
  ;; Sum 0..n-1 via a counter loop. Exercises:
  ;;   - loop header phis for $i and $acc
  ;;   - br 1 to exit the enclosing block (loop exit)
  ;;   - br 0 as the back-edge (continue)
  (func $sum_to (param $n i32) (result i32)
    (local $i i32)
    (local $acc i32)
    block
      loop
        local.get $i
        local.get $n
        i32.ge_s
        br_if 1                 ;; i >= n -> exit outer block

        local.get $acc
        local.get $i
        i32.add
        local.set $acc          ;; acc += i

        local.get $i
        i32.const 1
        i32.add
        local.set $i            ;; i  += 1

        br 0                    ;; back-edge to loop header
      end
    end
    local.get $acc)

  ;; Same shape but with i64 locals — verifies that the loop-header phi
  ;; is emitted with the correct type for non-i32 locals.
  (func $sum_to_i64 (param $n i64) (result i64)
    (local $i i64)
    (local $acc i64)
    block
      loop
        local.get $i
        local.get $n
        i64.ge_s
        br_if 1

        local.get $acc
        local.get $i
        i64.add
        local.set $acc

        local.get $i
        i64.const 1
        i64.add
        local.set $i

        br 0
      end
    end
    local.get $acc)

  ;; Nested loops: outer iterates rows, inner iterates cols.
  ;; result = sum over r in [0,rows), c in [0,cols) of (r*cols + c).
  ;; Exercises nested loop labels — each inner-loop iteration creates new
  ;; phi inputs that must not leak into the outer loop's phis.
  (func $grid_sum (param $rows i32) (param $cols i32) (result i32)
    (local $r i32)
    (local $c i32)
    (local $acc i32)
    block ;; outer-exit
      loop ;; outer-header
        local.get $r
        local.get $rows
        i32.ge_s
        br_if 1               ;; exit outer

        ;; reset $c for each row
        i32.const 0
        local.set $c

        block ;; inner-exit
          loop ;; inner-header
            local.get $c
            local.get $cols
            i32.ge_s
            br_if 1            ;; exit inner

            local.get $acc
            local.get $r
            local.get $cols
            i32.mul
            local.get $c
            i32.add
            i32.add
            local.set $acc     ;; acc += r*cols + c

            local.get $c
            i32.const 1
            i32.add
            local.set $c

            br 0               ;; inner back-edge
          end
        end

        local.get $r
        i32.const 1
        i32.add
        local.set $r

        br 0                   ;; outer back-edge
      end
    end
    local.get $acc)

  ;; Loop with natural fall-through (no back-edge taken): body executes
  ;; once and then falls off the end of the loop, exiting normally.
  ;; Exercises the loop fall-through path in jit_wasm_end (creates the
  ;; post-loop block; does NOT branch back to the header).
  (func $once (result i32)
    (local $x i32)
    loop
      local.get $x
      i32.const 1
      i32.add
      local.set $x
      ;; no br -> fall through and exit the loop
    end
    local.get $x)

  ;; Early-exit via `return` from inside a loop — verifies that a loop
  ;; whose only exit is a return doesn't leave the verifier in a bad
  ;; state (label is terminated mid-loop). Uses `block`+`br_if` instead
  ;; of `if`, since the JIT does not yet implement the `if` opcode.
  (func $find_first_ge (param $n i32) (result i32)
    (local $i i32)
    loop
      block
        local.get $i
        local.get $n
        i32.lt_s
        br_if 0                    ;; i < n -> skip the return
        local.get $i
        return                     ;; i >= n -> return $i
      end
      local.get $i
      i32.const 1
      i32.add
      local.set $i
      br 0
    end
    i32.const -1)

  ;; Each check traps on first mismatch rather than accumulating. That way
  ;; if the test exits with 0, we know every individual case matched its
  ;; expected value — not that the sum of successes happened to land on
  ;; the expected total. The expected values themselves were
  ;; cross-validated against `wasm-interp` (wabt) when this test was
  ;; written.
  (func $_start (result i32)
    ;; sum_to(0)  = 0
    block i32.const 0   call $sum_to  i32.const 0    i32.eq br_if 0 unreachable end
    ;; sum_to(1)  = 0  (one iteration, adds 0)
    block i32.const 1   call $sum_to  i32.const 0    i32.eq br_if 0 unreachable end
    ;; sum_to(10) = 45
    block i32.const 10  call $sum_to  i32.const 45   i32.eq br_if 0 unreachable end
    ;; sum_to(100) = 4950
    block i32.const 100 call $sum_to  i32.const 4950 i32.eq br_if 0 unreachable end

    ;; sum_to_i64(1000) = 499500
    block i64.const 1000 call $sum_to_i64  i64.const 499500 i64.eq br_if 0 unreachable end

    ;; grid_sum(3, 4) = sum of {0..11} = 66
    block i32.const 3 i32.const 4 call $grid_sum  i32.const 66 i32.eq br_if 0 unreachable end
    ;; grid_sum(0, 5) = 0  (zero-trip outer loop)
    block i32.const 0 i32.const 5 call $grid_sum  i32.const 0  i32.eq br_if 0 unreachable end
    ;; grid_sum(5, 0) = 0  (zero-trip inner loop)
    block i32.const 5 i32.const 0 call $grid_sum  i32.const 0  i32.eq br_if 0 unreachable end

    ;; once() = 1  (loop body runs exactly once, then falls through)
    block call $once  i32.const 1 i32.eq br_if 0 unreachable end

    ;; find_first_ge(5) = 5  (loop with `return` exit)
    block i32.const 5 call $find_first_ge  i32.const 5 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
