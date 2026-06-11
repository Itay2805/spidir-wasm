;; `br 1` targeting the FUNCTION-BODY label (the outermost label), carrying
;; the function result (77) through the label's arity. This is a legal early
;; return. wat2wasm/wasm-validate accept; wasm-interp returns $f(9)=77,
;; $f(0)=5, so _start=0. The JIT cannot thread the result: the function-body
;; label's spidir block is never assigned (function.c:146 leaves it {}), and
;; the result operand is left on the stack -> CHECK fails in jit_wasm_end.
;; (The void analogue, br_if to the function label of a void function, makes
;; the JIT HANG on malformed IR.) EXPECTED exit 0; OBSERVED exit 1.
(module
  (func $f (param $x i32) (result i32)
    (local $r i32)
    block
      local.get $x
      i32.const 0
      i32.eq
      br_if 0      ;; x==0 -> exit this inner block normally
      i32.const 77
      local.set $r
      local.get $r ;; function result on the stack (function arity 1)
      br 1         ;; br to the function-body label -> early return of 77
    end
    i32.const 5)   ;; x==0 path
  (func $_start (result i32)
    block i32.const 0 call $f i32.const 5  i32.eq br_if 0 unreachable end
    block i32.const 9 call $f i32.const 77 i32.eq br_if 0 unreachable end
    i32.const 0)
  (export "_start" (func $_start)))
