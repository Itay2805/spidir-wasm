(module
  (memory 1)
  (data $seg "ABCDEFGH")     ;; 8 bytes
  (data $empty "")
  (func $_start (result i32)
    (local $sum i32)

    ;; 1. memory.init exact-boundary slice: src_offset=5, n=3 -> 5+3=8 == data_len. OK, no trap.
    i32.const 10 i32.const 5 i32.const 3 memory.init $seg
    i32.const 10 i32.load8_u i32.const 0x46 i32.eq  ;; 'F'
    local.get $sum i32.add local.set $sum
    i32.const 12 i32.load8_u i32.const 0x48 i32.eq  ;; 'H'
    local.get $sum i32.add local.set $sum

    ;; 2. memory.init n=0 with src_offset == data_len (boundary): 8+0=8 == data_len, OK.
    i32.const 20 i32.const 8 i32.const 0 memory.init $seg
    i32.const 20 i32.load8_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; 3. memory.fill value truncation high bit: 0x1FF & 0xFF = 0xFF
    i32.const 30 i32.const 0x1FF i32.const 2 memory.fill
    i32.const 30 i32.load8_u i32.const 0xFF i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 31 i32.load8_u i32.const 0xFF i32.eq
    local.get $sum i32.add local.set $sum

    ;; 4. memory.fill value exactly 0x100 -> low byte 0x00
    i32.const 40 i32.const 0x100 i32.const 1 memory.fill
    i32.const 40 i32.load8_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; 5. memory.copy zero-length at offset == size (boundary, both src & dst): no trap.
    i32.const 65536 i32.const 65536 i32.const 0 memory.copy
    i32.const 1   ;; reaching here without trapping is the pass
    local.get $sum i32.add local.set $sum

    ;; 6. memory.copy fully-overlapping (src == dst): identity, n>0.
    i32.const 50 i32.const 0xAA i32.store8
    i32.const 51 i32.const 0xBB i32.store8
    i32.const 50 i32.const 50 i32.const 2 memory.copy
    i32.const 50 i32.load8_u i32.const 0xAA i32.eq
    local.get $sum i32.add local.set $sum
    i32.const 51 i32.load8_u i32.const 0xBB i32.eq
    local.get $sum i32.add local.set $sum

    ;; 7. data.drop then memory.init n=0 from dropped segment (data ptr null, n=0 ok)
    data.drop $seg
    i32.const 60 i32.const 0 i32.const 0 memory.init $seg
    i32.const 60 i32.load8_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; 8. memory.init n=0 from the empty passive segment (non-null but zero-len ptr)
    i32.const 70 i32.const 0 i32.const 0 memory.init $empty
    i32.const 70 i32.load8_u i32.const 0 i32.eq
    local.get $sum i32.add local.set $sum

    ;; 11 checks total
    local.get $sum i32.const 11 i32.ne)
  (export "_start" (func $_start)))
