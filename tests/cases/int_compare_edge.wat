;; Edge-case audit for the integer comparison family (jit_wasm_cmpi, 0x45-0x5A).
;; Covers signed/unsigned divergence at the sign-bit boundary, eqz on nonzero,
;; equal operands for le/ge (and false strict relations), INT_MIN/INT_MAX/0/-1
;; boundaries, and full-width i64 (low-halves-equal) compares. Returns 0 iff
;; every check produced its spec-mandated result.
(module
  (func $_start (result i32)
    (local $sum i32)   ;; counts checks that matched the spec

    ;; ================= i32 =================

    ;; --- eqz: 0 -> 1, nonzero -> 0 ---
    i32.const 0 i32.eqz                                 ;; 1
    local.get $sum i32.add local.set $sum
    i32.const 1 i32.eqz i32.eqz                         ;; eqz(1)=0 -> 1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.eqz i32.eqz                ;; eqz(-1)=0 -> 1
    local.get $sum i32.add local.set $sum
    i32.const 0x80000000 i32.eqz i32.eqz                ;; eqz(INT_MIN)=0 -> 1
    local.get $sum i32.add local.set $sum

    ;; --- signed vs unsigned divergence: -1 (0xFFFFFFFF) vs 0 ---
    i32.const 0xFFFFFFFF i32.const 0 i32.lt_s           ;; -1 < 0 signed -> 1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0 i32.lt_u i32.eqz   ;; 0xFFFFFFFF<0 unsigned=0 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0 i32.gt_s i32.eqz   ;; -1>0 signed=0 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0 i32.gt_u           ;; 0xFFFFFFFF>0 unsigned ->1
    local.get $sum i32.add local.set $sum

    ;; --- INT_MIN vs INT_MAX signed/unsigned ---
    i32.const 0x80000000 i32.const 0x7FFFFFFF i32.lt_s  ;; signed: MIN<MAX ->1
    local.get $sum i32.add local.set $sum
    i32.const 0x80000000 i32.const 0x7FFFFFFF i32.gt_u  ;; unsigned: 2^31>2^31-1 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0x80000000 i32.const 0x7FFFFFFF i32.lt_u i32.eqz ;; lt_u=0 ->1
    local.get $sum i32.add local.set $sum

    ;; --- equal operands for le/ge (signed and unsigned) ---
    i32.const 0x80000000 i32.const 0x80000000 i32.le_s  ;; 1
    local.get $sum i32.add local.set $sum
    i32.const 0x80000000 i32.const 0x80000000 i32.ge_s  ;; 1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0xFFFFFFFF i32.le_u  ;; 1 (le_u missing from existing test)
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0xFFFFFFFF i32.ge_u  ;; 1
    local.get $sum i32.add local.set $sum
    i32.const 5 i32.const 5 i32.lt_s i32.eqz            ;; lt_s=0 ->1
    local.get $sum i32.add local.set $sum
    i32.const 5 i32.const 5 i32.gt_u i32.eqz            ;; gt_u=0 ->1
    local.get $sum i32.add local.set $sum

    ;; --- le_u / ge_u directional (le_u missing from existing i32 test) ---
    i32.const 1 i32.const 0xFFFFFFFF i32.le_u           ;; 1<=0xFFFFFFFF ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 1 i32.le_u i32.eqz   ;; 0xFFFFFFFF<=1=0 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 1 i32.ge_u           ;; 0xFFFFFFFF>=1 ->1
    local.get $sum i32.add local.set $sum

    ;; --- gt_s / ge_s directional with negatives ---
    i32.const 1 i32.const 0xFFFFFFFF i32.gt_s           ;; 1 > -1 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFF i32.const 0xFFFFFFFE i32.ge_s  ;; -1 >= -2 ->1
    local.get $sum i32.add local.set $sum
    i32.const 0xFFFFFFFE i32.const 0xFFFFFFFF i32.ge_s i32.eqz ;; -2>=-1=0 ->1
    local.get $sum i32.add local.set $sum

    ;; --- eq/ne at boundary ---
    i32.const 0x80000000 i32.const 0x80000000 i32.eq    ;; 1
    local.get $sum i32.add local.set $sum
    i32.const 0x7FFFFFFF i32.const 0x80000000 i32.ne    ;; 1
    local.get $sum i32.add local.set $sum

    ;; ================= i64 (full-width) =================

    ;; eqz nonzero high-bit-only value (i64.eqz -> i32, invert with i32.eqz)
    i64.const 0x8000000000000000 i64.eqz i32.eqz        ;; eqz=0 ->1
    local.get $sum i32.add local.set $sum
    i64.const 0 i64.eqz                                 ;; 1
    local.get $sum i32.add local.set $sum

    ;; signed vs unsigned divergence: -1 vs 0 full width
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0 i64.lt_s   ;; -1<0 ->1
    local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0 i64.lt_u i32.eqz ;; lt_u=0 ->1
    local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0 i64.gt_u   ;; gt_u ->1
    local.get $sum i32.add local.set $sum

    ;; INT64_MIN vs INT64_MAX signed/unsigned
    i64.const 0x8000000000000000 i64.const 0x7FFFFFFFFFFFFFFF i64.lt_s ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0x8000000000000000 i64.const 0x7FFFFFFFFFFFFFFF i64.gt_u ;; 1
    local.get $sum i32.add local.set $sum

    ;; equal operands le/ge full width
    i64.const 0x8000000000000000 i64.const 0x8000000000000000 i64.le_s ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0x8000000000000000 i64.const 0x8000000000000000 i64.ge_s ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0xFFFFFFFFFFFFFFFF i64.le_u ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0xFFFFFFFFFFFFFFFF i64.const 0xFFFFFFFFFFFFFFFF i64.ge_u ;; 1
    local.get $sum i32.add local.set $sum

    ;; low halves equal, high halves differ -> exercises full 64-bit width
    i64.const 0x0000000100000000 i64.const 0x00000000FFFFFFFF i64.gt_s ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0x0000000100000000 i64.const 0x00000000FFFFFFFF i64.gt_u ;; 1
    local.get $sum i32.add local.set $sum
    i64.const 0x0000000100000000 i64.const 0x00000000FFFFFFFF i64.ne   ;; 1
    local.get $sum i32.add local.set $sum

    ;; total expected checks
    local.get $sum
    i32.const 39
    i32.ne)

  (export "_start" (func $_start)))
