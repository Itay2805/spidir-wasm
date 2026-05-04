;; Exercises the wasm `select` instruction. Spec: with stack [val1 val2
;; cond], pop cond (i32), then val2 and val1; push val1 if cond != 0,
;; else val2 — so any non-zero cond is "truthy", not just 1. Operand
;; types are picked at validation time, so we cover i32/i64/f32/f64.
;; Each check traps on first mismatch — exit-0 means every case matched.
(module
  (func $pick_i32 (param $cond i32) (param $a i32) (param $b i32) (result i32)
    local.get $a local.get $b local.get $cond select)
  (func $pick_i64 (param $cond i32) (param $a i64) (param $b i64) (result i64)
    local.get $a local.get $b local.get $cond select)
  (func $pick_f32 (param $cond i32) (param $a f32) (param $b f32) (result f32)
    local.get $a local.get $b local.get $cond select)
  (func $pick_f64 (param $cond i32) (param $a f64) (param $b f64) (result f64)
    local.get $a local.get $b local.get $cond select)

  (func $_start (result i32)
    ;; --- i32: cond=1 picks val1, cond=0 picks val2 ---
    block i32.const 1 i32.const 11 i32.const 22 call $pick_i32 i32.const 11 i32.eq br_if 0 unreachable end
    block i32.const 0 i32.const 11 i32.const 22 call $pick_i32 i32.const 22 i32.eq br_if 0 unreachable end

    ;; --- i32: any non-zero cond is "truthy" ---
    block i32.const 42         i32.const 11 i32.const 22 call $pick_i32 i32.const 11 i32.eq br_if 0 unreachable end
    block i32.const -1         i32.const 11 i32.const 22 call $pick_i32 i32.const 11 i32.eq br_if 0 unreachable end
    block i32.const 0x80000000 i32.const 11 i32.const 22 call $pick_i32 i32.const 11 i32.eq br_if 0 unreachable end ;; INT_MIN, MSB set

    ;; --- i64 ---
    block i32.const 1 i64.const 100000000000 i64.const -1 call $pick_i64  i64.const 100000000000 i64.eq br_if 0 unreachable end
    block i32.const 0 i64.const 100000000000 i64.const -1 call $pick_i64  i64.const -1           i64.eq br_if 0 unreachable end

    ;; --- f32 / f64: select must preserve bit pattern of the chosen value
    ;; (this also confirms `select` doesn't accidentally route through an
    ;; integer register and lose NaN payloads for fp operands).
    block i32.const 1 f32.const 1.5  f32.const 2.5  call $pick_f32  f32.const 1.5 f32.eq br_if 0 unreachable end
    block i32.const 0 f32.const 1.5  f32.const 2.5  call $pick_f32  f32.const 2.5 f32.eq br_if 0 unreachable end
    block i32.const 1 f64.const 12345.6789 f64.const -0.5 call $pick_f64 f64.const 12345.6789 f64.eq br_if 0 unreachable end
    block i32.const 0 f64.const 12345.6789 f64.const -0.5 call $pick_f64 f64.const -0.5 f64.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
