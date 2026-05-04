;; Exercises every i64 comparison opcode so an off-by-one in the JIT's
;; comparison table would be caught. Returns 0 on success.
(module
  (func $eq    (param i64 i64) (result i32) local.get 0 local.get 1 i64.eq)
  (func $ne    (param i64 i64) (result i32) local.get 0 local.get 1 i64.ne)
  (func $lt_s  (param i64 i64) (result i32) local.get 0 local.get 1 i64.lt_s)
  (func $lt_u  (param i64 i64) (result i32) local.get 0 local.get 1 i64.lt_u)
  (func $gt_s  (param i64 i64) (result i32) local.get 0 local.get 1 i64.gt_s)
  (func $gt_u  (param i64 i64) (result i32) local.get 0 local.get 1 i64.gt_u)
  (func $le_s  (param i64 i64) (result i32) local.get 0 local.get 1 i64.le_s)
  (func $le_u  (param i64 i64) (result i32) local.get 0 local.get 1 i64.le_u)
  (func $ge_s  (param i64 i64) (result i32) local.get 0 local.get 1 i64.ge_s)
  (func $ge_u  (param i64 i64) (result i32) local.get 0 local.get 1 i64.ge_u)
  (func $eqz   (param i64)     (result i32) local.get 0 i64.eqz)

  (func $_start (result i32)
    ;; sum of eleven "true" comparisons -> 11
    i64.const 0x10000000000 i64.const 0x10000000000 call $eq
    i64.const 0x10000000000 i64.const 0x20000000000 call $ne
    i32.add
    i64.const -1            i64.const 1             call $lt_s
    i32.add
    i64.const 1             i64.const 0xFFFFFFFFFFFFFFFF call $lt_u
    i32.add
    i64.const 0x100000000   i64.const 0             call $gt_s
    i32.add
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 1        call $gt_u
    i32.add
    i64.const 5             i64.const 5             call $le_s
    i32.add
    i64.const 5             i64.const 5             call $le_u
    i32.add
    i64.const -1            i64.const -2            call $ge_s
    i32.add
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 1        call $ge_u
    i32.add
    i64.const 0             call $eqz
    i32.add
    i32.const 11
    i32.ne)

  (export "_start" (func $_start)))
