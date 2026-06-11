;; Memory with NO declared maximum (limit_type 0x00) -- a parse path the
;; existing (memory 1 4) test never exercises. grow must still work,
;; returning the previous size, and a request past the implicit cap must
;; fail with -1 without changing the size.
(module
  (memory 1)   ;; min 1, no max
  (func $_start (result i32)
    (local $sum i32)

    ;; size starts at 1
    memory.size i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    ;; grow by 0 returns current size 1, size unchanged
    i32.const 0 memory.grow i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    ;; grow by 2 returns old size 1, size becomes 3
    i32.const 2 memory.grow i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 3 i32.eq
    local.get $sum i32.add local.set $sum

    ;; new page is accessible and zero
    i32.const 65536 i32.load i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; a request larger than the implicit cap must fail with -1; size unchanged
    i32.const 0x7FFFFFFF memory.grow i32.const -1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 3 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
