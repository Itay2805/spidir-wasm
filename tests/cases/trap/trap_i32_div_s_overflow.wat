;; TRAP test: i32.div_s of INT_MIN / -1 overflows (result +2^31 is not
;; representable as i32) and MUST trap. Contrast with rem_s INT_MIN%-1 which
;; must NOT trap (=0). EXPECTED non-zero exit.
(module
  (func $_start (result i32)
    i32.const -2147483648 i32.const -1 i32.div_s)
  (export "_start" (func $_start)))
