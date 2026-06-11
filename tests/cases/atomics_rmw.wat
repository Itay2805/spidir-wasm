;; Direct single-threaded coverage for atomic RMW binops and cmpxchg.
;; Each RMW returns the OLD value and updates memory to (old op arg);
;; cmpxchg stores the replacement and returns old ONLY when old==expected.
;; Single-threaded, so every result is deterministic.
;; Returns 0 iff all checks pass (accumulated into $sum, compared to count).
;; Assemble with: wat2wasm --enable-threads --debug-names
(module
  (memory 1 1)
  (func $sti64 (param $a i32) (param $v i64) local.get $a local.get $v i64.store)
  (func $_start (result i32)
    (local $sum i32)

    ;; ---- i32 full-width RMW: returns old, mem = old op arg ----
    i32.const 0 i32.const 100 i32.store
    (i32.eq (i32.atomic.rmw.add (i32.const 0) (i32.const 7)) (i32.const 100))      local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 0)) (i32.const 107))                              local.get $sum i32.add local.set $sum

    i32.const 4 i32.const 50 i32.store
    (i32.eq (i32.atomic.rmw.sub (i32.const 4) (i32.const 8)) (i32.const 50))       local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 4)) (i32.const 42))                               local.get $sum i32.add local.set $sum

    i32.const 8 i32.const 0xF0 i32.store
    (i32.eq (i32.atomic.rmw.and (i32.const 8) (i32.const 0x3C)) (i32.const 0xF0))  local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 8)) (i32.const 0x30))                             local.get $sum i32.add local.set $sum

    i32.const 12 i32.const 0x0F i32.store
    (i32.eq (i32.atomic.rmw.or (i32.const 12) (i32.const 0x30)) (i32.const 0x0F))  local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 12)) (i32.const 0x3F))                            local.get $sum i32.add local.set $sum

    i32.const 16 i32.const 0xFF i32.store
    (i32.eq (i32.atomic.rmw.xor (i32.const 16) (i32.const 0x0F)) (i32.const 0xFF)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 16)) (i32.const 0xF0))                            local.get $sum i32.add local.set $sum

    i32.const 20 i32.const 1234 i32.store
    (i32.eq (i32.atomic.rmw.xchg (i32.const 20) (i32.const 5678)) (i32.const 1234))local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 20)) (i32.const 5678))                            local.get $sum i32.add local.set $sum

    ;; ---- sub-width i32 (8/16): truncation + zero-extended old ----
    ;; 0xF0 + 0x20 = 0x110 -> low byte 0x10; old = 0xF0
    i32.const 24 i32.const 0xF0 i32.store8
    (i32.eq (i32.atomic.rmw8.add_u (i32.const 24) (i32.const 0x20)) (i32.const 0xF0)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 24)) (i32.const 0x10))                            local.get $sum i32.add local.set $sum
    ;; only low 8 bits of operand used
    i32.const 25 i32.const 0x01 i32.store8
    (i32.eq (i32.atomic.rmw8.add_u (i32.const 25) (i32.const 0xFFFFFF02)) (i32.const 0x01)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 25)) (i32.const 0x03))                                  local.get $sum i32.add local.set $sum
    ;; 16-bit wrap
    i32.const 26 i32.const 0xFFFF i32.store16
    (i32.eq (i32.atomic.rmw16.add_u (i32.const 26) (i32.const 2)) (i32.const 0xFFFF)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load16_u (i32.const 26)) (i32.const 0x0001))                         local.get $sum i32.add local.set $sum
    ;; 8-bit sub wrap
    i32.const 28 i32.const 0x05 i32.store8
    (i32.eq (i32.atomic.rmw8.sub_u (i32.const 28) (i32.const 0x08)) (i32.const 0x05)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 28)) (i32.const 0xFD))                            local.get $sum i32.add local.set $sum
    ;; 8-bit xchg drops high bits on store
    i32.const 29 i32.const 0xAA i32.store8
    (i32.eq (i32.atomic.rmw8.xchg_u (i32.const 29) (i32.const 0x1BB)) (i32.const 0xAA)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 29)) (i32.const 0xBB))                              local.get $sum i32.add local.set $sum

    ;; ---- i32 cmpxchg: success stores, failure leaves memory untouched ----
    i32.const 32 i32.const 111 i32.store
    (i32.eq (i32.atomic.rmw.cmpxchg (i32.const 32) (i32.const 111) (i32.const 222)) (i32.const 111)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 32)) (i32.const 222))                                               local.get $sum i32.add local.set $sum
    i32.const 36 i32.const 333 i32.store
    (i32.eq (i32.atomic.rmw.cmpxchg (i32.const 36) (i32.const 999) (i32.const 444)) (i32.const 333)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load (i32.const 36)) (i32.const 333))                                               local.get $sum i32.add local.set $sum
    ;; sub-width cmpxchg compares only low N bits of expected
    i32.const 40 i32.const 0x7E i32.store8
    (i32.eq (i32.atomic.rmw8.cmpxchg_u (i32.const 40) (i32.const 0x1234567E) (i32.const 0xAB99)) (i32.const 0x7E)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 40)) (i32.const 0x99))                                                        local.get $sum i32.add local.set $sum
    i32.const 41 i32.const 0x10 i32.store8
    (i32.eq (i32.atomic.rmw8.cmpxchg_u (i32.const 41) (i32.const 0x11) (i32.const 0xFF)) (i32.const 0x10)) local.get $sum i32.add local.set $sum
    (i32.eq (i32.load8_u (i32.const 41)) (i32.const 0x10))                                                 local.get $sum i32.add local.set $sum

    ;; ---- full-width i64 RMW + cmpxchg ----
    i32.const 48 i64.const 0x0123456789ABCDEF call $sti64
    (i64.eq (i64.atomic.rmw.add (i32.const 48) (i64.const 0x1000000000000001)) (i64.const 0x0123456789ABCDEF)) local.get $sum i32.add local.set $sum
    (i64.eq (i64.load (i32.const 48)) (i64.const 0x1123456789ABCDF0))                                          local.get $sum i32.add local.set $sum
    i32.const 56 i64.const 0xFFFFFFFFFFFFFFFF call $sti64
    (i64.eq (i64.atomic.rmw.and (i32.const 56) (i64.const 0x00000000FFFFFFFF)) (i64.const 0xFFFFFFFFFFFFFFFF)) local.get $sum i32.add local.set $sum
    (i64.eq (i64.load (i32.const 56)) (i64.const 0x00000000FFFFFFFF))                                          local.get $sum i32.add local.set $sum
    i32.const 64 i64.const 42 call $sti64
    (i64.eq (i64.atomic.rmw.xchg (i32.const 64) (i64.const 0xDEADBEEFCAFEBABE)) (i64.const 42))               local.get $sum i32.add local.set $sum
    (i64.eq (i64.load (i32.const 64)) (i64.const 0xDEADBEEFCAFEBABE))                                          local.get $sum i32.add local.set $sum
    i32.const 72 i64.const 1000 call $sti64
    (i64.eq (i64.atomic.rmw.cmpxchg (i32.const 72) (i64.const 1000) (i64.const 2000)) (i64.const 1000))       local.get $sum i32.add local.set $sum
    (i64.eq (i64.load (i32.const 72)) (i64.const 2000))                                                       local.get $sum i32.add local.set $sum
    (i64.eq (i64.atomic.rmw.cmpxchg (i32.const 72) (i64.const 7) (i64.const 9999)) (i64.const 2000))          local.get $sum i32.add local.set $sum
    (i64.eq (i64.load (i32.const 72)) (i64.const 2000))                                                       local.get $sum i32.add local.set $sum

    ;; 40 checks total
    local.get $sum i32.const 40 i32.ne)
  (export "_start" (func $_start)))
