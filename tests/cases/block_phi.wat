;; Exercises jit_wasm_prepare_branch's phi-creation path. The previous
;; block_br test only had each block-end reached from a single incoming
;; edge, which short-circuits prepare_branch (it just copies the locals
;; on the first incoming branch). Here we deliberately produce two
;; incoming edges into the same block-end with locals that differ along
;; each path, forcing the JIT to materialize a phi.
;; Returns 0 on success.
(module
  ;; Single-local phi: $x is set to a different constant on each path.
  (func $sel (param $cond i32) (result i32)
    (local $x i32)
    block                        ;; outer (phi target)
      block                      ;; inner
        local.get $cond
        i32.eqz
        br_if 0                  ;; cond == 0 -> exit inner, take "x=2" path
        ;; cond != 0 path
        i32.const 1
        local.set $x
        br 1                     ;; jump to outer-end with $x = 1
      end
      ;; cond == 0 path falls through here
      i32.const 2
      local.set $x
      ;; fall through to outer-end with $x = 2
    end
    local.get $x)

  ;; Multi-local phi: forces the per-local phi loop in prepare_branch to
  ;; iterate, exercising phi creation for several locals at once.
  (func $sel_multi (param $cond i32) (result i32)
    (local $a i32) (local $b i32) (local $c i32) (local $d i32)
    block
      block
        local.get $cond
        i32.eqz
        br_if 0
        ;; cond != 0 path: a=10, b=20, c=30, d=40
        i32.const 10  local.set $a
        i32.const 20  local.set $b
        i32.const 30  local.set $c
        i32.const 40  local.set $d
        br 1
      end
      ;; cond == 0 path: a=100, b=200, c=300, d=400
      i32.const 100  local.set $a
      i32.const 200  local.set $b
      i32.const 300  local.set $c
      i32.const 400  local.set $d
    end
    local.get $a
    local.get $b
    i32.add
    local.get $c
    i32.add
    local.get $d
    i32.add)

  (func $_start (result i32)
    (local $sum i32)

    ;; sel(1) = 1; sel(0) = 2
    i32.const 1 call $sel  i32.const 1 i32.eq
    local.set $sum
    i32.const 0 call $sel  i32.const 2 i32.eq
    local.get $sum  i32.add
    local.set $sum

    ;; sel_multi(1) = 10+20+30+40 = 100
    i32.const 1 call $sel_multi  i32.const 100 i32.eq
    local.get $sum  i32.add
    local.set $sum

    ;; sel_multi(0) = 100+200+300+400 = 1000
    i32.const 0 call $sel_multi  i32.const 1000 i32.eq
    local.get $sum  i32.add
    local.set $sum

    ;; expect 4 successful comparisons -> sum == 4
    local.get $sum
    i32.const 4
    i32.ne)

  (export "_start" (func $_start)))
