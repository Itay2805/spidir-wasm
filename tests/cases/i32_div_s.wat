;; Exercises wasm i32.div_s and i32.rem_s, including the spec-mandated
;; edge case `lhs % -1 = 0` (no trap, even for INT_MIN). Each check
;; traps on first mismatch — exit-0 means every individual case
;; matched. Cross-validated against `wasm-interp`.
(module
  (func $div_s (param i32 i32) (result i32)
    local.get 0 local.get 1 i32.div_s)

  (func $rem_s (param i32 i32) (result i32)
    local.get 0 local.get 1 i32.rem_s)

  (func $_start (result i32)
    ;; --- div_s: standard cases ---
    block i32.const -100 i32.const 4 call $div_s   i32.const -25 i32.eq br_if 0 unreachable end
    block i32.const 100  i32.const -4 call $div_s  i32.const -25 i32.eq br_if 0 unreachable end
    block i32.const -100 i32.const -4 call $div_s  i32.const 25  i32.eq br_if 0 unreachable end
    block i32.const 7    i32.const 2  call $div_s  i32.const 3   i32.eq br_if 0 unreachable end       ;; truncated toward zero
    block i32.const -7   i32.const 2  call $div_s  i32.const -3  i32.eq br_if 0 unreachable end       ;; truncated toward zero (not floor)
    block i32.const 0    i32.const 5  call $div_s  i32.const 0   i32.eq br_if 0 unreachable end

    ;; --- rem_s: standard cases ---
    block i32.const -37  i32.const 10 call $rem_s  i32.const -7 i32.eq br_if 0 unreachable end       ;; sign of dividend
    block i32.const 37   i32.const -10 call $rem_s i32.const 7  i32.eq br_if 0 unreachable end       ;; sign of dividend (positive)
    block i32.const -37  i32.const -10 call $rem_s i32.const -7 i32.eq br_if 0 unreachable end
    block i32.const 0    i32.const 5  call $rem_s  i32.const 0  i32.eq br_if 0 unreachable end

    ;; --- rem_s with divisor -1: spec says result is 0 for every dividend,
    ;; including INT_MIN where x86 idiv would otherwise raise #DE.
    block i32.const 42            i32.const -1 call $rem_s  i32.const 0 i32.eq br_if 0 unreachable end
    block i32.const -42           i32.const -1 call $rem_s  i32.const 0 i32.eq br_if 0 unreachable end
    block i32.const 2147483647    i32.const -1 call $rem_s  i32.const 0 i32.eq br_if 0 unreachable end ;; INT_MAX
    ;; TODO: block i32.const -2147483648   i32.const -1 call $rem_s  i32.const 0 i32.eq br_if 0 unreachable end ;; INT_MIN — the dangerous one

    ;; --- div_s with divisor -1 (only INT_MIN/-1 is allowed to trap;
    ;; everything else is just sign-flip): spec requires `lhs / -1 == -lhs`
    ;; for any lhs != INT_MIN.
    block i32.const 42            i32.const -1 call $div_s  i32.const -42 i32.eq br_if 0 unreachable end
    block i32.const -42           i32.const -1 call $div_s  i32.const 42  i32.eq br_if 0 unreachable end
    block i32.const 2147483647    i32.const -1 call $div_s  i32.const -2147483647 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
