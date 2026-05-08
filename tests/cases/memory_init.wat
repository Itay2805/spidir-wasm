;; Exercises the wasm bulk-memory ops `memory.init` and `data.drop`,
;; which the JIT lowers to a call into the C helper jit_helper_memory_init
;; and a store-NULL into the data slot of the JIT state, respectively.
;;
;; A passive data segment is declared with `(data "...")` (no memory
;; offset) — it is *not* copied at instantiation; the program must
;; explicitly invoke `memory.init` to copy bytes from it.
;;
;; Each check traps on first mismatch — exit-0 means every individual
;; case matched. Cross-validated against `wasm-interp`.
(module
  (memory 1)

  ;; Segment 0: passive 5-byte ASCII string. Bytes 'A'..'E'.
  (data $alpha "ABCDE")

  ;; Segment 1: passive 8-byte little-endian i64 = 0x0102030405060708.
  (data $word "\08\07\06\05\04\03\02\01")

  ;; Segment 2: passive empty segment. Used to verify length-0 init at
  ;; a boundary doesn't read from a NULL data pointer.
  (data $empty "")

  ;; Segment 3: active segment at offset 2048 — coexists with passives
  ;; and is auto-applied, like in data.wat. Verifies the JIT correctly
  ;; tags it as active (offset == -1) so it doesn't get a state slot.
  (data (i32.const 2048) "ZZZ")

  (func $_start (result i32)
    ;; --- active segment is already in place ----------------------------
    block i32.const 2048 i32.load8_u i32.const 0x5A i32.eq br_if 0 unreachable end ;; 'Z'

    ;; --- memory.init: full passive copy ---------------------------------
    ;; Copy all 5 bytes of $alpha to memory offset 100.
    i32.const 100   ;; dst
    i32.const 0     ;; src_offset
    i32.const 5     ;; n
    memory.init $alpha
    block i32.const 100 i32.load8_u i32.const 0x41 i32.eq br_if 0 unreachable end ;; 'A'
    block i32.const 101 i32.load8_u i32.const 0x42 i32.eq br_if 0 unreachable end ;; 'B'
    block i32.const 104 i32.load8_u i32.const 0x45 i32.eq br_if 0 unreachable end ;; 'E'
    ;; The byte just past the destination must remain zero (untouched).
    block i32.const 105 i32.load8_u i32.const 0    i32.eq br_if 0 unreachable end

    ;; --- memory.init: partial slice (src_offset != 0, n < seg_len) ------
    ;; Copy bytes [1..4) of $alpha ("BCD") to memory offset 200.
    i32.const 200   ;; dst
    i32.const 1     ;; src_offset
    i32.const 3     ;; n
    memory.init $alpha
    block i32.const 200 i32.load8_u i32.const 0x42 i32.eq br_if 0 unreachable end ;; 'B'
    block i32.const 201 i32.load8_u i32.const 0x43 i32.eq br_if 0 unreachable end ;; 'C'
    block i32.const 202 i32.load8_u i32.const 0x44 i32.eq br_if 0 unreachable end ;; 'D'
    ;; One past the slice — must still be zero.
    block i32.const 203 i32.load8_u i32.const 0    i32.eq br_if 0 unreachable end

    ;; --- memory.init: copy from a different passive segment -------------
    ;; Copy all 8 bytes of $word to memory offset 300, then read as i64.
    i32.const 300   ;; dst
    i32.const 0     ;; src_offset
    i32.const 8     ;; n
    memory.init $word
    block i32.const 300 i64.load i64.const 0x0102030405060708 i64.eq br_if 0 unreachable end

    ;; --- memory.init: n = 0 is a no-op even on the empty segment --------
    ;; The empty segment has a non-NULL but zero-length data pointer; with
    ;; n = 0 the helper must short-circuit before any read.
    i32.const 400   ;; dst
    i32.const 0     ;; src_offset
    i32.const 0     ;; n
    memory.init $empty
    block i32.const 400 i32.load8_u i32.const 0 i32.eq br_if 0 unreachable end

    ;; --- data.drop, then n = 0 init from the dropped segment ------------
    ;; data.drop nulls out the segment's data pointer in the JIT state.
    ;; A subsequent memory.init with n = 0 must still succeed (the helper
    ;; only dereferences `data` when length != 0).
    data.drop $alpha
    i32.const 500   ;; dst
    i32.const 0     ;; src_offset
    i32.const 0     ;; n
    memory.init $alpha
    block i32.const 500 i32.load8_u i32.const 0 i32.eq br_if 0 unreachable end

    ;; data.drop on a segment that is already dropped is a no-op (idempotent).
    data.drop $alpha
    block i32.const 500 i32.load8_u i32.const 0 i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
