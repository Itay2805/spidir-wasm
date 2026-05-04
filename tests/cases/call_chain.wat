;; Exercises multi-level direct calls. Each helper forwards to the next so
;; the JIT has to materialize a chain of internal calls.
;; Returns 0 on success.
(module
  (func $leaf   (param i32) (result i32) local.get 0 i32.const 1 i32.add)
  (func $level2 (param i32) (result i32) local.get 0 call $leaf   i32.const 2 i32.add)
  (func $level3 (param i32) (result i32) local.get 0 call $level2 i32.const 3 i32.add)
  (func $level4 (param i32) (result i32) local.get 0 call $level3 i32.const 4 i32.add)

  (func $_start (result i32)
    ;; level4(0) = ((((0+1)+2)+3)+4) = 10
    i32.const 0
    call $level4
    i32.const 10
    i32.ne)

  (export "_start" (func $_start)))
