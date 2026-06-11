;; int_unary edge-case gap-fill: i32/i64 clz, ctz, popcnt.
;; All inputs are DYNAMIC (param-derived / shift-built) so the JIT must emit the
;; real lzcount/tzcount/popcount intrinsics instead of constant-folding.
;; Covers the spec's "i = 0" totality cases (iclz/ictz return N, ipopcnt returns 0),
;; the all-ones cases (return N), single-bit endpoints, and i64 cross-word
;; boundaries (guards against accidental 32-bit truncation in the i64 lowering).
;; Returns 0 iff all 24 checks pass.
(module
  (func $clz32    (param i32)(result i32) local.get 0 i32.clz)
  (func $ctz32    (param i32)(result i32) local.get 0 i32.ctz)
  (func $popcnt32 (param i32)(result i32) local.get 0 i32.popcnt)
  (func $clz64    (param i64)(result i64) local.get 0 i64.clz)
  (func $ctz64    (param i64)(result i64) local.get 0 i64.ctz)
  (func $popcnt64 (param i64)(result i64) local.get 0 i64.popcnt)

  (func $_start (result i32)
    (local $sum i32)
    (local $z i32)     ;; opaque dynamic 0
    (local $z64 i64)
    i32.const 0xDEAD i32.const 0xDEAD i32.xor local.set $z
    i64.const 0xDEAD i64.const 0xDEAD i64.xor local.set $z64

    ;; ---- i32.clz ----
    local.get $z                       call $clz32    i32.const 32 i32.eq local.get $sum i32.add local.set $sum
    i32.const 1 i32.const 31 i32.shl   call $clz32    i32.const 0  i32.eq local.get $sum i32.add local.set $sum
    local.get $z i32.const 1 i32.or    call $clz32    i32.const 31 i32.eq local.get $sum i32.add local.set $sum
    i32.const 0x40000000               call $clz32    i32.const 1  i32.eq local.get $sum i32.add local.set $sum
    i32.const 0x7FFFFFFF               call $clz32    i32.const 1  i32.eq local.get $sum i32.add local.set $sum
    ;; ---- i32.ctz ----
    local.get $z                       call $ctz32    i32.const 32 i32.eq local.get $sum i32.add local.set $sum
    i32.const 1 i32.const 31 i32.shl   call $ctz32    i32.const 31 i32.eq local.get $sum i32.add local.set $sum
    i32.const 0x7FFFFFFF               call $ctz32    i32.const 0  i32.eq local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFE               call $ctz32    i32.const 1  i32.eq local.get $sum i32.add local.set $sum
    ;; ---- i32.popcnt ----
    local.get $z                       call $popcnt32 i32.const 0  i32.eq local.get $sum i32.add local.set $sum
    local.get $z i32.const -1 i32.xor  call $popcnt32 i32.const 32 i32.eq local.get $sum i32.add local.set $sum
    i32.const 1 i32.const 31 i32.shl   call $popcnt32 i32.const 1  i32.eq local.get $sum i32.add local.set $sum

    ;; ---- i64.clz ----
    local.get $z64                     call $clz64    i64.const 64 i64.eq local.get $sum i32.add local.set $sum
    i64.const 1 i64.const 63 i64.shl   call $clz64    i64.const 0  i64.eq local.get $sum i32.add local.set $sum
    local.get $z64 i64.const 1 i64.or  call $clz64    i64.const 63 i64.eq local.get $sum i32.add local.set $sum
    i64.const 0x0000000080000000       call $clz64    i64.const 32 i64.eq local.get $sum i32.add local.set $sum
    i64.const 0x00000000FFFFFFFF       call $clz64    i64.const 32 i64.eq local.get $sum i32.add local.set $sum
    ;; ---- i64.ctz ----
    local.get $z64                     call $ctz64    i64.const 64 i64.eq local.get $sum i32.add local.set $sum
    i64.const 1 i64.const 63 i64.shl   call $ctz64    i64.const 63 i64.eq local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFF00000000       call $ctz64    i64.const 32 i64.eq local.get $sum i32.add local.set $sum
    ;; ---- i64.popcnt ----
    local.get $z64                     call $popcnt64 i64.const 0  i64.eq local.get $sum i32.add local.set $sum
    local.get $z64 i64.const -1 i64.xor call $popcnt64 i64.const 64 i64.eq local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFF00000000       call $popcnt64 i64.const 32 i64.eq local.get $sum i32.add local.set $sum
    i64.const 0x8000000000000001       call $popcnt64 i64.const 2  i64.eq local.get $sum i32.add local.set $sum

    local.get $sum i32.const 24 i32.ne)
  (export "_start" (func $_start)))
