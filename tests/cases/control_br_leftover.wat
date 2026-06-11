;; `br 0` to an arity-0 block while an operand (7) is left on the stack.
;; Spec: br pops n=0 result values and discards the rest down to the label
;; height. wat2wasm/wasm-validate accept; wasm-interp returns $f=5.
;; The JIT leaves the 7 on label->stack and trips CHECK(stack.length==0) in
;; jit_wasm_end (inst.c:1850). EXPECTED exit 0; OBSERVED exit 1.
(module
  (func $f (result i32)
    (local $r i32)
    i32.const 5
    local.set $r
    block
      i32.const 7    ;; leftover operand on the stack at the br
      br 0           ;; must discard the 7 and branch to block end
    end
    local.get $r)
  (func $_start (result i32)
    call $f
    i32.const 5
    i32.ne)
  (export "_start" (func $_start)))
