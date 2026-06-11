;; Spec-valid uses of br/return/unreachable that leave dead operands on the
;; operand stack below the branch result arity. WebAssembly makes these
;; instructions stack-polymorphic: the leftover operands must be discarded.
;; The JIT instead asserts label->stack.length==0 (inst.c:392 / inst.c:1850)
;; and REJECTS the module. wat2wasm + wasm-validate accept it; wasm-interp
;; runs $f to 42. EXPECTED exit 0; OBSERVED exit 1 -> suspected bug.
(module
  (func $f (result i32)
    i32.const 7      ;; leftover operand below the return value (must be discarded)
    i32.const 42
    return)
  (func $_start (result i32)
    call $f
    i32.const 42
    i32.ne)
  (export "_start" (func $_start)))
