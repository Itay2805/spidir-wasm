;; drop across all value types, and that drop removes ONLY the top of stack
;; (the value beneath is untouched). Each check adds 1; expects $sum == 6.
(module
  (func $_start (result i32)
    (local $sum i32)

    ;; drop i32: 7 beneath, 99 dropped -> 7 remains
    i32.const 7 i32.const 99 drop i32.const 7 i32.eq
    local.get $sum i32.add local.set $sum

    ;; drop i64
    i64.const 123456789012 i64.const -1 drop i64.const 123456789012 i64.eq
    local.get $sum i32.add local.set $sum

    ;; drop f32 (and the surviving value keeps its exact bits incl -0.0 sign)
    f32.const 1.0 f32.const -0.0 f32.const 42.0 drop f32.div
    f32.const -inf f32.eq local.get $sum i32.add local.set $sum

    ;; drop f64
    f64.const 3.5 f64.const 9.0 drop f64.const 3.5 f64.eq
    local.get $sum i32.add local.set $sum

    ;; two consecutive drops leave the deepest value
    i32.const 55 i32.const 1 i32.const 2 drop drop i32.const 55 i32.eq
    local.get $sum i32.add local.set $sum

    ;; drop of an fp keeps a following i32 intact (mixed types)
    i32.const 88 f64.const 1.25 drop i32.const 88 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 6 i32.ne)
  (export "_start" (func $_start)))
