;; Exercises the f32 / f64 comparison families, including the wasm
;; ordered-compare semantics for NaN: NaN compared to anything (including
;; itself) is `eq=false / ne=true / lt=false / le=false / gt=false / ge=false`.
;; Each check traps on first mismatch — exit-0 means every case matched.
;; Cross-validated against `wasm-interp`.
(module
  (func $f32_eq (param f32 f32) (result i32) local.get 0 local.get 1 f32.eq)
  (func $f32_ne (param f32 f32) (result i32) local.get 0 local.get 1 f32.ne)
  (func $f32_lt (param f32 f32) (result i32) local.get 0 local.get 1 f32.lt)
  (func $f32_gt (param f32 f32) (result i32) local.get 0 local.get 1 f32.gt)
  (func $f32_le (param f32 f32) (result i32) local.get 0 local.get 1 f32.le)
  (func $f32_ge (param f32 f32) (result i32) local.get 0 local.get 1 f32.ge)

  (func $f64_eq (param f64 f64) (result i32) local.get 0 local.get 1 f64.eq)
  (func $f64_ne (param f64 f64) (result i32) local.get 0 local.get 1 f64.ne)
  (func $f64_lt (param f64 f64) (result i32) local.get 0 local.get 1 f64.lt)
  (func $f64_gt (param f64 f64) (result i32) local.get 0 local.get 1 f64.gt)
  (func $f64_le (param f64 f64) (result i32) local.get 0 local.get 1 f64.le)
  (func $f64_ge (param f64 f64) (result i32) local.get 0 local.get 1 f64.ge)

  (func $_start (result i32)
    ;; --- f32 ordered cases ---
    block f32.const 1.5  f32.const 1.5  call $f32_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 2.0  call $f32_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 2.0  call $f32_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 1.5  call $f32_ne i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 2.0  call $f32_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 2.0  f32.const 1.5  call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 3.0  f32.const 1.5  call $f32_gt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 1.5  call $f32_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 1.5  call $f32_le i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 2.0  f32.const 1.5  call $f32_le i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 2.0  f32.const 1.5  call $f32_ge i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 1.5  f32.const 2.0  call $f32_ge i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- f32 NaN cases: every ordered op except `ne` returns 0 when
    ;; either operand is NaN; `ne` returns 1.
    block f32.const nan f32.const 1.5  call $f32_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const nan  call $f32_eq i32.const 0 i32.eq br_if 0 unreachable end ;; NaN != NaN even when "equal"
    block f32.const nan f32.const 1.5  call $f32_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const nan  call $f32_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const 1.5  call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const 1.5  call $f32_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const 1.5  call $f32_le i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const nan f32.const 1.5  call $f32_ge i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5 f32.const nan  call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end ;; NaN on rhs too
    block f32.const 1.5 f32.const nan  call $f32_ge i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- f64 ordered cases ---
    block f64.const 1.5  f64.const 1.5  call $f64_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 1.5  f64.const 2.0  call $f64_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 1.5  f64.const 2.0  call $f64_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 3.0  f64.const 1.5  call $f64_gt i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 1.5  f64.const 1.5  call $f64_le i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 2.0  f64.const 1.5  call $f64_ge i32.const 1 i32.eq br_if 0 unreachable end

    ;; --- f64 NaN cases ---
    block f64.const nan f64.const 1.5  call $f64_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const nan  call $f64_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const 1.5  call $f64_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const 1.5  call $f64_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const 1.5  call $f64_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const 1.5  call $f64_le i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const nan f64.const 1.5  call $f64_ge i32.const 0 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
