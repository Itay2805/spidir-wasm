;; Direct `call` coverage the existing call_chain.wat misses:
;;   - mixed-type params (i32,i64,f64,i32) in one call, args spilling
;;   - i64 result and f64 result through a direct call
;;   - a function with NO result (result_types_count == 0 path in handler)
;;     observed via a global side-effect.
;; exit-0 == all checks pass.
(module
  (global $g (mut i32) (i32.const 0))
  (func $mix (param $a i32) (param $b i64) (param $c f64) (param $d i32) (result i64)
    local.get $b
    local.get $a local.get $d i32.add i64.extend_i32_s
    i64.add)                        ;; = b + (a+d)
  (func $f64id (param $x f64) (result f64) local.get $x f64.const 0.0 f64.add)
  (func $setg (param $v i32)            ;; no result
    local.get $v global.set $g)

  (func $_start (result i32)
    (local $sum i32)
    i32.const 3 i64.const 1000 f64.const 2.5 i32.const 4 call $mix
    i64.const 1007 i64.eq
    local.get $sum i32.add local.set $sum
    f64.const 42.25 call $f64id f64.const 42.25 f64.eq
    local.get $sum i32.add local.set $sum
    i32.const 99 call $setg
    global.get $g i32.const 99 i32.eq
    local.get $sum i32.add local.set $sum
    local.get $sum i32.const 3 i32.ne)
  (export "_start" (func $_start)))
