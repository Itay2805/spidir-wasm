;; trunc_sat (saturating float->int truncation), all 8 opcodes 0xFC 0x00-0x07.
;; Spec 4.3.3 itruncsat: truncate toward zero; NaN -> 0; out-of-range saturates
;; to the integer min/max (never traps). Results are compared bit-exact with
;; i32.eq / i64.eq. Returns 0 on success.
(module
  (func (export "_start") (result i32)
    (local $sum i32)
    ;; i32.trunc_sat_f32_s
    f32.const 2.5 i32.trunc_sat_f32_s i32.const 2 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -2.5 i32.trunc_sat_f32_s i32.const -2 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const 7.9 i32.trunc_sat_f32_s i32.const 7 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -7.9 i32.trunc_sat_f32_s i32.const -7 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const nan i32.trunc_sat_f32_s i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const inf i32.trunc_sat_f32_s i32.const 0x7FFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf i32.trunc_sat_f32_s i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const 2147483648.0 i32.trunc_sat_f32_s i32.const 0x7FFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -2147483648.0 i32.trunc_sat_f32_s i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; i32.trunc_sat_f32_u
    f32.const 2.5 i32.trunc_sat_f32_u i32.const 2 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.5 i32.trunc_sat_f32_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const nan i32.trunc_sat_f32_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const inf i32.trunc_sat_f32_u i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf i32.trunc_sat_f32_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const 4294967296.0 i32.trunc_sat_f32_u i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    ;; i32.trunc_sat_f64_s
    f64.const 2.5 i32.trunc_sat_f64_s i32.const 2 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const -2.5 i32.trunc_sat_f64_s i32.const -2 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const nan i32.trunc_sat_f64_s i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const inf i32.trunc_sat_f64_s i32.const 0x7FFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf i32.trunc_sat_f64_s i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const 2147483648.0 i32.trunc_sat_f64_s i32.const 0x7FFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    ;; i32.trunc_sat_f64_u
    f64.const 2.5 i32.trunc_sat_f64_u i32.const 2 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.5 i32.trunc_sat_f64_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const nan i32.trunc_sat_f64_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const inf i32.trunc_sat_f64_u i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf i32.trunc_sat_f64_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    f64.const 4294967296.0 i32.trunc_sat_f64_u i32.const 0xFFFFFFFF i32.eq
    local.get $sum i32.add local.set $sum
    ;; i64.trunc_sat_f32_s
    f32.const 2.5 i64.trunc_sat_f32_s i64.const 2 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -2.5 i64.trunc_sat_f32_s i64.const -2 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const nan i64.trunc_sat_f32_s i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const inf i64.trunc_sat_f32_s i64.const 0x7FFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf i64.trunc_sat_f32_s i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const 100.5 i64.trunc_sat_f32_s i64.const 100 i64.eq
    local.get $sum i32.add local.set $sum
    ;; i64.trunc_sat_f32_u
    f32.const 2.5 i64.trunc_sat_f32_u i64.const 2 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -0.5 i64.trunc_sat_f32_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const nan i64.trunc_sat_f32_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const inf i64.trunc_sat_f32_u i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf i64.trunc_sat_f32_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    ;; i64.trunc_sat_f64_s
    f64.const 2.5 i64.trunc_sat_f64_s i64.const 2 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -2.5 i64.trunc_sat_f64_s i64.const -2 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan i64.trunc_sat_f64_s i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const inf i64.trunc_sat_f64_s i64.const 0x7FFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf i64.trunc_sat_f64_s i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const 9007199254740993.0 i64.trunc_sat_f64_s i64.const 9007199254740992 i64.eq
    local.get $sum i32.add local.set $sum
    ;; i64.trunc_sat_f64_u
    f64.const 2.5 i64.trunc_sat_f64_u i64.const 2 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.5 i64.trunc_sat_f64_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan i64.trunc_sat_f64_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const inf i64.trunc_sat_f64_u i64.const 0xFFFFFFFFFFFFFFFF i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf i64.trunc_sat_f64_u i64.const 0 i64.eq
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 49
    i32.ne))
