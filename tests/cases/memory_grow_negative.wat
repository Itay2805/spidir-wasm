;; memory.grow with a NEGATIVE page count (high bit set) and with a
;; positive-but-enormous count. The argument is an i32; the host rejects
;; new_page_count<0 with -1 and rejects over-max with -1, leaving the
;; size unchanged in every case.
(module
  (memory 1 4)
  (func $_start (result i32)
    (local $sum i32)

    ;; 0x80000000 as i32 is negative -> grow returns -1, size unchanged
    i32.const 0x80000000 memory.grow i32.const -1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    ;; 0xFFFFFFFF (= -1) page count -> -1, size unchanged
    i32.const 0xFFFFFFFF memory.grow i32.const -1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    ;; huge positive request way over max -> -1, size unchanged
    i32.const 0x40000000 memory.grow i32.const -1 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 6 i32.ne)
  (export "_start" (func $_start)))
