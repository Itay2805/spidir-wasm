;; Exercises the four reinterpret (bitcast) opcodes:
;;   i32.reinterpret_f32 0xBC, i64.reinterpret_f64 0xBD,
;;   f32.reinterpret_i32 0xBE, f64.reinterpret_i64 0xBF
;;
;; reinterpret copies the raw bit pattern unchanged (spec 4.3.1) — it is total
;; (never traps) and preserves sign, exponent and the full mantissa, including
;; signed zero and NaN payloads. Tests compare on the INTEGER side with i32/i64
;; .eq, which is exact for every bit pattern (unlike f*.eq, which treats +0==-0
;; and makes every NaN compare unequal). Returns 0 on success.
;;
;; KNOWN FAILING: the chained round-trip checks below currently miscompile
;; (bitcast store/load-forwarding bug — distinct stack slots alias). Kept as-is
;; to track the fix; see the minimal reproducer in the bug report.
(module
  (func $_start (result i32)
    (local $sum i32)

    ;; ---- f32 -> i32 : exact IEEE-754 bit patterns ---------------------------
    f32.const 1.0  i32.reinterpret_f32 i32.const 0x3F800000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const 2.0  i32.reinterpret_f32 i32.const 0x40000000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -2.0 i32.reinterpret_f32 i32.const 0xC0000000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const 0.0  i32.reinterpret_f32 i32.const 0x00000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; -0.0 has the sign bit set — reinterpret is the only way to see it
    f32.const -0.0 i32.reinterpret_f32 i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const inf  i32.reinterpret_f32 i32.const 0x7F800000 i32.eq
    local.get $sum i32.add local.set $sum
    f32.const -inf i32.reinterpret_f32 i32.const 0xFF800000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; a non-canonical NaN: the full payload must survive untouched
    f32.const nan:0x355555 i32.reinterpret_f32 i32.const 0x7FB55555 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- i32 -> f32 : normal values compare fine with f32.eq ----------------
    i32.const 0x40000000 f32.reinterpret_i32 f32.const 2.0  f32.eq
    local.get $sum i32.add local.set $sum
    i32.const 0x3F800000 f32.reinterpret_i32 f32.const 1.0  f32.eq
    local.get $sum i32.add local.set $sum
    i32.const 0xBF800000 f32.reinterpret_i32 f32.const -1.0 f32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f32 round-trips (bit-exact through both directions) ----------------
    ;; ordinary value survives f32 -> i32 -> f32
    f32.const 0.5 i32.reinterpret_f32 f32.reinterpret_i32 f32.const 0.5 f32.eq
    local.get $sum i32.add local.set $sum
    ;; -0.0 bits survive i32 -> f32 -> i32 (f32.eq could not tell -0 from +0)
    i32.const 0x80000000 f32.reinterpret_i32 i32.reinterpret_f32 i32.const 0x80000000 i32.eq
    local.get $sum i32.add local.set $sum
    ;; NaN payload survives i32 -> f32 -> i32
    i32.const 0x7FB55555 f32.reinterpret_i32 i32.reinterpret_f32 i32.const 0x7FB55555 i32.eq
    local.get $sum i32.add local.set $sum

    ;; ---- f64 -> i64 ---------------------------------------------------------
    f64.const 1.0  i64.reinterpret_f64 i64.const 0x3FF0000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const 2.0  i64.reinterpret_f64 i64.const 0x4000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -0.0 i64.reinterpret_f64 i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const inf  i64.reinterpret_f64 i64.const 0x7FF0000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const -inf i64.reinterpret_f64 i64.const 0xFFF0000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const nan:0x4000000000000 i64.reinterpret_f64 i64.const 0x7FF4000000000000 i64.eq
    local.get $sum i32.add local.set $sum

    ;; ---- i64 -> f64 + round-trips -------------------------------------------
    i64.const 0x4000000000000000 f64.reinterpret_i64 f64.const 2.0 f64.eq
    local.get $sum i32.add local.set $sum
    i64.const 0x3FF0000000000000 f64.reinterpret_i64 f64.const 1.0 f64.eq
    local.get $sum i32.add local.set $sum
    ;; ordinary value survives f64 -> i64 -> f64
    f64.const 0.5 i64.reinterpret_f64 f64.reinterpret_i64 f64.const 0.5 f64.eq
    local.get $sum i32.add local.set $sum
    ;; -0.0 bits survive i64 -> f64 -> i64
    i64.const 0x8000000000000000 f64.reinterpret_i64 i64.reinterpret_f64 i64.const 0x8000000000000000 i64.eq
    local.get $sum i32.add local.set $sum
    ;; NaN payload survives i64 -> f64 -> i64
    i64.const 0x7FF4000000000000 f64.reinterpret_i64 i64.reinterpret_f64 i64.const 0x7FF4000000000000 i64.eq
    local.get $sum i32.add local.set $sum

    ;; expected: 25 successful checks -> 25
    local.get $sum
    i32.const 25
    i32.ne)

  (export "_start" (func $_start)))
