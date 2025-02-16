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

typedef struct jit_context {
    // the current stack value
    jit_value_t* stack;

    // the locals
    jit_value_t* locals;

    // the function we are jitting
    wasm_func_t* function;
} jit_context_t;

#define SWAP(a, b) \
    ({ \
        typeof(a) __tmp = a; \
        a = b; \
        b = __tmp; \
    })

static wasm_err_t wasm_jit_instr(spidir_builder_handle_t builder, jit_context_t* ctx, uint8_t instr, binary_reader_t* code) {
    wasm_err_t err = WASM_NO_ERROR;

    switch (instr) {
        //--------------------------------------------------------------------------------------------------------------
        // Control instructions
        //--------------------------------------------------------------------------------------------------------------

        case 0x00: // unreachable
            spidir_builder_build_unreachable(builder);
            break;

        case 0x01: // nop
            break;

        // TODO: block
        // TODO: loop
        // TODO: if
        // TODO: if-else
        // TODO: br
        // TODO: br-if
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
        } break;

        // TODO: call
        // TODO: call_indirect

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

        // TODO: global.get
        // TODO: global.set

        //--------------------------------------------------------------------------------------------------------------
        // Memory instructions
        //--------------------------------------------------------------------------------------------------------------

        // TODO:

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

        // TODO: {i32,i64}.eqz

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

        // TODO: other conversions

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
    return err;
}

static wasm_err_t wasm_jit_expr(spidir_builder_handle_t builder, jit_context_t* ctx, binary_reader_t* code) {
    wasm_err_t err = WASM_NO_ERROR;

    // go over all the instructions until we read the end instruction
    for (;;) {
        uint8_t instr = BINARY_READER_PULL_BYTE(code);
        if (instr == 0x0B) {
            break;
        }

        // jit the isntruction
        RETHROW(wasm_jit_instr(builder, ctx, instr, code));
    }

cleanup:
    return err;
}

static void wasm_jit_build_callback(spidir_builder_handle_t builder, void* _ctx) {
    wasm_err_t err = WASM_NO_ERROR;
    jit_build_context_t* ctx = _ctx;

    jit_context_t jit_ctx = {
        .locals = NULL,
        .stack = NULL,
        .function = ctx->function,
    };

    // init the locals array to something known
    arrsetlen(jit_ctx.locals, arrlen(ctx->local_desc));
    for (int64_t i = 0; i < arrlen(jit_ctx.locals); i++) {
        jit_ctx.locals[i].kind = ctx->local_desc[i].type->kind;
        if (i < ctx->function->functype->params.size) {
            jit_ctx.locals[i].value = spidir_builder_build_param_ref(builder, i);
        } else {
            jit_ctx.locals[i].value = SPIDIR_VALUE_INVALID;
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
