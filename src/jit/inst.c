#include "inst.h"

#include "function.h"
#include "jit/helpers.h"
#include "opcodes.h"
#include "spidir/module.h"
#include "util/vec.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"
#include "wasm/host.h"

#define JIT_PUSH(_type, _value) \
    do { \
        jit_value_t value__ = { \
            .type = _type, \
            .value = _value, \
        }; \
        vec_push(&label->stack, value__); \
    } while (0)

#define JIT_POP(_type) \
    ({ \
        jit_value_t value__ = vec_pop(&label->stack); \
        spidir_value_type_t type__ = _type; \
        CHECK(value__.type == type__, "Unexpected type (%d != %d)", value__.type, type__); \
        value__.value; \
    })

void jit_free_label(jit_label_t* label) {
    wasm_host_free(label->locals_phis);
    wasm_host_free(label->locals_values);
    vec_free(&label->stack);
}

// Emit a runtime trap at the current insertion point. Lowering goes
// through JIT_HELPER_TRAP rather than spidir's `unreachable` so the
// optimizer doesn't get to delete the incoming branch — extern calls
// have unknown side effects, so spidir is forced to keep this path.
// We still terminate the block with `unreachable` afterwards because
// the helper is `noreturn` from our perspective.
static wasm_err_t jit_emit_trap(spidir_builder_handle_t builder, jit_context_t* ctx) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_funcref_t trap;
    RETHROW(jit_get_helper(ctx, JIT_HELPER_TRAP, &trap));
    spidir_builder_build_call(builder, trap, 0, nullptr);
    spidir_builder_build_unreachable(builder);
cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Parametric Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_unreachable(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tunreachable");
    RETHROW(jit_emit_trap(builder, ctx));
    label->terminated = true;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_nop(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tnop");

cleanup:
    return err;
}

static wasm_err_t jit_wasm_drop(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tdrop");
    CHECK(label->stack.length >= 1);
    vec_pop(&label->stack);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_select(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tselect");

    spidir_value_t c = JIT_POP(SPIDIR_TYPE_I32);
    jit_value_t val2 = vec_pop(&label->stack);
    jit_value_t val1 = vec_pop(&label->stack);
    CHECK(val1.type == val2.type);

    // prepare the next block
    spidir_block_t next_block = spidir_builder_create_block(builder);

    // we are going to use a brcond, if its zero it will take val1 and if its
    // non-zero it will take val2
    spidir_value_t values[] = { val1.value, val2.value };
    spidir_builder_build_brcond(builder, c, next_block, next_block);

    // setup the continuation
    spidir_builder_set_block(builder, next_block);
    spidir_value_t value = spidir_builder_build_phi(builder, val1.type, 2, values, NULL);

    // and push it
    JIT_PUSH(val1.type, value);

cleanup:
    return err;
}


static wasm_type_t* wasm_get_func_type(jit_context_t* ctx, uint32_t funcidx) {
    size_t imports_count = ctx->module->imports_count;
    typeidx_t typeidx;
    if (funcidx < imports_count) {
        typeidx = ctx->module->imports[funcidx].index;
    } else {
        typeidx = ctx->module->functions[funcidx - imports_count];
    }
    wasm_type_t* type = &ctx->module->types[typeidx];
    return type;
}

//----------------------------------------------------------------------------------------------------------------------
// Control Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_pull_block_type(buffer_t* code) {
    wasm_err_t err = WASM_NO_ERROR;

    // TODO: support for other block types
    uint8_t byte = BUFFER_PULL(uint8_t, code);
    CHECK(byte == 0x40);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_block(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    RETHROW(jit_wasm_pull_block_type(code));
    JIT_TRACE("wasm: \tblock");

    // append a new label
    jit_label_t* new_label = vec_add(&func->labels, 1);
    memset(new_label, 0, sizeof(*new_label));

    // this is the block after this block ends
    new_label->block = spidir_builder_create_block(builder);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_loop(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    RETHROW(jit_wasm_pull_block_type(code));
    JIT_TRACE("wasm: \tloop");

    // append a new label
    jit_label_t* new_label = vec_add(&func->labels, 1);
    memset(new_label, 0, sizeof(*new_label));

    // this is the loop block, we enter it right now
    new_label->block = spidir_builder_create_block(builder);
    spidir_builder_build_branch(builder, new_label->block);
    spidir_builder_set_block(builder, new_label->block);

    // this is a loop
    new_label->loop = true;

    // we need to prepare the locals of the label with phis from the get-go,
    // because a loop has backwards jumps so we can't know ahead of time
    // what will change

    new_label->locals_values = CALLOC(spidir_value_t, func->locals.length);
    CHECK(new_label->locals_values != nullptr);
    new_label->locals_phis = CALLOC(spidir_phi_t, func->locals.length);
    CHECK(new_label->locals_phis != nullptr);

    // we have an existing branch, check if we need any phis
    for (int i = 0; i < func->locals.length; i++) {
        // we mark as invalid to ensure that in the prepare_branch we will
        // always add to the phi, this is required because the values are
        // set in-stone at the entry point
        new_label->locals_values[i] = SPIDIR_VALUE_INVALID;

        // create the phi, update the local value directly, we ensure
        // to add it as input obviously
        func->locals.elements[i].value = spidir_builder_build_phi(builder,
            func->locals.elements[i].type,
            1, &func->locals.elements[i].value,
            &new_label->locals_phis[i]
        );
    }

cleanup:
    return err;
}

static wasm_err_t jit_wasm_prepare_branch(spidir_builder_handle_t builder, jit_function_ctx_t* func, jit_label_t* target) {
    wasm_err_t err = WASM_NO_ERROR;

    if (target->locals_values == nullptr) {
        // first attempt at going to the label, just copy
        // all the values as-is
        target->locals_values = CALLOC(spidir_value_t, func->locals.length);
        CHECK(target->locals_values != nullptr);
        target->locals_phis = CALLOC(spidir_phi_t, func->locals.length);
        CHECK(target->locals_phis != nullptr);

        for (int i = 0; i < func->locals.length; i++) {
            target->locals_values[i] = func->locals.elements[i].value;

            // TODO: this is not standard...
            target->locals_phis[i].id = UINT32_MAX;
        }

    } else {
        // switch into the block so we can create phis properly
        spidir_block_t current;
        CHECK(spidir_builder_cur_block(builder, &current));
        spidir_builder_set_block(builder, target->block);

        // we have an existing branch, check if we need any phis
        for (int i = 0; i < func->locals.length; i++) {
            // ignore if same value
            if (func->locals.elements[i].value.id == target->locals_values[i].id) {
                continue;
            }

            // create phi if doesn't exist
            if (target->locals_phis[i].id == UINT32_MAX) {
                // we need to create a new phi
                spidir_value_t value = spidir_builder_build_phi(builder,
                    func->locals.elements[i].type,
                    0, nullptr,
                    &target->locals_phis[i]
                );

                // fill with the last input as many times as needed
                for (int j = 0; j < target->inputs; j++) {
                    spidir_builder_add_phi_input(builder, target->locals_phis[i], target->locals_values[i]);
                }

                // this is the new value
                target->locals_values[i] = value;
            }

            // add as input
            spidir_builder_add_phi_input(builder, target->locals_phis[i], func->locals.elements[i].value);
        }

        // switch back
        spidir_builder_set_block(builder, current);
    }

    // increment the input count
    target->inputs++;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_br(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    // get the target
    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tbr %d", index);
    CHECK(index < func->labels.length);
    jit_label_t* target = &func->labels.elements[func->labels.length - index - 1];

    // prepare a branch to the target
    RETHROW(jit_wasm_prepare_branch(builder, func, target));

    // branch into the block
    spidir_builder_build_branch(builder, target->block);

    // block is now terminated
    label->terminated = true;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_br_if(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    // get the target
    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tbr.if %d", index);
    CHECK(index < func->labels.length);
    jit_label_t* target = &func->labels.elements[func->labels.length - index - 1];

    // prepare a branch to the target
    RETHROW(jit_wasm_prepare_branch(builder, func, target));

    // get the condition
    spidir_value_t c = JIT_POP(SPIDIR_TYPE_I32);

    // perform the branch
    spidir_block_t continuation = spidir_builder_create_block(builder);
    spidir_builder_build_brcond(builder, c, target->block, continuation);

    // and continue
    spidir_builder_set_block(builder, continuation);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_return(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \treturn");

    // handle return value if any
    spidir_value_t value = SPIDIR_VALUE_INVALID;
    if (func->ret_type != SPIDIR_TYPE_NONE) {
        value = JIT_POP(func->ret_type);
    }
    CHECK(label->stack.length == 0);
    spidir_builder_build_return(builder, value);

    // we terminated the label
    label->terminated = true;

cleanup:
    return err;
}

static wasm_err_t jit_wasm_call(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_t* params = nullptr;

    // prepare the function for jitting
    uint32_t funcidx = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tcall %d", funcidx);

    RETHROW(jit_prepare_function(ctx, funcidx));
    jit_function_t* callee = &ctx->functions[funcidx];

    // get the function signature
    wasm_type_t* type = wasm_get_func_type(ctx, funcidx);
    CHECK(type != nullptr);

    // first two params are the hidden mem/state bases, pass them
    // through unchanged from our own frame so the callee sees the same
    // module-instance state.
    size_t params_count = type->arg_types_count + 2;
    params = CALLOC(spidir_value_t, params_count);
    CHECK(params != nullptr);

    params[0] = spidir_builder_build_param_ref(builder, 0);
    params[1] = spidir_builder_build_param_ref(builder, 1);

    // pop the rest of the args from the stack
    for (int i = 0; i < type->arg_types_count; i++) {
        size_t arg_index = type->arg_types_count - i - 1;
        spidir_value_type_t stype = jit_get_spidir_value_type(type->arg_types[arg_index]);
        spidir_value_t value = JIT_POP(stype);
        params[arg_index + 2] = value;
    }

    // perform the call
    spidir_value_t ret_val = spidir_builder_build_call(builder, callee->spidir, params_count, params);

    // if we have a return push it into the stack
    if (type->result_types_count > 0) {
        CHECK(type->result_types_count == 1);

        spidir_value_type_t stype = jit_get_spidir_value_type(type->result_types[0]);
        JIT_PUSH(stype, ret_val);
    }

cleanup:
    wasm_host_free(params);

    return err;
}

static wasm_err_t jit_wasm_call_indirect(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_t* params = nullptr;
    spidir_value_type_t* arg_types = nullptr;

    uint32_t typeidx = BUFFER_PULL_U32(code);
    uint32_t tableidx = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tcall_indirect %d %d", typeidx, tableidx);

    CHECK(typeidx < ctx->module->types_count);
    CHECK(tableidx < ctx->module->tables_count);
    wasm_type_t* type = &ctx->module->types[typeidx];
    jit_table_t* table = &ctx->tables[tableidx];

    // pop the table index
    spidir_value_t idx = JIT_POP(SPIDIR_TYPE_I32);

    // bounds check: trap on idx >= table->length
    spidir_value_t in_bounds = spidir_builder_build_icmp(
        builder,
        SPIDIR_ICMP_ULT, SPIDIR_TYPE_I32,
        idx,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, table->length)
    );
    spidir_block_t ok_block = spidir_builder_create_block(builder);
    spidir_block_t trap_block = spidir_builder_create_block(builder);
    spidir_builder_build_brcond(builder, in_bounds, ok_block, trap_block);

    // out-of-bounds path: emit a real trap call so the optimizer
    // can't eliminate the incoming branch
    spidir_builder_set_block(builder, trap_block);
    RETHROW(jit_emit_trap(builder, ctx));

    // in-bounds path: compute &state_base[table.offset + idx*sizeof(void*)]
    spidir_builder_set_block(builder, ok_block);
    spidir_value_t state_base = spidir_builder_build_param_ref(builder, 1);
    spidir_value_t idx64 = spidir_builder_build_iext(builder, idx);
    idx64 = spidir_builder_build_and(builder, idx64,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));
    spidir_value_t slot_byte_offset = spidir_builder_build_imul(builder, idx64,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, sizeof(void*)));
    spidir_value_t total_offset = spidir_builder_build_iadd(builder, slot_byte_offset,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, table->offset));
    spidir_value_t slot_ptr = spidir_builder_build_ptroff(builder, state_base, total_offset);

    // load the funcref (a host-pointer-sized value)
    spidir_value_t target = spidir_builder_build_load(
        builder,
        SPIDIR_MEM_SIZE_8, SPIDIR_TYPE_PTR,
        slot_ptr
    );

    // build the params: hidden mem/state passthrough, then wasm args.
    size_t arg_count = type->arg_types_count + 2;
    params = CALLOC(spidir_value_t, arg_count);
    CHECK(params != nullptr);
    arg_types = CALLOC(spidir_value_type_t, arg_count);
    CHECK(arg_types != nullptr);

    arg_types[0] = SPIDIR_TYPE_PTR;
    arg_types[1] = SPIDIR_TYPE_PTR;
    params[0] = spidir_builder_build_param_ref(builder, 0);
    params[1] = state_base;

    for (int i = 0; i < type->arg_types_count; i++) {
        size_t ai = type->arg_types_count - i - 1;
        spidir_value_type_t stype = jit_get_spidir_value_type(type->arg_types[ai]);
        params[ai + 2] = JIT_POP(stype);
        arg_types[ai + 2] = stype;
    }

    spidir_value_type_t ret_type = SPIDIR_TYPE_NONE;
    if (type->result_types_count == 1) {
        ret_type = jit_get_spidir_value_type(type->result_types[0]);
    } else {
        CHECK(type->result_types_count == 0);
    }

    spidir_value_t ret = spidir_builder_build_callind(
        builder,
        ret_type, arg_count, arg_types,
        target, params
    );

    if (ret_type != SPIDIR_TYPE_NONE) {
        JIT_PUSH(ret_type, ret);
    }

cleanup:
    wasm_host_free(params);
    wasm_host_free(arg_types);
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Variable Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_local_get(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.get %d", index);
    CHECK(index < func->locals.length);
    spidir_value_t value = func->locals.elements[index].value;
    CHECK(value.id != SPIDIR_VALUE_INVALID.id);
    JIT_PUSH(func->locals.elements[index].type, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_local_set(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.set %d", index);
    CHECK(index < func->locals.length);
    func->locals.elements[index].value = JIT_POP(func->locals.elements[index].type);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_local_tee(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tlocal.tee %d", index);
    CHECK(index < func->locals.length);
    spidir_value_type_t value_type = func->locals.elements[index].type;
    spidir_value_t value = JIT_POP(value_type);
    func->locals.elements[index].value = value;
    JIT_PUSH(value_type, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_global_get(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tglobal.get %d", index);
    CHECK(index < ctx->module->globals_count);

    spidir_value_type_t value_type = ctx->globals[index].type;

    spidir_value_t value = SPIDIR_VALUE_INVALID;
    if (ctx->globals[index].offset == -1) {
        CHECK_FAIL("TODO: immutable globals");

    } else {
        // get the pointer to the global data
        spidir_value_t globals_base = spidir_builder_build_param_ref(builder, 1);
        globals_base = spidir_builder_build_ptroff(builder, globals_base,
            spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, ctx->globals[index].offset));

        // read it
        value = spidir_builder_build_load(
            builder,
            jit_get_spidir_mem_size(value_type),
            value_type,
            globals_base
        );
    }

    // and push
    JIT_PUSH(value_type, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_global_set(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t index = BUFFER_PULL_U32(code);
    JIT_TRACE("wasm: \tglobal.set %d", index);
    CHECK(index < ctx->module->globals_count);

    // ensure this is not an immutable value
    CHECK(ctx->globals[index].offset != -1);

    // get the pointer to the global data
    spidir_value_t globals_base = spidir_builder_build_param_ref(builder, 1);
    globals_base = spidir_builder_build_ptroff(builder, globals_base,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, ctx->globals[index].offset));

    // read it
    spidir_value_type_t value_type = ctx->globals[index].type;
    spidir_value_t value = JIT_POP(value_type);
    spidir_builder_build_store(
        builder,
        jit_get_spidir_mem_size(value_type),
        value,
        globals_base
    );

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Memory instructions
//----------------------------------------------------------------------------------------------------------------------

typedef struct wasm_mem_arg {
    uint32_t index;
    uint32_t align;
    uint64_t offset;
} wasm_mem_arg_t;

static wasm_err_t jit_pull_memarg(buffer_t* buffer, wasm_mem_arg_t* arg) {
    wasm_err_t err = WASM_NO_ERROR;

    arg->align = BUFFER_PULL_U32(buffer);

    // if the value is larger than or equal to 64 then we have an index
    // and the real value is 64 less
    if (arg->align >= 64) {
        arg->align -= 64;
        arg->index = BUFFER_PULL_U32(buffer);
    } else {
        arg->index = 0;
    }

    // validate the final alignment
    CHECK(arg->align < 64);

    // get the offset
    arg->offset = BUFFER_PULL_U64(buffer);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_load(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];

    // get the memory argument
    wasm_mem_arg_t mem_arg = {};
    RETHROW(jit_pull_memarg(code, &mem_arg));
    if (mem_arg.index == 0) {
        JIT_TRACE("wasm: \t%s %d, %llu", g_wasm_opcode_names[opcode], 1 << mem_arg.align, (unsigned long long)mem_arg.offset);
    } else {
        JIT_TRACE("wasm: \t%s %d, %d, %llu", g_wasm_opcode_names[opcode], 1 << mem_arg.align, mem_arg.index, (unsigned long long)mem_arg.offset);
    }

    // figure the exact parameters for the load
    spidir_value_type_t type;
    spidir_mem_size_t mem_size;
    uint32_t sign_extend = 0;
    uint32_t zero_extend = 0;
    switch (opcode) {
        case 0x28: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_4; break;
        case 0x29: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_8; break;
        case 0x2A: type = SPIDIR_TYPE_F32; mem_size = SPIDIR_MEM_SIZE_4; break;
        case 0x2B: type = SPIDIR_TYPE_F64; mem_size = SPIDIR_MEM_SIZE_8; break;
        case 0x2C: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_1; sign_extend = 8; break;
        case 0x2D: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_1; zero_extend = 8; break;
        case 0x2E: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_2; sign_extend = 16; break;
        case 0x2F: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_2; zero_extend = 16; break;
        case 0x30: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_1; sign_extend = 8; break;
        case 0x31: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_1; zero_extend = 8; break;
        case 0x32: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_2; sign_extend = 16; break;
        case 0x33: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_2; zero_extend = 16; break;
        case 0x34: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_4; sign_extend = 32; break;
        case 0x35: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_4; zero_extend = 32; break;
        default: CHECK_FAIL();
    }

    // get the value and offset, the value type depends on the instruction
    spidir_value_t offset = JIT_POP(SPIDIR_TYPE_I32);

    // extend the offset to 64bit
    offset = spidir_builder_build_iext(builder, offset);
    offset = spidir_builder_build_and(builder, offset,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));

    // if we have an offset add it
    if (mem_arg.offset != 0) {
        // because we ensure the offset is at most 32bit, the addition will be 33bit, the runtime
        // ensures an 8GB region per memory instance, so it will always trap no matter what value
        // it gets in here
        CHECK(mem_arg.offset <= UINT32_MAX);
        offset = spidir_builder_build_iadd(builder, offset,
            spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, mem_arg.offset));
    }

    // load the value
    spidir_value_t mem_base = spidir_builder_build_param_ref(builder, 0);
    spidir_value_t value = spidir_builder_build_load(
        builder,
        mem_size, type,
        spidir_builder_build_ptroff(builder, mem_base, offset)
    );

    if (sign_extend != 0) {
        // sign extend from the relevant bit
        value = spidir_builder_build_sfill(builder, sign_extend, value);
    } else if (zero_extend != 0) {
        // perform a mask on the lower bits
        uint64_t mask = (1ull << zero_extend) - 1;
        spidir_value_t mask_value = SPIDIR_VALUE_INVALID;
        if (type == SPIDIR_TYPE_I32) {
            mask_value = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, mask);
        } else {
            mask_value = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, mask);
        }
        value = spidir_builder_build_and(builder, value, mask_value);
    }

    // and finally push it
    JIT_PUSH(type, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_store(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];

    // get the memory argument
    wasm_mem_arg_t mem_arg = {};
    RETHROW(jit_pull_memarg(code, &mem_arg));
    if (mem_arg.index == 0) {
        JIT_TRACE("wasm: \t%s %d, %llu", g_wasm_opcode_names[opcode], 1 << mem_arg.align, (unsigned long long)mem_arg.offset);
    } else {
        JIT_TRACE("wasm: \t%s %d, %d, %llu", g_wasm_opcode_names[opcode], 1 << mem_arg.align, mem_arg.index, (unsigned long long)mem_arg.offset);
    }

    // figure the parameters for the store
    spidir_value_type_t type;
    spidir_mem_size_t mem_size;
    switch (opcode) {
        case 0x36: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_4; break;
        case 0x37: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_8;  break;
        case 0x38: type = SPIDIR_TYPE_F32; mem_size = SPIDIR_MEM_SIZE_4;  break;
        case 0x39: type = SPIDIR_TYPE_F64; mem_size = SPIDIR_MEM_SIZE_8;  break;
        case 0x3A: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_1;  break;
        case 0x3B: type = SPIDIR_TYPE_I32; mem_size = SPIDIR_MEM_SIZE_2;  break;
        case 0x3C: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_1;  break;
        case 0x3D: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_2;  break;
        case 0x3E: type = SPIDIR_TYPE_I64; mem_size = SPIDIR_MEM_SIZE_4;  break;
        default: CHECK_FAIL();
    }

    // get the value and offset, the value type depends on the instruction
    spidir_value_t value = JIT_POP(type);
    spidir_value_t offset = JIT_POP(SPIDIR_TYPE_I32);

    // extend the offset to 64bit
    offset = spidir_builder_build_iext(builder, offset);
    offset = spidir_builder_build_and(builder, offset,
        spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));

    // if we have an offset add it
    if (mem_arg.offset != 0) {
        // because we ensure the offset is at most 32bit, the addition will be 33bit, the runtime
        // ensures an 8GB region per memory instance, so it will always trap no matter what value
        // it gets in here
        CHECK(mem_arg.offset <= UINT32_MAX);
        offset = spidir_builder_build_iadd(builder, offset,
            spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, mem_arg.offset));
    }

    spidir_value_t mem_base = spidir_builder_build_param_ref(builder, 0);
    spidir_builder_build_store(
        builder,
        mem_size,
        value,
        spidir_builder_build_ptroff(builder, mem_base, offset)
    );

cleanup:
    return err;
}

static wasm_err_t jit_wasm_memory_copy(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    // for now we don't have multi-memory support, so it is required to be at zero
    CHECK(BUFFER_PULL(uint8_t, code) == 0);
    CHECK(BUFFER_PULL(uint8_t, code) == 0);
    JIT_TRACE("wasm: \tmemory.copy");

    spidir_value_t n = JIT_POP(SPIDIR_TYPE_I32);
    spidir_value_t src = JIT_POP(SPIDIR_TYPE_I32);
    spidir_value_t dst = JIT_POP(SPIDIR_TYPE_I32);

    spidir_funcref_t helper;
    RETHROW(jit_get_helper(ctx, JIT_HELPER_MEMORY_COPY, &helper));

    spidir_value_t mem_base = spidir_builder_build_param_ref(builder, 0);
    spidir_value_t args[] = { mem_base, dst, src, n };
    spidir_builder_build_call(builder, helper, ARRAY_LENGTH(args), args);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_memory_fill(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    // for now we don't have multi-memory support, so it is required to be at zero
    CHECK(BUFFER_PULL(uint8_t, code) == 0);
    JIT_TRACE("wasm: \tmemory.fill");

    spidir_value_t n   = JIT_POP(SPIDIR_TYPE_I32);
    spidir_value_t val = JIT_POP(SPIDIR_TYPE_I32);
    spidir_value_t dst = JIT_POP(SPIDIR_TYPE_I32);

    spidir_funcref_t helper;
    RETHROW(jit_get_helper(ctx, JIT_HELPER_MEMORY_FILL, &helper));

    spidir_value_t mem_base = spidir_builder_build_param_ref(builder, 0);
    spidir_value_t args[] = { mem_base, dst, val, n };
    spidir_builder_build_call(builder, helper, ARRAY_LENGTH(args), args);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_prefix_fc(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint32_t sub = BUFFER_PULL_U32(code);
    switch (sub) {
        case 10: RETHROW(jit_wasm_memory_copy(builder, code, ctx, func, label)); break;
        case 11: RETHROW(jit_wasm_memory_fill(builder, code, ctx, func, label)); break;
        default: CHECK_FAIL("Unsupported 0xFC sub-opcode %u", sub);
    }

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Numeric Instructions
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_i32_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    int32_t value = BUFFER_PULL_I32(code);
    JIT_TRACE("wasm: \ti32.const %d", value);
    JIT_PUSH(SPIDIR_TYPE_I32, spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, (uint32_t)value));

cleanup:
    return err;
}

static wasm_err_t jit_wasm_i64_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    int64_t value = BUFFER_PULL_I64(code);
    JIT_TRACE("wasm: \ti64.const %lld", (long long)value);
    JIT_PUSH(SPIDIR_TYPE_I64, spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, value));

cleanup:
    return err;
}

static wasm_err_t jit_wasm_f32_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    float value = BUFFER_PULL(float, code);
    JIT_TRACE("wasm: \tf32.const %f", value);
    JIT_PUSH(SPIDIR_TYPE_F32, spidir_builder_build_fconst32(builder, value));

cleanup:
    return err;
}

static wasm_err_t jit_wasm_f64_const(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    double value = BUFFER_PULL(double, code);
    JIT_TRACE("wasm: \tf64.const %lf", value);
    JIT_PUSH(SPIDIR_TYPE_F64, spidir_builder_build_fconst64(builder, value));

cleanup:
    return err;
}

#define SWAP(a, b) \
    do { \
        typeof(a) temp__ = a; \
        a = b; \
        b = temp__; \
    } while (0)

static wasm_err_t jit_wasm_cmpi(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    // figure the exact type
    spidir_value_type_t type;
    switch (opcode) {
        case 0x45 ... 0x4F: type = SPIDIR_TYPE_I32; opcode -= 0x45; break;
        case 0x50 ... 0x5A: type = SPIDIR_TYPE_I64; opcode -= 0x50; break;
        default: CHECK_FAIL();
    }

    // get the args
    spidir_value_t arg2, arg1;
    if (opcode == 0) {
        // eqz
        arg2 = JIT_POP(type);
        arg1 = spidir_builder_build_iconst(builder, type, 0);
    } else {
        arg2 = JIT_POP(type);
        arg1 = JIT_POP(type);
    }

    // choose the compare, got gt kinds we just swap
    spidir_icmp_kind_t kind;
    switch (opcode) {
        case 0:
        case 1: kind = SPIDIR_ICMP_EQ; break;
        case 2: kind = SPIDIR_ICMP_NE; break;
        case 3: kind = SPIDIR_ICMP_SLT; break;
        case 4: kind = SPIDIR_ICMP_ULT; break;
        case 5: kind = SPIDIR_ICMP_SLT; SWAP(arg1, arg2); break;
        case 6: kind = SPIDIR_ICMP_ULT; SWAP(arg1, arg2); break;
        case 7: kind = SPIDIR_ICMP_SLE; break;
        case 8: kind = SPIDIR_ICMP_ULE; break;
        case 9: kind = SPIDIR_ICMP_SLE; SWAP(arg1, arg2); break;
        case 10: kind = SPIDIR_ICMP_ULE; SWAP(arg1, arg2); break;
        default: CHECK_FAIL();
    }

    // build the icmp, it always outputs i32
    spidir_value_t value = spidir_builder_build_icmp(builder,
        kind, SPIDIR_TYPE_I32,
        arg1, arg2
    );

    // push the result
    JIT_PUSH(SPIDIR_TYPE_I32, value);

cleanup:
    return err;
}

// Integer-to-integer conversions: wrap, extend (signed/unsigned), and the
// sign-extension-within-type ops. Skipping reinterpret_* because spidir has
// no bitcast builder.
//   0xA7 i32.wrap_i64     -> itrunc
//   0xAC i64.extend_i32_s -> iext + sfill 32
//   0xAD i64.extend_i32_u -> iext + and 0xFFFFFFFF
//   0xC0 i32.extend8_s    -> sfill 8  (i32)
//   0xC1 i32.extend16_s   -> sfill 16 (i32)
//   0xC2 i64.extend8_s    -> sfill 8  (i64)
//   0xC3 i64.extend16_s   -> sfill 16 (i64)
//   0xC4 i64.extend32_s   -> sfill 32 (i64)
static wasm_err_t jit_wasm_iconv(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    switch (opcode) {
        case 0xA7: {
            spidir_value_t v = JIT_POP(SPIDIR_TYPE_I64);
            JIT_PUSH(SPIDIR_TYPE_I32, spidir_builder_build_itrunc(builder, v));
        } break;

        case 0xAC: {
            spidir_value_t v = JIT_POP(SPIDIR_TYPE_I32);
            spidir_value_t e = spidir_builder_build_iext(builder, v);
            JIT_PUSH(SPIDIR_TYPE_I64, spidir_builder_build_sfill(builder, 32, e));
        } break;

        case 0xAD: {
            spidir_value_t v = JIT_POP(SPIDIR_TYPE_I32);
            spidir_value_t e = spidir_builder_build_iext(builder, v);
            spidir_value_t mask = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF);
            JIT_PUSH(SPIDIR_TYPE_I64, spidir_builder_build_and(builder, e, mask));
        } break;

        case 0xC0 ... 0xC4: {
            // sfill within the operand type
            spidir_value_type_t type = (opcode <= 0xC1) ? SPIDIR_TYPE_I32 : SPIDIR_TYPE_I64;
            uint8_t width;
            switch (opcode) {
                case 0xC0: width = 8;  break;
                case 0xC1: width = 16; break;
                case 0xC2: width = 8;  break;
                case 0xC3: width = 16; break;
                case 0xC4: width = 32; break;
                default: CHECK_FAIL();
            }
            spidir_value_t v = JIT_POP(type);
            JIT_PUSH(type, spidir_builder_build_sfill(builder, width, v));
        } break;

        default: CHECK_FAIL();
    }

cleanup:
    return err;
}

static wasm_err_t jit_wasm_cmpf(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    // figure the exact type
    spidir_value_type_t type;
    switch (opcode) {
        case 0x5B ... 0x60: type = SPIDIR_TYPE_F32; opcode -= 0x5B; break;
        case 0x61 ... 0x66: type = SPIDIR_TYPE_F64; opcode -= 0x61; break;
        default: CHECK_FAIL();
    }

    // get the args
    spidir_value_t arg2 = JIT_POP(type);
    spidir_value_t arg1 = JIT_POP(type);

    // choose the compare, got gt kinds we just swap
    spidir_fcmp_kind_t kind;
    switch (opcode) {
        case 0: kind = SPIDIR_FCMP_OEQ; break;
        case 1: kind = SPIDIR_FCMP_UNE; break;
        case 2: kind = SPIDIR_FCMP_OLT; break;
        case 3: kind = SPIDIR_FCMP_OLT; SWAP(arg1, arg2); break;
        case 4: kind = SPIDIR_FCMP_OLE; break;
        case 5: kind = SPIDIR_FCMP_OLE; SWAP(arg1, arg2); break;
        default: CHECK_FAIL();
    }

    // build the icmp, it always outputs i32
    spidir_value_t value = spidir_builder_build_fcmp(builder,
        kind, SPIDIR_TYPE_I32,
        arg1, arg2
    );

    // push the result
    JIT_PUSH(SPIDIR_TYPE_I32, value);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_itof(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    spidir_value_type_t out_type;
    switch (opcode) {
        case 0xB2 ... 0xB5: out_type = SPIDIR_TYPE_F32; opcode -= 0xB2; break;
        case 0xB7 ... 0xBA: out_type = SPIDIR_TYPE_F64; opcode -= 0xB7; break;
        default: CHECK_FAIL();
    }

    spidir_value_type_t in_type = (opcode & 2) ? SPIDIR_TYPE_I64 : SPIDIR_TYPE_I32;
    bool is_unsigned = opcode & 1;

    spidir_value_t value = JIT_POP(in_type);
    spidir_value_t result = is_unsigned
        ? spidir_builder_build_uinttofloat(builder, out_type, value)
        : spidir_builder_build_sinttofloat(builder, out_type, value);

    JIT_PUSH(out_type, result);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_fconv(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    switch (opcode) {
        case 0xB6: {
            spidir_value_t v = JIT_POP(SPIDIR_TYPE_F64);
            JIT_PUSH(SPIDIR_TYPE_F32, spidir_builder_build_fnarrow(builder, v));
        } break;

        case 0xBB: {
            spidir_value_t v = JIT_POP(SPIDIR_TYPE_F32);
            JIT_PUSH(SPIDIR_TYPE_F64, spidir_builder_build_fwiden(builder, v));
        } break;

        default: CHECK_FAIL();
    }

cleanup:
    return err;
}

static wasm_err_t jit_wasm_binopf(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    spidir_value_type_t type;
    switch (opcode) {
        case 0x92 ... 0x95: type = SPIDIR_TYPE_F32; opcode -= 0x92; break;
        case 0xA0 ... 0xA3: type = SPIDIR_TYPE_F64; opcode -= 0xA0; break;
        default: CHECK_FAIL();
    }

    spidir_value_t arg2 = JIT_POP(type);
    spidir_value_t arg1 = JIT_POP(type);

    spidir_value_t result;
    switch (opcode) {
        case 0: result = spidir_builder_build_fadd(builder, arg1, arg2); break;
        case 1: result = spidir_builder_build_fsub(builder, arg1, arg2); break;
        case 2: result = spidir_builder_build_fmul(builder, arg1, arg2); break;
        case 3: result = spidir_builder_build_fdiv(builder, arg1, arg2); break;
        default: CHECK_FAIL();
    }

    JIT_PUSH(type, result);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_shift(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    spidir_value_type_t type;
    uint64_t mask;
    switch (opcode) {
        case 0x74 ... 0x76: type = SPIDIR_TYPE_I32; mask = 31; opcode -= 0x74; break;
        case 0x86 ... 0x88: type = SPIDIR_TYPE_I64; mask = 63; opcode -= 0x86; break;
        default: CHECK_FAIL();
    }

    // top of stack is the shift amount, below is the value
    spidir_value_t shift_amt = JIT_POP(type);
    spidir_value_t value = JIT_POP(type);

    // wasm reduces the shift count modulo the bit width - mask explicitly
    // so the result is well-defined no matter how the backend handles
    // out-of-range counts.
    shift_amt = spidir_builder_build_and(builder, shift_amt,
        spidir_builder_build_iconst(builder, type, mask));

    spidir_value_t result;
    switch (opcode) {
        case 0: result = spidir_builder_build_shl(builder, value, shift_amt); break;
        case 1: result = spidir_builder_build_ashr(builder, value, shift_amt); break;
        case 2: result = spidir_builder_build_lshr(builder, value, shift_amt); break;
        default: CHECK_FAIL();
    }

    JIT_PUSH(type, result);

cleanup:
    return err;
}

static wasm_err_t jit_wasm_binopi(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    uint8_t opcode = ((uint8_t*)code->data)[-1];
    JIT_TRACE("wasm: \t%s", g_wasm_opcode_names[opcode]);

    // figure the exact type
    spidir_value_type_t type;
    switch (opcode) {
        case 0x6A ... 0x73: type = SPIDIR_TYPE_I32; opcode -= 0x6A; break;
        case 0x7C ... 0x85: type = SPIDIR_TYPE_I64; opcode -= 0x7C; break;
        default: CHECK_FAIL();
    }

    // get the two values
    spidir_value_t arg2 = JIT_POP(type);
    spidir_value_t arg1 = JIT_POP(type);

    // and now perform the action
    spidir_value_t value;
    switch (opcode) {
        case 0: value = spidir_builder_build_iadd(builder, arg1, arg2); break;
        case 1: value = spidir_builder_build_isub(builder, arg1, arg2); break;
        case 2: value = spidir_builder_build_imul(builder, arg1, arg2); break;
        case 3: value = spidir_builder_build_sdiv(builder, arg1, arg2); break;
        case 4: value = spidir_builder_build_udiv(builder, arg1, arg2); break;
        case 5: value = spidir_builder_build_srem(builder, arg1, arg2); break;
        case 6: value = spidir_builder_build_urem(builder, arg1, arg2); break;
        case 7: value = spidir_builder_build_and(builder, arg1, arg2); break;
        case 8: value = spidir_builder_build_or(builder, arg1, arg2); break;
        case 9: value = spidir_builder_build_xor(builder, arg1, arg2); break;
        default: CHECK_FAIL();
    }

    // and push it back
    JIT_PUSH(type, value);

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Instruction lookup table
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_wasm_end(spidir_builder_handle_t builder, buffer_t* code, jit_context_t* ctx, jit_function_ctx_t* func, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    JIT_TRACE("wasm: \tend");

    if (func->labels.length == 1) {
        // can't be a loop
        CHECK(!label->loop);

        if (!label->terminated) {
            // this is a fallthrough from the entire function,
            // handle it like a return
            spidir_value_t value = SPIDIR_VALUE_INVALID;
            if (func->ret_type != SPIDIR_TYPE_NONE) {
                value = JIT_POP(func->ret_type);
            }
            spidir_builder_build_return(builder, value);
        }

    } else if (!label->loop) {
        if (!label->terminated) {
            // handle fallthrough from a block into its label
            RETHROW(jit_wasm_prepare_branch(builder, func, label));
            spidir_builder_build_branch(builder, label->block);
        }

        // copy over all the locals, if we never initialized them
        // we stay with the current locals which is correct
        for (int i = 0; i < func->locals.length; i++) {
            func->locals.elements[i].value = label->locals_values[i];
        }

        // and use the new block
        spidir_builder_set_block(builder, label->block);
    } else {
        spidir_block_t next_block = spidir_builder_create_block(builder);

        if (!label->terminated) {
            // if not terminated fallthrough
            spidir_builder_build_branch(builder, next_block);
        }

        // we continue with the locals that we have currently because
        // this is a fallthrough from a loop (which is the only exit
        // point of the loop)

        spidir_builder_set_block(builder, next_block);
    }

    // stack must be empty at this point
    CHECK(label->stack.length == 0);

    // remove the block
    label = nullptr;
    jit_label_t top_label = vec_pop(&func->labels);
    jit_free_label(&top_label);

cleanup:
    return err;
}

const jit_instruction_t g_wasm_inst_jit_callbacks[0x100] = {
    [0x0B] = jit_wasm_end,

    // Parametric Instructions
    [0x00] = jit_wasm_unreachable,
    [0x01] = jit_wasm_nop,
    [0x1A] = jit_wasm_drop,
    [0x1B] = jit_wasm_select,

    // Control Instructions
    [0x02] = jit_wasm_block,
    [0x03] = jit_wasm_loop,
    [0x0C] = jit_wasm_br,
    [0x0D] = jit_wasm_br_if,
    [0x0F] = jit_wasm_return,
    [0x10] = jit_wasm_call,
    [0x11] = jit_wasm_call_indirect,

    // Variable Instructions
    [0x20] = jit_wasm_local_get,
    [0x21] = jit_wasm_local_set,
    [0x22] = jit_wasm_local_tee,
    [0x23] = jit_wasm_global_get,
    [0x24] = jit_wasm_global_set,

    // Memory Instructions
    [0x28 ... 0x35] = jit_wasm_load,
    [0x36 ... 0x3E] = jit_wasm_store,

    // Multi-byte prefix instructions
    [0xFC] = jit_wasm_prefix_fc,

    // Numeric Instructions
    [0x41] = jit_wasm_i32_const,
    [0x42] = jit_wasm_i64_const,
    [0x45 ... 0x5A] = jit_wasm_cmpi,
    [0x6A ... 0x73] = jit_wasm_binopi,
    [0x74 ... 0x76] = jit_wasm_shift,
    [0x7C ... 0x85] = jit_wasm_binopi,
    [0x86 ... 0x88] = jit_wasm_shift,
    [0x92 ... 0x95] = jit_wasm_binopf,
    [0xA0 ... 0xA3] = jit_wasm_binopf,
    [0x43] = jit_wasm_f32_const,
    [0x44] = jit_wasm_f64_const,
    [0x5B ... 0x66] = jit_wasm_cmpf,

    // Conversions
    [0xA7]          = jit_wasm_iconv,    // i32.wrap_i64
    [0xAC ... 0xAD] = jit_wasm_iconv,    // i64.extend_i32_{s,u}
    [0xB2 ... 0xB5] = jit_wasm_itof,     // f32.convert_i{32,64}_{s,u}
    [0xB6]          = jit_wasm_fconv,    // f32.demote_f64
    [0xB7 ... 0xBA] = jit_wasm_itof,     // f64.convert_i{32,64}_{s,u}
    [0xBB]          = jit_wasm_fconv,    // f64.promote_f32
    [0xC0 ... 0xC4] = jit_wasm_iconv,    // i{32,64}.extend{8,16,32}_s
};
