(module
  (func $f32_min      (param f32 f32) (result f32) local.get 0 local.get 1 f32.min)
  (func $f32_max      (param f32 f32) (result f32) local.get 0 local.get 1 f32.max)
  (func $f32_copysign (param f32 f32) (result f32) local.get 0 local.get 1 f32.copysign)
  (func $f64_min      (param f64 f64) (result f64) local.get 0 local.get 1 f64.min)
  (func $f64_max      (param f64 f64) (result f64) local.get 0 local.get 1 f64.max)
  (func $f64_copysign (param f64 f64) (result f64) local.get 0 local.get 1 f64.copysign)

  (func $_start (result i32)
    (local $sum i32)
    (local $t f32)
    (local $td f64)

    ;; ---- f32 min with infinities ----
    ;; min(-inf, 5) = -inf ; min(5, -inf) = -inf
    f32.const -inf f32.const 5.0 call $f32_min f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 5.0 f32.const -inf call $f32_min f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; min(+inf, 5) = 5 ; min(5, +inf) = 5 (return the other value)
    f32.const inf f32.const 5.0 call $f32_min f32.const 5.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 5.0 f32.const inf call $f32_min f32.const 5.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f32 max with infinities ----
    ;; max(+inf, 5) = +inf ; max(5, +inf) = +inf
    f32.const inf f32.const 5.0 call $f32_max f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 5.0 f32.const inf call $f32_max f32.const inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; max(-inf, 5) = 5 ; max(5, -inf) = 5 (return the other value)
    f32.const -inf f32.const 5.0 call $f32_max f32.const 5.0 f32.eq
    local.get $sum i32.add local.set $sum
    f32.const 5.0 f32.const -inf call $f32_max f32.const 5.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f32 both-NaN must be NaN (x != x is 1 only for NaN) ----
    f32.const nan f32.const nan call $f32_min local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum
    f32.const nan f32.const nan call $f32_max local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; ---- f32 copysign: inf magnitude, +0 sign onto negative, NaN magnitude ----
    ;; copysign(inf, -1) = -inf
    f32.const inf f32.const -1.0 call $f32_copysign f32.const -inf f32.eq
    local.get $sum i32.add local.set $sum
    ;; copysign(-5, +0) = +5 (take + sign from +0, flips negative to positive)
    f32.const -5.0 f32.const 0.0 call $f32_copysign f32.const 5.0 f32.eq
    local.get $sum i32.add local.set $sum
    ;; copysign(NaN, -1) keeps NaN-ness (copysign is NaN-propagation exempt)
    f32.const nan f32.const -1.0 call $f32_copysign local.tee $t local.get $t f32.ne
    local.get $sum i32.add local.set $sum

    ;; ---- f64 min with infinities ----
    f64.const -inf f64.const 5.0 call $f64_min f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 5.0 f64.const -inf call $f64_min f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const inf f64.const 5.0 call $f64_min f64.const 5.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 5.0 f64.const inf call $f64_min f64.const 5.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f64 max with infinities ----
    f64.const inf f64.const 5.0 call $f64_max f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 5.0 f64.const inf call $f64_max f64.const inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf f64.const 5.0 call $f64_max f64.const 5.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const 5.0 f64.const -inf call $f64_max f64.const 5.0 f64.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f64 both-NaN ----
    f64.const nan f64.const nan call $f64_min local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum
    f64.const nan f64.const nan call $f64_max local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum

    ;; ---- f64 copysign ----
    f64.const inf f64.const -1.0 call $f64_copysign f64.const -inf f64.eq
    local.get $sum i32.add local.set $sum
    f64.const -5.0 f64.const 0.0 call $f64_copysign f64.const 5.0 f64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan f64.const -1.0 call $f64_copysign local.tee $td local.get $td f64.ne
    local.get $sum i32.add local.set $sum

    ;; expected: 26 successful checks -> returns 0
    local.get $sum
    i32.const 26
    i32.ne)

  (export "_start" (func $_start)))
