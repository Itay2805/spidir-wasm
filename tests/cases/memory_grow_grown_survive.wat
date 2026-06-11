;; Data written into a page added by an EARLIER grow must survive a
;; LATER grow (the later grow only mmaps the newly-added range, so
;; previously-grown pages must be left untouched). The existing test
;; only checks the ORIGINAL min pages survive.
(module
  (memory 1 8)
  (func $_start (result i32)
    (local $sum i32)

    ;; grow to 3 (adds pages 1,2). returns old size 1.
    i32.const 2 memory.grow i32.const 1 i32.eq
    local.get $sum i32.add local.set $sum

    ;; write sentinels into the freshly added pages 1 and 2
    i32.const 65536  i32.const 0x11112222 i32.store   ;; page 1 start
    i32.const 196607 i32.const 0x33      i32.store8   ;; last byte of page 2

    ;; grow again to 6 (adds pages 3,4,5). returns old size 3.
    i32.const 3 memory.grow i32.const 3 i32.eq
    local.get $sum i32.add local.set $sum
    memory.size i32.const 6 i32.eq
    local.get $sum i32.add local.set $sum

    ;; the page-1/page-2 sentinels written before the 2nd grow are intact
    i32.const 65536  i32.load   i32.const 0x11112222 i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 196607 i32.load8_u i32.const 0x33      i32.eq
    local.get $sum i32.add local.set $sum

    ;; the brand new pages (3,4,5) are zero and writable
    i32.const 327680 i32.load i32.const 0 i32.eq      ;; page 5 start, zeroed
    local.get $sum i32.add local.set $sum
    i32.const 327680 i32.const 0x44445555 i32.store
    i32.const 327680 i32.load i32.const 0x44445555 i32.eq
    local.get $sum i32.add local.set $sum

    local.get $sum i32.const 7 i32.ne)
  (export "_start" (func $_start)))
