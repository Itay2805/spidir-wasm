(module
  (memory 1)
  (data $d "ABCDE")
  (func $_start (result i32)
    ;; SPEC: memory.init with NONZERO length from a dropped segment MUST TRAP.
    ;; The JIT correctly traps here (ASSERT(data != nullptr) -> __builtin_trap).
    ;; This is a positive trap-path test (binary aborts; exit 132 = SIGILL).
    data.drop $d
    i32.const 0 i32.const 0 i32.const 3 memory.init $d
    i32.const 0)
  (export "_start" (func $_start)))
