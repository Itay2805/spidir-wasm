;; Exercises unreachable (dead) code handling in the JIT. Every stack-polymorphic
;; instruction (unreachable / br / br_table / return) terminates the current
;; block; per the wasm spec the instructions after it, up to the matching `end`,
;; are unreachable — decoded but never emitted. The JIT must walk past them
;; without emitting into the (already terminated) spidir block, and must not
;; dereference a block's lazily-allocated locals when the continuation is dead.
;;
;; Returns 0 on success.
(module
  ;; Plain dead code after a terminator: a second `unreachable` plus assorted
  ;; instructions that must all be skipped, not emitted.
  (func $dead_after_terminator (result i32)
    i32.const 0
    return
    ;; --- unreachable below ---
    unreachable
    unreachable
    i32.const 999
    i32.add
    drop
    call $dead_after_terminator)

  ;; A block carrying a local, terminated by `unreachable`, with nothing
  ;; branching to it: its locals array is never allocated and its continuation
  ;; is dead. The JIT must not read the null locals array at `end`. This is
  ;; compiled (JIT'd) but never called by $_start — its reachable path runs into
  ;; the `unreachable` and would trap; being present is enough to exercise the
  ;; codegen path the fix covers.
  (func $dead_block_with_local (result i32) (local i32)
    block
      i32.const 5
      local.set 0
      unreachable
    end
    i32.const 0)

  ;; A block whose fallthrough is dead but which IS reached via an inner `br`:
  ;; the continuation is live, so the merged locals/result must flow through.
  (func $reachable_via_inner_br (param $x i32) (result i32) (local i32)
    block (result i32)
      local.get $x
      local.set 0
      local.get 0
      br 0
      ;; --- unreachable below ---
      unreachable
    end)

  ;; Structured control nested inside dead code: the skipper must track nesting
  ;; so the correct `end` closes the terminated frame.
  (func $nested_dead (result i32)
    i32.const 0
    return
    ;; --- unreachable below ---
    block
      loop
        i32.const 1
        br 0
      end
      unreachable
    end
    unreachable)

  ;; br_table as a terminator, followed by dead code with immediates to skip.
  (func $dead_after_br_table (result i32)
    block
      i32.const 0
      br_table 0 0
      ;; --- unreachable below ---
      i32.const 123
      i32.const 456
      i32.add
      drop
    end
    i32.const 0)

  (func $_start (result i32)
    call $dead_after_terminator     ;; 0
    i32.const 7
    call $reachable_via_inner_br    ;; 7
    i32.add
    call $nested_dead               ;; 0
    i32.add
    call $dead_after_br_table       ;; 0
    i32.add
    ;; sum is 7 → success
    i32.const 7
    i32.ne)

  (export "_start" (func $_start)))
