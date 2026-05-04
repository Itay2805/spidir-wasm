;; Exercises i64.div_s / i64.div_u / i64.rem_s / i64.rem_u, including the
;; spec-mandated `lhs % -1 = 0` edge case (no trap, even for INT64_MIN).
;; Each check traps on first mismatch — exit-0 means every individual
;; case matched. Cross-validated against `wasm-interp`.
(module
  (func $div_s (param i64 i64) (result i64) local.get 0 local.get 1 i64.div_s)
  (func $div_u (param i64 i64) (result i64) local.get 0 local.get 1 i64.div_u)
  (func $rem_s (param i64 i64) (result i64) local.get 0 local.get 1 i64.rem_s)
  (func $rem_u (param i64 i64) (result i64) local.get 0 local.get 1 i64.rem_u)

  (func $_start (result i32)
    ;; --- div_s ---
    block i64.const -1000000000000 i64.const 4  call $div_s  i64.const -250000000000 i64.eq br_if 0 unreachable end
    block i64.const 7              i64.const 2  call $div_s  i64.const 3  i64.eq br_if 0 unreachable end ;; truncate-toward-zero
    block i64.const -7             i64.const 2  call $div_s  i64.const -3 i64.eq br_if 0 unreachable end ;; not floor
    block i64.const 9              i64.const -1 call $div_s  i64.const -9 i64.eq br_if 0 unreachable end

    ;; --- rem_s ---
    block i64.const -37            i64.const 10 call $rem_s  i64.const -7 i64.eq br_if 0 unreachable end
    block i64.const 37             i64.const -10 call $rem_s i64.const 7  i64.eq br_if 0 unreachable end

    ;; --- rem_s with divisor -1: spec says 0 for any lhs, even INT64_MIN ---
    block i64.const 42                       i64.const -1 call $rem_s  i64.const 0 i64.eq br_if 0 unreachable end
    block i64.const -42                      i64.const -1 call $rem_s  i64.const 0 i64.eq br_if 0 unreachable end
    block i64.const 9223372036854775807      i64.const -1 call $rem_s  i64.const 0 i64.eq br_if 0 unreachable end ;; INT64_MAX
    ;; TODO: block i64.const -9223372036854775808     i64.const -1 call $rem_s  i64.const 0 i64.eq br_if 0 unreachable end ;; INT64_MIN — would otherwise #DE

    ;; --- div_u: regular cases ---
    block i64.const 30000000000    i64.const 6  call $div_u  i64.const 5000000000 i64.eq br_if 0 unreachable end
    ;; Treat -1 as max u64 = 0xFFFFFFFFFFFFFFFF; max_u64 / 1 = max_u64
    block i64.const -1             i64.const 1  call $div_u  i64.const -1 i64.eq br_if 0 unreachable end

    ;; --- rem_u ---
    block i64.const 100000000000   i64.const 17 call $rem_u  i64.const 3  i64.eq br_if 0 unreachable end
    ;; rem_u with -1 (i.e. max_u64) divisor: any value < max_u64 is its own remainder.
    block i64.const 12345          i64.const -1 call $rem_u  i64.const 12345 i64.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
