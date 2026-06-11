;; Memory that starts EMPTY (min 0). size must report 0, and the very
;; first grow must return the previous size 0 (the old_count==0 path),
;; not 1 and not -1.
(module
  (memory 0 4)
  (func $_start (result i32)
    (local $sum i32)

    ;; size starts at 0
    memory.size i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; grow(0) on empty memory returns 0, leaves size 0
    i32.const 0 memory.grow i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; first real grow: from 0 to 2, returns old size 0
    i32.const 2 memory.grow i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 2 i32.eq
    local.get $sum i32.add local.set $sum

    ;; freshly created pages are accessible and zeroed (low and high byte)
    i32.const 0 i32.load i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 131071 i32.load8_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; store/readback into the new page
    i32.const 0 i32.const 0xABCD1234 i32.store
    i32.const 0 i32.load i32.const 0xABCD1234 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 8 i32.ne)
  (export "_start" (func $_start)))
