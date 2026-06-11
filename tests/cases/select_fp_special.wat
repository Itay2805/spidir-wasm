;; select with fp operands that are special values: confirms the chosen
;; operand's exact bit pattern survives the brcond/phi lowering.
;;   - select of -0.0 keeps the sign bit (probe 1.0/x = -inf; note operand
;;     order: push 1.0 THEN the select result, so f32.div computes 1.0/x).
;;   - select of NaN stays a NaN (x x f32.ne).
;;   - select of +/-inf stays infinite.
;; Each successful check adds 1; module returns 0 iff $sum == 12.
(module
  (func $sel_f32 (param $cond i32) (param $a f32) (param $b f32) (result f32)
    local.get $a local.get $b local.get $cond select)
  (func $sel_f64 (param $cond i32) (param $a f64) (param $b f64) (result f64)
    local.get $a local.get $b local.get $cond select)

  (func $_start (result i32)
    (local $sum i32)
    (local $t f32)
    (local $u f64)

    ;; f32: -0.0 sign survives via val1 (1.0 / -0.0 = -inf)
    f32.const 1.0 i32.const 1 f32.const -0.0 f32.const 5.0 call $sel_f32 f32.div
    f32.const -inf f32.eq local.get $sum i32.add local.set $sum

    ;; f32: -0.0 sign survives via val2 (cond=0)
    f32.const 1.0 i32.const 0 f32.const 5.0 f32.const -0.0 call $sel_f32 f32.div
    f32.const -inf f32.eq local.get $sum i32.add local.set $sum

    ;; f32: +0.0 via val1 stays +0.0 (1.0 / +0.0 = +inf)
    f32.const 1.0 i32.const 1 f32.const 0.0 f32.const 5.0 call $sel_f32 f32.div
    f32.const inf f32.eq local.get $sum i32.add local.set $sum

    ;; f32: NaN (val1) stays NaN
    i32.const 1 f32.const nan f32.const 5.0 call $sel_f32
    local.tee $t local.get $t f32.ne local.get $sum i32.add local.set $sum

    ;; f32: +inf (val1) survives
    i32.const 1 f32.const inf f32.const 5.0 call $sel_f32
    f32.const inf f32.eq local.get $sum i32.add local.set $sum

    ;; f32: -inf (val2, cond=0) survives
    i32.const 0 f32.const 5.0 f32.const -inf call $sel_f32
    f32.const -inf f32.eq local.get $sum i32.add local.set $sum

    ;; f64: -0.0 sign survives via val1
    f64.const 1.0 i32.const 1 f64.const -0.0 f64.const 5.0 call $sel_f64 f64.div
    f64.const -inf f64.eq local.get $sum i32.add local.set $sum

    ;; f64: -0.0 sign survives via val2
    f64.const 1.0 i32.const 0 f64.const 5.0 f64.const -0.0 call $sel_f64 f64.div
    f64.const -inf f64.eq local.get $sum i32.add local.set $sum

    ;; f64: +0.0 via val1 stays +0.0
    f64.const 1.0 i32.const 1 f64.const 0.0 f64.const 5.0 call $sel_f64 f64.div
    f64.const inf f64.eq local.get $sum i32.add local.set $sum

    ;; f64: NaN (val1) stays NaN
    i32.const 1 f64.const nan f64.const 5.0 call $sel_f64
    local.tee $u local.get $u f64.ne local.get $sum i32.add local.set $sum

    ;; f64: +inf (val1) survives
    i32.const 1 f64.const inf f64.const 5.0 call $sel_f64
    f64.const inf f64.eq local.get $sum i32.add local.set $sum

    ;; f64: -inf (val2, cond=0) survives
    i32.const 0 f64.const 5.0 f64.const -inf call $sel_f64
    f64.const -inf f64.eq local.get $sum i32.add local.set $sum

    local.get $sum i32.const 12 i32.ne)
  (export "_start" (func $_start)))
