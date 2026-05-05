;; Exercises function imports. The host provides three test functions under
;; module name "env" — add_i32, mul_i64, and magic — wired up in host/main.c
;; via the resolve_import callback. Each block traps unless the imported call
;; returns the expected value, so exit-0 means every import resolved and the
;; codegen'd extern call delivered the right result.
(module
  (import "env" "add_i32" (func $add_i32 (param i32 i32) (result i32)))
  (import "env" "mul_i64" (func $mul_i64 (param i64 i64) (result i64)))
  (import "env" "magic"   (func $magic                   (result i32)))

  (func $_start (result i32)
    ;; --- add_i32(7, 35) == 42 ---
    block i32.const 7 i32.const 35 call $add_i32 i32.const 42 i32.eq br_if 0 unreachable end

    ;; --- add_i32 with negative arg: (-100) + 100 == 0 ---
    block i32.const -100 i32.const 100 call $add_i32 i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- mul_i64 forces 64-bit args/result through the import boundary ---
    block i64.const 1000000 i64.const 1000000 call $mul_i64 i64.const 1000000000000 i64.eq br_if 0 unreachable end

    ;; --- zero-arg import returns the agreed magic constant ---
    block call $magic i32.const 0xDEADBEEF i32.eq br_if 0 unreachable end

    ;; --- call the same import twice in a row to ensure the extern ref
    ;;     is reusable, not single-shot ---
    block
      i32.const 1 i32.const 2 call $add_i32
      i32.const 3 i32.const 4 call $add_i32
      i32.add
      i32.const 10
      i32.eq br_if 0 unreachable
    end

    i32.const 0)

  (export "_start" (func $_start)))
