;; Exercises global.get and global.set on a mutable i32 global.
;; The test always writes the global before reading it, so it doesn't depend
;; on the host zero-initializing the globals region.
;; Returns 0 on success.
(module
  (global $counter (mut i32) (i32.const 0))

  (func $bump (param $by i32)
    global.get $counter
    local.get $by
    i32.add
    global.set $counter)

  (func $_start (result i32)
    ;; reset to a known value
    i32.const 0
    global.set $counter
    ;; counter = 0 + 10 + 15 + 17 = 42
    i32.const 10 call $bump
    i32.const 15 call $bump
    i32.const 17 call $bump
    global.get $counter
    i32.const 42
    i32.ne)

  (export "_start" (func $_start)))
