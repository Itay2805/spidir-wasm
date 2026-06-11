;; Round-trips all four numeric types through local.set/get and local.tee,
;; exercises params-vs-declared locals, multiple locals, destructive set.
(module
  ;; local.tee returns the value AND stores it; p + t == 2*p proves both.
  (func $tee_i64 (param $p i64) (result i64)
    (local $t i64)
    local.get $p
    local.tee $t
    local.get $t
    i64.add)
  (func $_start (result i32)
    (local $sum i32)
    (local $li i32) (local $lj i64) (local $lf f32) (local $ld f64)
    i32.const -123456789 local.set $li
    local.get $li i32.const -123456789 i32.eq
    local.get $sum i32.add local.set $sum
    i64.const -1234567890123456789 local.set $lj
    local.get $lj i64.const -1234567890123456789 i64.eq
    local.get $sum i32.add local.set $sum
    f32.const -2.5 local.set $lf
    local.get $lf f32.const -2.5 f32.eq
    local.get $sum i32.add local.set $sum
    f64.const 1234.5 local.set $ld
    local.get $ld f64.const 1234.5 f64.eq
    local.get $sum i32.add local.set $sum
    ;; local.tee leaves value on stack AND stores it
    i32.const 7
    local.tee $li
    i32.const 7 i32.eq
    local.get $sum i32.add local.set $sum
    local.get $li i32.const 7 i32.eq
    local.get $sum i32.add local.set $sum
    ;; i64 tee via helper: 2*21 == 42
    i64.const 21 call $tee_i64 i64.const 42 i64.eq
    local.get $sum i32.add local.set $sum
    ;; set is destructive
    i32.const 100 local.set $li
    local.get $li i32.const 100 i32.eq
    local.get $sum i32.add local.set $sum
    local.get $sum
    i32.const 8
    i32.ne)
  (export "_start" (func $_start)))
