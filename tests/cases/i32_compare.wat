;; Exercises the i32 comparison family.
;; Each helper returns a 0/1 i32, and we sum them so a single mismatched
;; comparison is easy to spot. Returns 0 on success.
(module
  (func $eq   (param i32 i32) (result i32) local.get 0 local.get 1 i32.eq)
  (func $ne   (param i32 i32) (result i32) local.get 0 local.get 1 i32.ne)
  (func $lt_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.lt_s)
  (func $le_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.le_s)
  (func $gt_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.gt_s)
  (func $ge_s (param i32 i32) (result i32) local.get 0 local.get 1 i32.ge_s)
  (func $lt_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.lt_u)
  (func $ge_u (param i32 i32) (result i32) local.get 0 local.get 1 i32.ge_u)
  (func $eqz  (param i32)     (result i32) local.get 0 i32.eqz)

  (func $_start (result i32)
    ;; sum of nine "true" comparisons -> 9
    i32.const 7  i32.const 7  call $eq
    i32.const 7  i32.const 8  call $ne
    i32.add
    i32.const -3 i32.const 1  call $lt_s
    i32.add
    i32.const 5  i32.const 5  call $le_s
    i32.add
    i32.const 10 i32.const 2  call $gt_s
    i32.add
    i32.const -1 i32.const -2 call $ge_s
    i32.add
    i32.const 1  i32.const 0xFFFFFFFF call $lt_u
    i32.add
    i32.const 0xFFFFFFFF i32.const 1 call $ge_u
    i32.add
    i32.const 0  call $eqz
    i32.add
    i32.const 9
    i32.ne)

  (export "_start" (func $_start)))
