#include "jit.h"

#include <internal.h>
#include <stb_ds.h>
#include <spidir/module.h>

typedef struct jit_local_desc {
    // the type of the local
    wasm_valtype_t* type;
} jit_local_desc_t;

typedef struct jit_build_context {
    wasm_err_t err;
    binary_reader_t* code;
    wasm_func_t* function;
    wasm_module_t* module;
    jit_local_desc_t* local_desc;
} jit_build_context_t;

typedef struct jit_value {
    wasm_valkind_t kind;
    spidir_value_t value;
} jit_value_t;

typedef struct jit_label {
    // the block of this label
    spidir_block_t label_block;

    // a phi for each of the locals, so whenever we jump to
    // this block we can properly jump to it
    spidir_phi_t* locals_phis;

    // the phi values for the locals to be used in the block
    jit_value_t* local_values;

    // did we terminate the block
    bool block_terminated;

    // if this is a loop it means that the label is not placed at
    // the end, but at the start. and at the end it will need to
    // create a new block
    bool loop;
} jit_label_t;

typedef struct jit_context {
    // the current stack value
    jit_value_t* stack;

    // the locals
    jit_value_t* locals;

    // the stack of labels pushed into the stack
    jit_label_t* labels;

    // the function we are jitting
    wasm_func_t* function;

    // the module we are jitting from
    wasm_module_t* module;
} jit_context_t;

#define SWAP(a, b) \
    ({ \
        typeof(a) __tmp = a; \
        a = b; \
        b = __tmp; \
    })

static spidir_value_type_t wasm_valkind_to_spidir(wasm_valkind_t kind) {
    switch (kind) {
        // numeric types
        case WASM_I32: return SPIDIR_TYPE_I32;
        case WASM_I64: return SPIDIR_TYPE_I64;

        // TODO: float

        // reference types
        case WASM_EXTERNREF:
        case WASM_FUNCREF:
            return SPIDIR_TYPE_PTR;
    }
    __builtin_unreachable();
}

static wasm_err_t wasm_create_label(spidir_builder_handle_t builder, jit_context_t* ctx, binary_reader_t* code, jit_label_t* label) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(code->size >= 1);
    uint8_t op = *(uint8_t*)code->ptr;
    switch (op) {
        // empty type
        case 0x40: BINARY_READER_PULL_BYTE(code); break;

        // // inline result type
        // case 0x7F: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_I32); break;
        // case 0x7E: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_I64); break;
        // case 0x7D: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_F32); break;
        // case 0x7C: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_F64); break;
        // case 0x70: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_FUNCREF); break;
        // case 0x6F: BINARY_READER_PULL_BYTE(code); arrpush(label->result_types, WASM_EXTERNREF); break;
        //
        // // func type
        // default: {
        //     int64_t index = BINARY_READER_PULL_I64(code);
        //     CHECK(index >= 0);
        //     CHECK(index < ctx->module->typefuncs.size);
        //     wasm_functype_t* functype = ctx->module->typefuncs.data[index];
        //
        //     // validate the stack types
        //     CHECK(arrlen(ctx->stack) >= functype->params.size);
        //     for (int i = 0; i < functype->params.size; i++) {
        //         CHECK(ctx->stack[arrlen(ctx->stack) - i - 1].kind == functype->params.data[i]->kind);
        //     }
        //
        //     // add the result types so we can validate branch into this
        //     for (int i = 0; i < functype->params.size; i++) {
        //         arrpush(label->result_types, functype->params.data[i]->kind);
        //     }
        // } break;

        // TODO: support block params
        default:
            CHECK_FAIL();
    }

    // create the end label, and create phis inside of it
    label->label_block = spidir_builder_create_block(builder);
    spidir_block_t current;
    CHECK(spidir_builder_cur_block(builder, &current));
    spidir_builder_set_block(builder, label->label_block);

    // create the phis for the locals
    arrsetlen(label->local_values, arrlen(ctx->locals));
    arrsetlen(label->locals_phis, arrlen(ctx->locals));
    for (int i = 0; i < arrlen(ctx->locals); i++) {
        label->local_values[i].kind = ctx->locals[i].kind;
        label->local_values[i].value = spidir_builder_build_phi(
            builder,
            wasm_valkind_to_spidir(ctx->locals[i].kind),
            0, NULL,
            &label->locals_phis[i]
        );
    }

    spidir_builder_set_block(builder, current);

cleanup:
    return err;
}

static void wasm_prepare_branch(spidir_builder_handle_t builder, jit_context_t* ctx, jit_label_t* label) {
    // add the phi inputs to everything
    for (int i = 0; i < arrlen(ctx->locals); i++) {
        spidir_builder_add_phi_input(builder, label->locals_phis[i], ctx->locals[i].value);
    }
}

static void wasm_enter_label(spidir_builder_handle_t builder, jit_context_t* ctx, jit_label_t* label) {
    // set the new block
    spidir_builder_set_block(builder, label->label_block);

    // copy over the locals
    for (int i = 0; i < arrlen(label->local_values); i++) {
        ctx->locals[i] = label->local_values[i];
    }
}

