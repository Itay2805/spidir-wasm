;; i32/i64 rem_s sign-of-dividend matrix + div_s truncation toward zero, plus
;; the non-trapping INT_MIN%-1=0 / INT_MIN/1 boundaries. Returns 0 iff all 16 pass.
(module
  (func $ds (param i32 i32) (result i32) local.get 0 local.get 1 i32.div_s)
  (func $rs (param i32 i32) (result i32) local.get 0 local.get 1 i32.rem_s)
  (func $ds64 (param i64 i64) (result i64) local.get 0 local.get 1 i64.div_s)
  (func $rs64 (param i64 i64) (result i64) local.get 0 local.get 1 i64.rem_s)
  (func $_start (result i32)
    (local $sum i32)
    ;; rem_s sign follows DIVIDEND (4 quadrants)
    i32.const  7 i32.const  3 call $rs i32.const  1 i32.eq local.get $sum i32.add local.set $sum
    i32.const  7 i32.const -3 call $rs i32.const  1 i32.eq local.get $sum i32.add local.set $sum
    i32.const -7 i32.const  3 call $rs i32.const -1 i32.eq local.get $sum i32.add local.set $sum
    i32.const -7 i32.const -3 call $rs i32.const -1 i32.eq local.get $sum i32.add local.set $sum
    ;; div_s truncates toward zero (4 quadrants)
    i32.const  7 i32.const  3 call $ds i32.const  2 i32.eq local.get $sum i32.add local.set $sum
    i32.const  7 i32.const -3 call $ds i32.const -2 i32.eq local.get $sum i32.add local.set $sum
    i32.const -7 i32.const  3 call $ds i32.const -2 i32.eq local.get $sum i32.add local.set $sum
    i32.const -7 i32.const -3 call $ds i32.const  2 i32.eq local.get $sum i32.add local.set $sum
    ;; div_s boundaries: INT_MIN/1, INT_MIN/2, -1/-1
    i32.const -2147483648 i32.const 1 call $ds i32.const -2147483648 i32.eq local.get $sum i32.add local.set $sum
    i32.const -2147483648 i32.const 2 call $ds i32.const -1073741824 i32.eq local.get $sum i32.add local.set $sum
    i32.const -1 i32.const -1 call $ds i32.const 1 i32.eq local.get $sum i32.add local.set $sum
    ;; rem_s INT_MIN%-1 == 0 (must NOT trap; masking-trick edge)
    i32.const -2147483648 i32.const -1 call $rs i32.const 0 i32.eq local.get $sum i32.add local.set $sum
    ;; i64 dangerous boundaries
    i64.const -9223372036854775808 i64.const -1 call $rs64 i64.const 0 i64.eq local.get $sum i32.add local.set $sum
    i64.const -9223372036854775808 i64.const  1 call $ds64 i64.const -9223372036854775808 i64.eq local.get $sum i32.add local.set $sum
    i64.const -9223372036854775808 i64.const  2 call $ds64 i64.const -4611686018427387904 i64.eq local.get $sum i32.add local.set $sum
    i64.const 100000000000 i64.const -7 call $rs64 i64.const 5 i64.eq local.get $sum i32.add local.set $sum
    local.get $sum i32.const 16 i32.ne)
  (export "_start" (func $_start)))
