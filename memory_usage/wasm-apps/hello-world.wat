(module
  (type $t0 (func))
  (type $t1 (func (result i32)))
  (func $__wasm_call_ctors (type $t0))
  (func $_initialize (type $t0)
    call $__wasm_call_ctors)
  (func $dummy (type $t1) (result i32)
    (local $l0 i32)
    i32.const 0
    local.set $l0
    local.get $l0
    return)
  (table $T0 1 1 funcref)
  (memory $memory 2)
  (global $g0 (mut i32) (i32.const 66560))
  (export "memory" (memory 0))
  (export "_initialize" (func $_initialize))
  (export "dummy" (func $dummy)))
