;; Multiple globals of all four numeric types: mutable set/get round-trip,
;; immutable global.get (materialized as const), and non-aliasing.
(module
  (global $gi (mut i32) (i32.const 0))
  (global $gj (mut i64) (i64.const 0))
  (global $gf (mut f32) (f32.const 0))
  (global $gd (mut f64) (f64.const 0))
  (global $imm_i32 i32 (i32.const 777))
  (global $imm_f64 f64 (f64.const 2.5))
  (func $_start (result i32)
    (local $sum i32)
    i32.const -42 global.set $gi
    global.get $gi i32.const -42 i32.eq
    local.get $sum i32.add local.set $sum
    i64.const 9000000000 global.set $gj
    global.get $gj i64.const 9000000000 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -3.25 global.set $gf
    global.get $gf f32.const -3.25 f32.eq
    local.get $sum i32.add local.set $sum
    f64.const 6.75 global.set $gd
    global.get $gd f64.const 6.75 f64.eq
    local.get $sum i32.add local.set $sum
    ;; destructive overwrite
    i32.const 123 global.set $gi
    global.get $gi i32.const 123 i32.eq
    local.get $sum i32.add local.set $sum
    ;; immutable global.get
    global.get $imm_i32 i32.const 777 i32.eq
    local.get $sum i32.add local.set $sum
    global.get $imm_f64 f64.const 2.5 f64.eq
    local.get $sum i32.add local.set $sum
    ;; non-aliasing of distinct globals
    i32.const 11 global.set $gi
    i64.const 22 global.set $gj
    global.get $gi i32.const 11 i32.eq
    local.get $sum i32.add local.set $sum
    global.get $gj i64.const 22 i64.eq
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 9
    i32.ne)
  (export "_start" (func $_start)))
