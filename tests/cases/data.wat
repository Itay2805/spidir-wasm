;; Exercises wasm active data segments. The host applies each segment
;; into memory after mmap and before invoking `_start`, so when we
;; start running the bytes are already there. Each check traps on
;; first mismatch — exit-0 means every individual case matched.
(module
  (memory 1)

  ;; Segment 0: classic "hello" string at offset 0.
  ;; Bytes 'H'=0x48 'e'=0x65 'l'=0x6C 'l'=0x6C 'o'=0x6F.
  (data (i32.const 0) "Hello")

  ;; Segment 1: a 4-byte little-endian word at offset 16. wat2wasm encodes
  ;; "\78\56\34\12" verbatim; loading as i32 reads 0x12345678.
  (data (i32.const 16) "\78\56\34\12")

  ;; Segment 2: an 8-byte little-endian i64 at offset 32 = 0x0102030405060708.
  (data (i32.const 32) "\08\07\06\05\04\03\02\01")

  ;; Segment 3: empty segment (zero-length). Spec allows this and the
  ;; host should accept it without error or memory writes.
  (data (i32.const 4096) "")

  ;; Segment 4: starts at a non-zero offset that doesn't overlap the
  ;; ones above. Verifies multiple segments coexist.
  (data (i32.const 100) "ABCDE")

  (func $_start (result i32)
    ;; --- "Hello" at offset 0 ---
    block i32.const 0  i32.load8_u  i32.const 0x48 i32.eq br_if 0 unreachable end ;; 'H'
    block i32.const 1  i32.load8_u  i32.const 0x65 i32.eq br_if 0 unreachable end ;; 'e'
    block i32.const 4  i32.load8_u  i32.const 0x6F i32.eq br_if 0 unreachable end ;; 'o'
    ;; The byte just past the segment must still be zero (untouched).
    block i32.const 5  i32.load8_u  i32.const 0    i32.eq br_if 0 unreachable end

    ;; --- 4-byte little-endian word at offset 16 ---
    block i32.const 16 i32.load     i32.const 0x12345678 i32.eq br_if 0 unreachable end

    ;; --- 8-byte i64 at offset 32 ---
    block i32.const 32 i64.load     i64.const 0x0102030405060708 i64.eq br_if 0 unreachable end

    ;; --- "ABCDE" at offset 100 ---
    block i32.const 100 i32.load8_u i32.const 0x41 i32.eq br_if 0 unreachable end ;; 'A'
    block i32.const 104 i32.load8_u i32.const 0x45 i32.eq br_if 0 unreachable end ;; 'E'

    ;; --- region between segments stays zero ---
    block i32.const 50  i32.load8_u i32.const 0    i32.eq br_if 0 unreachable end

    i32.const 0)

  (export "_start" (func $_start)))