__attribute__((unused))
static void wasm_debug_print_instr(uint8_t instr, binary_reader_t* _r) {
    wasm_err_t err = WASM_NO_ERROR;

    binary_reader_t code = *_r;
    static int depth = 0;

    switch (instr) {
        case 0x00: TRACE("%*sunreachable", depth, ""); break;
        case 0x01: TRACE("%*snop", depth, ""); break;
        case 0x02: TRACE("%*sblock", depth, ""); depth += 4; break;
        case 0x03: TRACE("%*sloop", depth, ""); depth += 4; break;
        case 0x04: TRACE("%*sif", depth, ""); depth += 4; break;
        case 0x05: TRACE("%*selse", depth, ""); break;
        case 0x0B: depth -= 4; TRACE("%*send", depth, ""); break;
        case 0x0C: TRACE("%*sbr %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x0D: TRACE("%*sbr_if %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x0E: TRACE("%*sbr_table", depth, ""); break;
        case 0x0F: TRACE("%*sreturn", depth, ""); break;

        case 0xD0: TRACE("%*sdrop", depth, ""); break;
        case 0xD1: TRACE("%*sselect", depth, ""); break;

        case 0x20: TRACE("%*slocal.get %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x21: TRACE("%*slocal.set %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x22: TRACE("%*slocal.tee %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x23: TRACE("%*sglobal.get %u", depth, "", BINARY_READER_PULL_U32(&code)); break;
        case 0x24: TRACE("%*sglobal.set %u", depth, "", BINARY_READER_PULL_U32(&code)); break;

        case 0x28: TRACE("%*si32.load {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x29: TRACE("%*si64.load {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2A: TRACE("%*sf32.load {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2B: TRACE("%*sf64.load {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2C: TRACE("%*si32.load8_s {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2D: TRACE("%*si32.load8_u {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2E: TRACE("%*si32.load16_s {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x2F: TRACE("%*si32.load16_u {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x30: TRACE("%*si64.load8_s {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x31: TRACE("%*si64.load8_u {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x32: TRACE("%*si64.load16_s {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x33: TRACE("%*si64.load16_u {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x34: TRACE("%*si64.load32_s {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x35: TRACE("%*si64.load32_u {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x36: TRACE("%*si32.store {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x37: TRACE("%*si64.store {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x38: TRACE("%*sf32.store {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x39: TRACE("%*sf64.store {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3A: TRACE("%*si32.store8 {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3B: TRACE("%*si32.store16 {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3C: TRACE("%*si64.store8 {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3D: TRACE("%*si64.store16 {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3E: TRACE("%*si64.store32 {%u,%u}", depth, "", 1 << BINARY_READER_PULL_U32(&code), BINARY_READER_PULL_U32(&code)); break;
        case 0x3F: TRACE("%*smemory.size", depth, ""); break;
        case 0x40: TRACE("%*smemory.grow", depth, ""); break;

        case 0x41: TRACE("%*si32.const %d", depth, "", BINARY_READER_PULL_I32(&code)); break;
        case 0x42: TRACE("%*si64.const %lld", depth, "", BINARY_READER_PULL_I64(&code)); break;
        case 0x43: TRACE("%*sf32.const x", depth, ""); break;
        case 0x44: TRACE("%*sf64.const x", depth, ""); break;

        case 0x45: TRACE("%*si32.eqz", depth, ""); break;
        case 0x46: TRACE("%*si32.eq", depth, ""); break;
        case 0x47: TRACE("%*si32.ne", depth, ""); break;
        case 0x48: TRACE("%*si32.lt_s", depth, ""); break;
        case 0x49: TRACE("%*si32.lt_u", depth, ""); break;
        case 0x4A: TRACE("%*si32.gt_s", depth, ""); break;
        case 0x4B: TRACE("%*si32.gt_u", depth, ""); break;
        case 0x4C: TRACE("%*si32.le_s", depth, ""); break;
        case 0x4D: TRACE("%*si32.le_u", depth, ""); break;
        case 0x4E: TRACE("%*si32.ge_s", depth, ""); break;
        case 0x4F: TRACE("%*si32.ge_u", depth, ""); break;

        case 0x50: TRACE("%*si64.eqz", depth, ""); break;
        case 0x51: TRACE("%*si64.eq", depth, ""); break;
        case 0x52: TRACE("%*si64.ne", depth, ""); break;
        case 0x53: TRACE("%*si64.lt_s", depth, ""); break;
        case 0x54: TRACE("%*si64.lt_u", depth, ""); break;
        case 0x55: TRACE("%*si64.gt_s", depth, ""); break;
        case 0x56: TRACE("%*si64.gt_u", depth, ""); break;
        case 0x57: TRACE("%*si64.le_s", depth, ""); break;
        case 0x58: TRACE("%*si64.le_u", depth, ""); break;
        case 0x59: TRACE("%*si64.ge_s", depth, ""); break;
        case 0x5A: TRACE("%*si64.ge_u", depth, ""); break;

        case 0x5B: TRACE("%*sf32.eq", depth, ""); break;
        case 0x5C: TRACE("%*sf32.ne", depth, ""); break;
        case 0x5D: TRACE("%*sf32.lt", depth, ""); break;
        case 0x5E: TRACE("%*sf32.gt", depth, ""); break;
        case 0x5F: TRACE("%*sf32.le", depth, ""); break;
        case 0x60: TRACE("%*sf32.ge", depth, ""); break;

        case 0x61: TRACE("%*sf64.eq", depth, ""); break;
        case 0x62: TRACE("%*sf64.ne", depth, ""); break;
        case 0x63: TRACE("%*sf64.lt", depth, ""); break;
        case 0x64: TRACE("%*sf64.gt", depth, ""); break;
        case 0x65: TRACE("%*sf64.le", depth, ""); break;
        case 0x66: TRACE("%*sf64.ge", depth, ""); break;

        case 0x67: TRACE("%*si32.clz", depth, ""); break;
        case 0x68: TRACE("%*si32.ctz", depth, ""); break;
        case 0x69: TRACE("%*si32.popcnt", depth, ""); break;
        case 0x6A: TRACE("%*si32.add", depth, ""); break;
        case 0x6B: TRACE("%*si32.sub", depth, ""); break;
        case 0x6C: TRACE("%*si32.mul", depth, ""); break;
        case 0x6D: TRACE("%*si32.div_s", depth, ""); break;
        case 0x6E: TRACE("%*si32.div_u", depth, ""); break;
        case 0x6F: TRACE("%*si32.rem_s", depth, ""); break;
        case 0x70: TRACE("%*si32.rem_u", depth, ""); break;
        case 0x71: TRACE("%*si32.and", depth, ""); break;
        case 0x72: TRACE("%*si32.or", depth, ""); break;
        case 0x73: TRACE("%*si32.xor", depth, ""); break;
        case 0x74: TRACE("%*si32.shl", depth, ""); break;
        case 0x75: TRACE("%*si32.shr_s", depth, ""); break;
        case 0x76: TRACE("%*si32.shr_u", depth, ""); break;
        case 0x77: TRACE("%*si32.rotl", depth, ""); break;
        case 0x78: TRACE("%*si32.rotr", depth, ""); break;

        case 0x79: TRACE("%*si64.clz", depth, ""); break;
        case 0x7A: TRACE("%*si64.ctz", depth, ""); break;
        case 0x7B: TRACE("%*si64.popcnt", depth, ""); break;
        case 0x7C: TRACE("%*si64.add", depth, ""); break;
        case 0x7D: TRACE("%*si64.sub", depth, ""); break;
        case 0x7E: TRACE("%*si64.mul", depth, ""); break;
        case 0x7F: TRACE("%*si64.div_s", depth, ""); break;
        case 0x80: TRACE("%*si64.div_u", depth, ""); break;
        case 0x81: TRACE("%*si64.rem_s", depth, ""); break;
        case 0x82: TRACE("%*si64.rem_u", depth, ""); break;
        case 0x83: TRACE("%*si64.and", depth, ""); break;
        case 0x84: TRACE("%*si64.or", depth, ""); break;
        case 0x85: TRACE("%*si64.xor", depth, ""); break;
        case 0x86: TRACE("%*si64.shl", depth, ""); break;
        case 0x87: TRACE("%*si64.shr_s", depth, ""); break;
        case 0x88: TRACE("%*si64.shr_u", depth, ""); break;
        case 0x89: TRACE("%*si64.rotl", depth, ""); break;
        case 0x8A: TRACE("%*si64.rotr", depth, ""); break;

        case 0xA7: TRACE("%*si32.wrap_i64", depth, ""); break;

        case 0xAC: TRACE("%*si64.extend_i32_s", depth, ""); break;
        case 0xAD: TRACE("%*si64.extend_i32_u", depth, ""); break;

        case 0xC0: TRACE("%*si32.extend8_s", depth, ""); break;
        case 0xC1: TRACE("%*si32.extend16_s", depth, ""); break;
        case 0xC2: TRACE("%*si64.extend8_s", depth, ""); break;
        case 0xC3: TRACE("%*si64.extend16_s", depth, ""); break;
        case 0xC4: TRACE("%*si64.extend32_s", depth, ""); break;

        default: TRACE("<unknown %02x>", instr); break;
    }

cleanup:
    (void)err;
}

static wasm_err_t wasm_jit_instr(spidir_builder_handle_t builder, jit_context_t* ctx, uint8_t instr, binary_reader_t* code) {
    wasm_err_t err = WASM_NO_ERROR;
    spidir_value_t* value_arr = NULL;

#if 0
    wasm_debug_print_instr(instr, code);
#endif

    switch (instr) {
        //--------------------------------------------------------------------------------------------------------------
        // Control instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x00: // unreachable
        {
            spidir_builder_build_unreachable(builder);
        } break;

        case 0x01: // nop
        {
        } break;

        case 0x02: // block
        {
            jit_label_t* label = arraddnptr(ctx->labels, 1);
            __builtin_memset(label, 0, sizeof(*label));
            RETHROW(wasm_create_label(builder, ctx, code, label));
        } break;

        case 0x03: // loop
        {
            jit_label_t* label = arraddnptr(ctx->labels, 1);
            __builtin_memset(label, 0, sizeof(*label));
            label->loop = true;
            RETHROW(wasm_create_label(builder, ctx, code, label));

            // unlike block, we start from the block
            // start by having all the locals as phis for the input,
            // in case we change them in the loop
            wasm_prepare_branch(builder, ctx, label);

            // enter the loop
            spidir_builder_build_branch(builder, label->label_block);

            // and now enter the label as the first iteration
            wasm_enter_label(builder, ctx, label);
        } break;

        // TODO: if
        // TODO: if-else
        // TODO: br

        case 0x0C: // br
        {
            uint32_t label_index = BINARY_READER_PULL_U32(code);
            jit_label_t* label = &ctx->labels[arrlen(ctx->labels) - label_index - 1];

            // prepare the jump
            wasm_prepare_branch(builder, ctx, label);

            // build the branch
            spidir_builder_build_branch(builder, label->label_block);

            // we terminated the block
            arrlast(ctx->labels).block_terminated = true;
        } break;

        case 0x0D: // br_if
        {
            uint32_t label_index = BINARY_READER_PULL_U32(code);
            jit_label_t* label = &ctx->labels[arrlen(ctx->labels) - label_index - 1];

            // prepare the jump
            wasm_prepare_branch(builder, ctx, label);

            // create the next location
            spidir_block_t next = spidir_builder_create_block(builder);

            // conditionally jump to the next location
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrpop(ctx->stack);
            CHECK(value.kind == WASM_I32);
            spidir_builder_build_brcond(builder, value.value, label->label_block, next);

            // switch the block we are in now
            spidir_builder_set_block(builder, next);
        } break;

        // TODO: br_table

        case 0x0F: // return
        {
            if (ctx->function->functype->results.size != 0) {
                CHECK(arrlen(ctx->stack) == ctx->function->functype->results.size);
                CHECK(arrlen(ctx->stack) == 1);

                // ensure the value matches
                jit_value_t value = arrpop(ctx->stack);
                CHECK(value.kind == ctx->function->functype->results.data[0]->kind);
                spidir_builder_build_return(builder, value.value);
            } else {
                spidir_builder_build_return(builder, SPIDIR_VALUE_INVALID);
            }

            // we terminated the block
            if (arrlen(ctx->labels) >= 1) {
                arrlast(ctx->labels).block_terminated = true;
            }
        } break;

        case 0x10: // call
        {
            uint32_t func_idx = BINARY_READER_PULL_U32(code);
            CHECK(func_idx < arrlen(ctx->module->functions));
            wasm_func_t* func = ctx->module->functions[func_idx];

            // gather all the arguments
            CHECK(arrlen(ctx->stack) >= func->functype->params.size);
            for (int i = 0; i < func->functype->params.size; i++) {
                jit_value_t value = arrpop(ctx->stack);
                CHECK(value.kind == func->functype->params.data[i]->kind);
                arrins(value_arr, 0, value.value);
            }

            // perform the call itself
            spidir_value_t value = spidir_builder_build_call(builder,
                func->jit.function,
                arrlen(value_arr),
                value_arr
            );

            // now push the result
            if (func->functype->results.size == 1) {
                jit_value_t stack_value = {
                    .kind = func->functype->results.data[0]->kind,
                    .value = value
                };
                arrpush(ctx->stack, stack_value);
            } else {
                CHECK(func->functype->results.size == 0);
            }
        } break;

        // TODO: call_indirect

        //--------------------------------------------------------------------------------------------------------------
        // Parametric instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x1B: // select
        {
            CHECK(arrlen(ctx->stack) >= 3);
            jit_value_t condition = arrpop(ctx->stack);
            jit_value_t val2 = arrpop(ctx->stack);
            jit_value_t val1 = arrpop(ctx->stack);

            CHECK(condition.kind == WASM_I32);
            CHECK(val2.kind == val1.kind);

            // choose the type
            spidir_value_type_t spidir_type;
            switch (val1.kind) {
                case WASM_I32: spidir_type = SPIDIR_TYPE_I32; break;
                case WASM_I64: spidir_type = SPIDIR_TYPE_I64; break;
                default: CHECK_FAIL();
            }

            // prepare the next block
            spidir_block_t next_block = spidir_builder_create_block(builder);

            // we are going to use a brcond, if its zero it will take val1 and if its
            // non-zero it will take val2
            spidir_value_t values[] = { val1.value, val2.value };
            spidir_builder_build_brcond(builder, condition.value, next_block, next_block);

            // setup the continuation
            spidir_builder_set_block(builder, next_block);
            val1.value = spidir_builder_build_phi(builder, spidir_type, 2, values, NULL);
            arrpush(ctx->stack, val1);
        } break;

        //--------------------------------------------------------------------------------------------------------------
        // variable instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x20: // local.get
        {
            uint32_t index = BINARY_READER_PULL_U32(code);
            CHECK(index < arrlen(ctx->locals));
            arrpush(ctx->stack, ctx->locals[index]);
        } break;

        case 0x21: // local.set
        {
            uint32_t index = BINARY_READER_PULL_U32(code);
            CHECK(index < arrlen(ctx->locals));

            // get the value from the stack
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrpop(ctx->stack);

            // update the local
            CHECK(ctx->locals[index].kind == value.kind);
            ctx->locals[index] = value;
        } break;

        case 0x22: // local.tee
        {
            uint32_t index = BINARY_READER_PULL_U32(code);
            CHECK(index < arrlen(ctx->locals));

            // get the value from the stack, don't pop it, just read it
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrlast(ctx->stack);

            // update the local
            CHECK(ctx->locals[index].kind == value.kind);
            ctx->locals[index] = value;
        } break;

        case 0x23: // global.get
        {
            uint32_t index = BINARY_READER_PULL_U32(code);
            CHECK(index == 0);
            jit_value_t value = {
                .kind = WASM_I32,
                .value = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, 0)
            };
            arrpush(ctx->stack, value);
        } break;

        // TODO: global.get
        // TODO: global.set

        //--------------------------------------------------------------------------------------------------------------
        // Memory instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x28: // i32.load
        case 0x29: // i64.load
        case 0x2C: // i32.load8_s
        case 0x2D: // i32.load8_u
        case 0x2E: // i32.load16_s
        case 0x2F: // i32.load16_u
        case 0x30: // i64.load8_s
        case 0x31: // i64.load8_u
        case 0x32: // i64.load16_s
        case 0x33: // i64.load16_u
        case 0x34: // i64.load32_s
        case 0x35: // i64.load32_u
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t address = arrpop(ctx->stack);
            CHECK(address.kind == WASM_I32);

            uint32_t align = 1 << BINARY_READER_PULL_U32(code);
            uint32_t offset = BINARY_READER_PULL_U32(code);

            // TODO: unused for now
            (void)align;

            // choose the load parameters
            spidir_mem_size_t size;
            spidir_value_type_t type;
            wasm_valkind_t kind;
            uint8_t sign_extend = 0;
            switch (instr) {
                case 0x28: size = SPIDIR_MEM_SIZE_4; type = SPIDIR_TYPE_I32; kind = WASM_I32; break; // i32.load
                case 0x29: size = SPIDIR_MEM_SIZE_8; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load
                case 0x2C: sign_extend = 8; size = SPIDIR_MEM_SIZE_1; type = SPIDIR_TYPE_I32; kind = WASM_I32; break; // i32.load8_s
                case 0x2D: size = SPIDIR_MEM_SIZE_1; type = SPIDIR_TYPE_I32; kind = WASM_I32; break; // i32.load8_u
                case 0x2E: sign_extend = 16; size = SPIDIR_MEM_SIZE_2; type = SPIDIR_TYPE_I32; kind = WASM_I32; break; // i32.load16_s
                case 0x2F: size = SPIDIR_MEM_SIZE_2; type = SPIDIR_TYPE_I32; kind = WASM_I32; break; // i32.load16_u
                case 0x30: sign_extend = 8; size = SPIDIR_MEM_SIZE_1; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load8_s
                case 0x31: size = SPIDIR_MEM_SIZE_1; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load8_u
                case 0x32: sign_extend = 16; size = SPIDIR_MEM_SIZE_2; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load16_s
                case 0x33: size = SPIDIR_MEM_SIZE_2; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load16_u
                case 0x34: sign_extend = 32; size = SPIDIR_MEM_SIZE_4; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load32_s
                case 0x35: size = SPIDIR_MEM_SIZE_4; type = SPIDIR_TYPE_I64; kind = WASM_I64; break; // i64.load32_u
                default: CHECK_FAIL();
            }

            // create the offset value
            spidir_value_t offset_value = spidir_builder_build_iadd(builder, address.value,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, offset));
            offset_value = spidir_builder_build_iext(builder, offset_value);
            offset_value = spidir_builder_build_and(builder, offset_value,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));

            // Add the memory base
            // TODO: for now we use zero as the base
            spidir_value_t ptr = spidir_builder_build_ptroff(builder,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_PTR, 0),
                offset_value);

            // prepare the value
            jit_value_t value = {
                .kind = kind,
                .value = spidir_builder_build_load(builder, size, type, ptr)
            };

            // check if we need to perform sign extension
            if (sign_extend) {
                value.value = spidir_builder_build_sfill(builder, sign_extend, value.value);
            }

            // and we can push it
            arrpush(ctx->stack, value);
        } break;

        case 0x36: // i32.store
        case 0x37: // i64.store
        case 0x3A: // i32.store8
        case 0x3B: // i32.store16
        case 0x3C: // i64.store8
        case 0x3D: // i64.store16
        case 0x3E: // i64.store32
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t data = arrpop(ctx->stack);
            jit_value_t address = arrpop(ctx->stack);
            CHECK(address.kind == WASM_I32);

            uint32_t align = 1 << BINARY_READER_PULL_U32(code);
            uint32_t offset = BINARY_READER_PULL_U32(code);

            (void)align;

            // choose the load parameters
            spidir_mem_size_t size;
            wasm_valkind_t kind;
            switch (instr) {
                case 0x36: size = SPIDIR_MEM_SIZE_4; kind = WASM_I32; break; // i32.store
                case 0x37: size = SPIDIR_MEM_SIZE_8; kind = WASM_I64; break; // i64.store
                case 0x3A: size = SPIDIR_MEM_SIZE_1; kind = WASM_I32; break; // i32.store8
                case 0x3B: size = SPIDIR_MEM_SIZE_2; kind = WASM_I32; break; // i32.store16
                case 0x3C: size = SPIDIR_MEM_SIZE_1; kind = WASM_I64; break; // i64.store8
                case 0x3D: size = SPIDIR_MEM_SIZE_2; kind = WASM_I64; break; // i64.store16
                case 0x3E: size = SPIDIR_MEM_SIZE_4; kind = WASM_I64; break; // i64.store32
                default: CHECK_FAIL();
            }
            CHECK(data.kind == kind);

            // create the offset value
            spidir_value_t offset_value = spidir_builder_build_iadd(builder, address.value,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, offset));
            offset_value = spidir_builder_build_iext(builder, offset_value);
            offset_value = spidir_builder_build_and(builder, offset_value,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));

            // Add the memory base
            // TODO: for now we use zero as the base
            spidir_value_t ptr = spidir_builder_build_ptroff(builder,
                spidir_builder_build_iconst(builder, SPIDIR_TYPE_PTR, 0),
                offset_value);

            spidir_builder_build_store(builder, size, data.value, ptr);
        } break;

        //--------------------------------------------------------------------------------------------------------------
        // Numeric instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x41: // i32.const
        {
            uint32_t imm = BINARY_READER_PULL_I32(code);
            jit_value_t value = {
                .kind = WASM_I32,
                .value = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I32, imm)
            };
            arrpush(ctx->stack, value);
        } break;

        case 0x42: // i64.const
        {
            uint64_t imm = BINARY_READER_PULL_I64(code);
            jit_value_t value = {
                .kind = WASM_I64,
                .value = spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, imm)
            };
            arrpush(ctx->stack, value);
        } break;

        // Compare to zero
        case 0x45: case 0x50: // {i32,i64}.eqz
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t c1 = arrpop(ctx->stack);

            spidir_value_type_t type;
            if (instr == 0x45) {
                CHECK(c1.kind == WASM_I32);
                type = SPIDIR_TYPE_I32;
            } else {
                CHECK(c1.kind == WASM_I64);
                type = SPIDIR_TYPE_I64;
            }

            // perform the operation and push it, the
            // compare always results in a 32bit immediate
            jit_value_t result_value = {
                .kind = WASM_I32,
                .value = spidir_builder_build_icmp(builder, SPIDIR_ICMP_EQ, SPIDIR_TYPE_I32, c1.value,
                    spidir_builder_build_iconst(builder, type, 0))
            };
            arrpush(ctx->stack, result_value);
        } break;

        // Compare operations
        case 0x46: case 0x51: // {i32,i64}.eq
        case 0x47: case 0x52: // {i32,i64}.ne
        case 0x48: case 0x53: // {i32,i64}.lt_s
        case 0x49: case 0x54: // {i32,i64}.lt_u
        case 0x4A: case 0x55: // {i32,i64}.gt_s
        case 0x4B: case 0x56: // {i32,i64}.gt_u
        case 0x4C: case 0x57: // {i32,i64}.le_s
        case 0x4D: case 0x58: // {i32,i64}.le_u
        case 0x4E: case 0x59: // {i32,i64}.ge_s
        case 0x4F: case 0x5A: // {i32,i64}.ge_u
        {
            CHECK(arrlen(ctx->stack) >= 2);
            jit_value_t c2 = arrpop(ctx->stack);
            jit_value_t c1 = arrpop(ctx->stack);

            // check the types
            if (0x46 <= instr && instr <= 0x4F) {
                CHECK(c2.kind == WASM_I32);
                CHECK(c1.kind == WASM_I32);
            } else {
                CHECK(c2.kind == WASM_I64);
                CHECK(c1.kind == WASM_I64);
            }

            // choose the kind, note that spidir doesn't have a GT/GE operations, instead
            // we need to swap the operands
            spidir_icmp_kind_t icmp_kind;
            switch (instr) {
                case 0x46: case 0x51: icmp_kind = SPIDIR_ICMP_EQ; break;
                case 0x47: case 0x52: icmp_kind = SPIDIR_ICMP_NE; break;
                case 0x48: case 0x53: icmp_kind = SPIDIR_ICMP_SLT; break;
                case 0x49: case 0x54: icmp_kind = SPIDIR_ICMP_ULT; break;
                case 0x4A: case 0x55: icmp_kind = SPIDIR_ICMP_SLT; SWAP(c1, c2); break;
                case 0x4B: case 0x56: icmp_kind = SPIDIR_ICMP_ULT; SWAP(c1, c2); break;
                case 0x4C: case 0x57: icmp_kind = SPIDIR_ICMP_SLE; break;
                case 0x4D: case 0x58: icmp_kind = SPIDIR_ICMP_ULE; break;
                case 0x4E: case 0x59: icmp_kind = SPIDIR_ICMP_SLE; SWAP(c1, c2); break;
                case 0x4F: case 0x5A: icmp_kind = SPIDIR_ICMP_ULE; SWAP(c1, c2); break;
                default: CHECK_FAIL();
            }

            // perform the operation and push it, the
            // compare always results in a 32bit immediate
            jit_value_t result_value = {
                .kind = WASM_I32,
                .value = spidir_builder_build_icmp(builder, icmp_kind, SPIDIR_TYPE_I32, c1.value, c2.value)
            };
            arrpush(ctx->stack, result_value);
        } break;

        // TODO: {i32,i64}.{clz,ctz,popcnt,rotl,rotr}

        // Binary operations
        case 0x6a: case 0x7C: // {i32,i64}.add
        case 0x6B: case 0x7D: // {i32,i64}.sub
        case 0x6C: case 0x7E: // {i32,i64}.mul
        case 0x6D: case 0x7F: // {i32,i64}.div_s
        case 0x6E: case 0x80: // {i32,i64}.div_u
        case 0x6F: case 0x81: // {i32,i64}.rem_s
        case 0x70: case 0x82: // {i32,i64}.rem_u
        case 0x71: case 0x83: // {i32,i64}.and
        case 0x72: case 0x84: // {i32,i64}.or
        case 0x73: case 0x85: // {i32,i64}.xor
        case 0x74: case 0x86: // {i32,i64}.shl
        case 0x75: case 0x87: // {i32,i64}.shr_s
        case 0x76: case 0x88: // {i32,i64}.shr_u
        {
            CHECK(arrlen(ctx->stack) >= 2);
            jit_value_t c2 = arrpop(ctx->stack);
            jit_value_t c1 = arrpop(ctx->stack);

            // check the types
            if (0x6A <= instr && instr <= 0x76) {
                CHECK(c2.kind == WASM_I32);
                CHECK(c1.kind == WASM_I32);
            } else {
                CHECK(c2.kind == WASM_I64);
                CHECK(c1.kind == WASM_I64);
            }

            // now perform it
            spidir_value_t result;
            switch (instr) {
                case 0x6a: case 0x7C: result = spidir_builder_build_iadd(builder, c1.value, c2.value); break;
                case 0x6B: case 0x7D: result = spidir_builder_build_isub(builder, c1.value, c2.value); break;
                case 0x6C: case 0x7E: result = spidir_builder_build_imul(builder, c1.value, c2.value); break;
                case 0x6D: case 0x7F: result = spidir_builder_build_sdiv(builder, c1.value, c2.value); break;
                case 0x6E: case 0x80: result = spidir_builder_build_udiv(builder, c1.value, c2.value); break;
                case 0x6F: case 0x81: result = spidir_builder_build_srem(builder, c1.value, c2.value); break;
                case 0x70: case 0x82: result = spidir_builder_build_urem(builder, c1.value, c2.value); break;
                case 0x71: case 0x83: result = spidir_builder_build_and(builder, c1.value, c2.value); break;
                case 0x72: case 0x84: result = spidir_builder_build_or(builder, c1.value, c2.value); break;
                case 0x73: case 0x85: result = spidir_builder_build_xor(builder, c1.value, c2.value); break;
                case 0x74: case 0x86: result = spidir_builder_build_shl(builder, c1.value, c2.value); break;
                case 0x75: case 0x87: result = spidir_builder_build_ashr(builder, c1.value, c2.value); break;
                case 0x76: case 0x88: result = spidir_builder_build_lshr(builder, c1.value, c2.value); break;
                default: CHECK_FAIL();
            }

            // push the result
            jit_value_t result_value = {
                .kind = c1.kind,
                .value = result
            };
            arrpush(ctx->stack, result_value);
        } break;

        // TODO: F32 operations

        // TODO: F64 operations

        case 0xA7: // i32.wrap_i64
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrpop(ctx->stack);
            CHECK(value.kind == WASM_I64);
            value.kind = WASM_I32;
            value.value = spidir_builder_build_itrunc(builder, value.value);
            arrpush(ctx->stack, value);
        } break;

        case 0xAC: // i64.extend_i32_s
        case 0xAD: // i64.extend_i32_u
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrpop(ctx->stack);
            CHECK(value.kind == WASM_I32);
            value.kind = WASM_I64;
            value.value = spidir_builder_build_iext(builder, value.value);
            if (instr == 0xAC) {
                value.value = spidir_builder_build_sfill(builder, 32, value.value);
            } else {
                value.value = spidir_builder_build_and(builder, value.value,
                    spidir_builder_build_iconst(builder, SPIDIR_TYPE_I64, 0xFFFFFFFF));
            }
            arrpush(ctx->stack, value);
        } break;

        case 0xC0: case 0xC2: // {i32,i64}.extend8_s
        case 0xC1: case 0xC3: // {i32,i64}.extend16_s
        case 0xC4: // i64.extend32_s
        {
            CHECK(arrlen(ctx->stack) >= 1);
            jit_value_t value = arrpop(ctx->stack);

            // check the type
            if (instr == 0xC0 || instr == 0xC1) {
                CHECK(value.kind == WASM_I32);
            } else {
                CHECK(value.kind == WASM_I64);
            }

            // figure the bit width
            uint8_t width = 0;
            switch (instr) {
                case 0xC0: case 0xC2: width = 8; break;
                case 0xC1: case 0xC3: width = 16; break;
                case 0xC4: width = 32; break;
                default: CHECK_FAIL();
            }

            // perform the extension
            value.value = spidir_builder_build_sfill(builder, width, value.value);

            arrpush(ctx->stack, value);
        } break;

        //--------------------------------------------------------------------------------------------------------------
        // unknown
        //--------------------------------------------------------------------------------------------------------------
        default:
            CHECK_FAIL("Unknown instruction: %02x", instr);
    }

cleanup:
    arrfree(value_arr);
    return err;
}

static wasm_err_t wasm_jit_expr(spidir_builder_handle_t builder, jit_context_t* ctx, binary_reader_t* code) {
    wasm_err_t err = WASM_NO_ERROR;

    // go over all the instructions until we read the end instruction
    for (;;) {
        uint8_t instr = BINARY_READER_PULL_BYTE(code);
        if (instr == 0x0B) { // end
            // the end instruction, in this case we need to switch
            // to a different block
            if (arrlen(ctx->labels) != 0) {
                jit_label_t label = arrpop(ctx->labels);

                if (!label.loop) {
                    // if the label we were inside was not terminated properly
                    // terminate it right now
                    if (!label.block_terminated) {
                        wasm_prepare_branch(builder, ctx, &label);
                        spidir_builder_build_branch(builder, label.label_block);
                    }

                    // we are now in the new block
                    wasm_enter_label(builder, ctx, &label);
                } else {
                    // if we are exiting a loop and we are terminated, it means
                    // that there is no way to exit the loop normally, and only
                    // using some other block, so we will create a dummy block
                    // just so it won't complain about terminating a block multiple
                    // times
                    if (label.block_terminated) {
                        spidir_builder_set_block(builder, spidir_builder_create_block(builder));
                    }
                }

                // we no longer need the phis, since this label
                // can no longer be jumped to
                arrfree(label.locals_phis);
                arrfree(label.local_values);

                // and continue to run new instructions
                continue;
            } else {
                // no more labels, we are done
                break;
            }
        }

        // jit the isntruction
        RETHROW(wasm_jit_instr(builder, ctx, instr, code));
    }

cleanup:
    for (int i = 0; i < arrlen(ctx->labels); i++) {
        arrfree(ctx->labels[i].locals_phis);
        arrfree(ctx->labels[i].local_values);
    }
    arrfree(ctx->labels);

    return err;
}

static void wasm_jit_build_callback(spidir_builder_handle_t builder, void* _ctx) {
    wasm_err_t err = WASM_NO_ERROR;
    jit_build_context_t* ctx = _ctx;

    jit_context_t jit_ctx = {
        .locals = NULL,
        .stack = NULL,
        .function = ctx->function,
        .module = ctx->module
    };

    // init the locals array to something known
    arrsetlen(jit_ctx.locals, arrlen(ctx->local_desc));
    for (int64_t i = 0; i < arrlen(jit_ctx.locals); i++) {
        jit_ctx.locals[i].kind = ctx->local_desc[i].type->kind;
        if (i < ctx->function->functype->params.size) {
            jit_ctx.locals[i].value = spidir_builder_build_param_ref(builder, i);
        } else {
            // TODO: maybe later do something
            jit_ctx.locals[i].value = spidir_builder_build_iconst(builder, wasm_valkind_to_spidir(jit_ctx.locals[i].kind), 0);
        }
    }

    // create the entry block
    spidir_block_t entry_block = spidir_builder_create_block(builder);
    spidir_builder_set_entry_block(builder, entry_block);
    spidir_builder_set_block(builder, entry_block);

    // we start from an expression
    RETHROW(wasm_jit_expr(builder, &jit_ctx, ctx->code));

    // ensure we got to the end of the code block
    CHECK(ctx->code->size == 0);

    // build an implicit return
    RETHROW(wasm_jit_instr(builder, &jit_ctx, 0x0F, ctx->code));

cleanup:
    arrfree(jit_ctx.stack);
    arrfree(jit_ctx.locals);
    arrfree(jit_ctx.labels);

    ctx->err = err;
}

wasm_err_t wasm_jit_function(
    spidir_module_handle_t spidir_module,
    wasm_module_t* module,
    wasm_func_t* func,
    binary_reader_t* code
) {
    wasm_err_t err = WASM_NO_ERROR;
    jit_local_desc_t* locals = NULL;
    wasm_valtype_t* local_type = NULL;

    // The jit currently only supports up to one return value
    // fail if there are too many
    CHECK(func->functype->results.size <= 1);

    // add the parameters as the first locals
    for (int64_t i = 0; i < func->functype->params.size; i++) {
        jit_local_desc_t desc = {
            .type = func->functype->params.data[i]
        };
        arrpush(locals, desc);
    }

    // parse all the non-parameter locals
    uint32_t locals_count = BINARY_READER_PULL_U32(code);
    for (int64_t i = 0; i < locals_count; i++) {
        // get the local descriptor
        uint32_t count = BINARY_READER_PULL_U32(code);
        local_type = wasm_parse_valtype(BINARY_READER_PULL_BYTE(code));
        CHECK(local_type != NULL);

        // duplicate as many times as we need
        for (int64_t j = 0; j < count; j++) {
            jit_local_desc_t desc = {0};
            desc.type = wasm_valtype_copy(local_type);
            CHECK(desc.type != NULL);
            arrpush(locals, desc);
        }

        // we are done with it
        wasm_valtype_delete(local_type);
        local_type = NULL;
    }

    // and now call the build function
    jit_build_context_t ctx = {
        .code = code,
        .module = module,
        .function = func,
        .local_desc = locals,
        .err = WASM_NO_ERROR
    };
    spidir_module_build_function(
        spidir_module,
        func->jit.function,
        wasm_jit_build_callback,
        &ctx
    );
    RETHROW(ctx.err);

cleanup:
    // free the temp locals type
    wasm_valtype_delete(local_type);

    // free the locals
    for (int i = 0; i < arrlen(locals); i++) {
        wasm_valtype_delete(locals[i].type);
    }
    arrfree(locals);

    return err;
}
