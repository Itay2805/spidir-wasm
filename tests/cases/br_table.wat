;; Exercises the wasm `br_table` opcode (computed branch / switch).
;; Covers:
;;   - multi-entry table dispatching to distinct labels
;;   - default case taken when the index is >= table size (incl. negative,
;;     which is interpreted as a large unsigned value by the ULT check)
;;   - single-entry table — exercises the i == table_size - 1 path on the
;;     very first iteration of the case-emission loop
;;   - the same label appearing multiple times in the table — verifies
;;     prepare_branch tolerates repeated entries to one target
;;   - dispatching to labels at *different* depths (each `block` is one
;;     label level), not just to siblings
;; Returns 0 on success.
(module

  ;; Three-way switch with a default. Each case sits at a distinct nesting
  ;; depth, so the table has labels of varying depth (0, 1, 2, 3).
  ;;   0 -> 100, 1 -> 200, 2 -> 300, otherwise -> 999.
  (func $classify (param $x i32) (result i32)
    block $default
      block $case2
        block $case1
          block $case0
            local.get $x
            br_table $case0 $case1 $case2 $default
          end
          i32.const 100
          return
        end
        i32.const 200
        return
      end
      i32.const 300
      return
    end
    i32.const 999)

  ;; Single-entry table — table_size == 1 means the case loop's very
  ;; first iteration is also its last, hitting the unconditional-branch
  ;; arm of the i == table_size - 1 special case.
  ;;   0 -> 11, otherwise -> 22.
  (func $only_zero (param $x i32) (result i32)
    block $default
      block $case0
        local.get $x
        br_table $case0 $default
      end
      i32.const 11
      return
    end
    i32.const 22)

  ;; Same label appearing multiple times in the table.
  ;;   0,1,2 -> 1, otherwise -> 0.
  (func $is_low (param $x i32) (result i32)
    block $default
      block $low
        local.get $x
        br_table $low $low $low $default
      end
      i32.const 1
      return
    end
    i32.const 0)

  ;; Empty table — `br_table` with zero entries collapses to an
  ;; unconditional branch to the default label, regardless of index.
  ;; The ULT check (index < 0) is always false, so the default is taken.
  (func $always_default (param $x i32) (result i32)
    block $default
      local.get $x
      br_table $default
    end
    i32.const 7)

  ;; Each check traps on first mismatch. Expected values were derived by
  ;; hand from the source above.
  (func $_start (result i32)
    ;; classify — first, middle, last in-range; out-of-range high; negative.
    block i32.const 0    call $classify  i32.const 100  i32.eq br_if 0 unreachable end
    block i32.const 1    call $classify  i32.const 200  i32.eq br_if 0 unreachable end
    block i32.const 2    call $classify  i32.const 300  i32.eq br_if 0 unreachable end
    block i32.const 3    call $classify  i32.const 999  i32.eq br_if 0 unreachable end
    block i32.const 100  call $classify  i32.const 999  i32.eq br_if 0 unreachable end
    block i32.const -1   call $classify  i32.const 999  i32.eq br_if 0 unreachable end

    ;; only_zero — single-entry table.
    block i32.const 0    call $only_zero  i32.const 11  i32.eq br_if 0 unreachable end
    block i32.const 1    call $only_zero  i32.const 22  i32.eq br_if 0 unreachable end
    block i32.const 5    call $only_zero  i32.const 22  i32.eq br_if 0 unreachable end
    block i32.const -1   call $only_zero  i32.const 22  i32.eq br_if 0 unreachable end

    ;; is_low — repeated label entries.
    block i32.const 0    call $is_low  i32.const 1  i32.eq br_if 0 unreachable end
    block i32.const 1    call $is_low  i32.const 1  i32.eq br_if 0 unreachable end
    block i32.const 2    call $is_low  i32.const 1  i32.eq br_if 0 unreachable end
    block i32.const 3    call $is_low  i32.const 0  i32.eq br_if 0 unreachable end
    block i32.const -1   call $is_low  i32.const 0  i32.eq br_if 0 unreachable end

    ;; always_default — empty table.
    block i32.const 0    call $always_default  i32.const 7  i32.eq br_if 0 unreachable end
    block i32.const 42   call $always_default  i32.const 7  i32.eq br_if 0 unreachable end
    block i32.const -1   call $always_default  i32.const 7  i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
