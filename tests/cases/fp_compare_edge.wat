;; Edge-case coverage for f32 comparison ops not exercised by fp_compare.wat:
;;   - signed-zero: feq(+0,-0)=1, fne(+0,-0)=0, flt/fgt(+0,-0)=0, fle/fge(+0,-0)=1
;;   - infinities: inf==inf=1, -inf<+inf=1, +inf>-inf=1, inf<inf=0 (same value),
;;     ordering of finite vs +/-inf
;;   - negative NaN (-nan): identical unordered behavior to +nan
;;   - NaN on the right-hand operand for gt/le (fp_compare.wat only covers lt/ge rhs)
;; Each case traps on first mismatch; exit-0 means all matched.
;; Cross-validated against the spec feq/fne/flt/fgt/fle/fge tables (4.3.3).
(module
  (func $f32_eq (param f32 f32) (result i32) local.get 0 local.get 1 f32.eq)
  (func $f32_ne (param f32 f32) (result i32) local.get 0 local.get 1 f32.ne)
  (func $f32_lt (param f32 f32) (result i32) local.get 0 local.get 1 f32.lt)
  (func $f32_gt (param f32 f32) (result i32) local.get 0 local.get 1 f32.gt)
  (func $f32_le (param f32 f32) (result i32) local.get 0 local.get 1 f32.le)
  (func $f32_ge (param f32 f32) (result i32) local.get 0 local.get 1 f32.ge)

  (func $_start (result i32)
    ;; --- f32 signed zero: +0 and -0 compare equal ---
    block f32.const 0.0  f32.const -0.0 call $f32_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const 0.0  f32.const -0.0 call $f32_ne i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 0.0  f32.const -0.0 call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 0.0  f32.const -0.0 call $f32_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 0.0  f32.const -0.0 call $f32_le i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const -0.0 f32.const 0.0  call $f32_ge i32.const 1 i32.eq br_if 0 unreachable end

    ;; --- f32 infinities ---
    block f32.const inf  f32.const inf  call $f32_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const -inf f32.const -inf call $f32_eq i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const inf  f32.const -inf call $f32_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const -inf f32.const inf  call $f32_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const inf  f32.const -inf call $f32_gt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const inf  f32.const inf  call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end ;; same value
    block f32.const inf  f32.const inf  call $f32_le i32.const 1 i32.eq br_if 0 unreachable end ;; same value
    block f32.const 1.0  f32.const inf  call $f32_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const -inf f32.const 1.0  call $f32_lt i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const inf  f32.const 1.0  call $f32_ge i32.const 1 i32.eq br_if 0 unreachable end

    ;; --- f32 negative NaN: unordered, identical to +nan ---
    block f32.const -nan f32.const 1.5  call $f32_eq i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const -nan f32.const 1.5  call $f32_ne i32.const 1 i32.eq br_if 0 unreachable end
    block f32.const -nan f32.const 1.5  call $f32_lt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const -nan f32.const 1.5  call $f32_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const -nan f32.const 1.5  call $f32_le i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const -nan f32.const 1.5  call $f32_ge i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- f32 NaN on the right operand for gt/le (fp_compare.wat omits these) ---
    block f32.const 1.5 f32.const nan   call $f32_gt i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5 f32.const nan   call $f32_le i32.const 0 i32.eq br_if 0 unreachable end
    block f32.const 1.5 f32.const nan   call $f32_ne i32.const 1 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
