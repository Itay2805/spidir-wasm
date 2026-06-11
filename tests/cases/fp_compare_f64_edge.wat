;; f64 comparison edge-case coverage. fp_compare.wat's f64 section is thin:
;; it has no signed-zero, no infinity ordering, no -nan, and no rhs-NaN for gt/le/ge.
;; Each case traps on first mismatch; exit-0 means all matched.
;; Cross-validated against spec feq/fne/flt/fgt/fle/fge tables (4.3.3).
(module
  (func $f64_eq (param f64 f64) (result i32) local.get 0 local.get 1 f64.eq)
  (func $f64_ne (param f64 f64) (result i32) local.get 0 local.get 1 f64.ne)
  (func $f64_lt (param f64 f64) (result i32) local.get 0 local.get 1 f64.lt)
  (func $f64_gt (param f64 f64) (result i32) local.get 0 local.get 1 f64.gt)
  (func $f64_le (param f64 f64) (result i32) local.get 0 local.get 1 f64.le)
  (func $f64_ge (param f64 f64) (result i32) local.get 0 local.get 1 f64.ge)

  (func $_start (result i32)
    ;; --- f64 signed zero ---
    block f64.const 0.0  f64.const -0.0 call $f64_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const 0.0  f64.const -0.0 call $f64_ne i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 0.0  f64.const -0.0 call $f64_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 0.0  f64.const -0.0 call $f64_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 0.0  f64.const -0.0 call $f64_le i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const -0.0 f64.const 0.0  call $f64_ge i32.const 1 i32.eq br_if 0 unreachable end

    ;; --- f64 infinities ---
    block f64.const inf  f64.const inf  call $f64_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const inf  f64.const -inf call $f64_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const -inf f64.const inf  call $f64_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const inf  f64.const -inf call $f64_gt i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const inf  f64.const inf  call $f64_lt i32.const 0 i32.eq br_if 0 unreachable end ;; same value
    block f64.const inf  f64.const inf  call $f64_le i32.const 1 i32.eq br_if 0 unreachable end ;; same value
    block f64.const 1.0  f64.const inf  call $f64_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const inf  f64.const 1.0  call $f64_ge i32.const 1 i32.eq br_if 0 unreachable end

    ;; --- f64 negative NaN ---
    block f64.const -nan f64.const 1.5  call $f64_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const -nan f64.const 1.5  call $f64_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f64.const -nan f64.const 1.5  call $f64_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const -nan f64.const 1.5  call $f64_le i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const -nan f64.const 1.5  call $f64_ge i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- f64 NaN on the right operand for gt/le/ge ---
    block f64.const 1.5 f64.const nan   call $f64_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 1.5 f64.const nan   call $f64_le i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 1.5 f64.const nan   call $f64_ge i32.const 0 i32.eq br_if 0 unreachable end
    block f64.const 1.5 f64.const nan   call $f64_ne i32.const 1 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
