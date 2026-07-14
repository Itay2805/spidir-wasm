#include "codegen.h"

#include "jit/helpers.h"
#include "jit/libcall.h"
#include "spidir/codegen.h"
#include "spidir/module.h"
#include "spidir/x64.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/hmap.h"
#include "util/vec.h"
#include "wasm/error.h"
#include "wasm/host.h"
#include "wasm/jit.h"
#include "util/string.h"
#include "jit_internal.h"
#include "wasm/wasm.h"

#include <cpuid.h>
#include <stdint.h>

/**
 * Represents a function with its codegen (a jitted function)
 */
typedef struct function_codegen {
    /** 
     * The codegen blob handle
     */
    spidir_codegen_blob_handle_t blob;

    /**
     * The spidir function we jitted
     */
    spidir_function_t function;

    /**
     * The code offset of this function
     */
    uint32_t code_offset;

    /**
     * The constpool offset for this function
     */
    uint32_t constpool_offset;
} function_codegen_t;

typedef struct codegen_ctx {
    /**
     * The queue of functions to jit, the index is into 
     * the functions array making it easier to deal with
     */
    vec(spidir_function_t) queue;

    /**
     * The list of all the functions we are jitting, in order
     */
    vec(function_codegen_t) functions;

    /**
     * A spidir function to its index in the functions array
     */
    hmap_t func_to_idx;

    /** 
     * Maps a spidir global into an offset
     */
    hmap_t global_offsets;

    /**
     * The import -> address map
     */
    hmap_t imports;

    /**
     * The total size of the code that we need for this
     */
    size_t code_size;

    /**
     * The total size of the rodata that we need for this
     */
    size_t rodata_size;
} codegen_ctx_t;

static spidir_codegen_machine_handle_t m_spidir_machine = nullptr;

static void wasm_jit_init(wasm_jit_config_t* config) {
    if (config->machine_handle == nullptr) {
        spidir_x64_machine_config_t machine_config = {
            .extern_code_model = SPIDIR_X64_CM_LARGE_ABS,
            .internal_code_model = SPIDIR_X64_CM_SMALL_PIC,
        };

        // check for current cpu features by default
        unsigned int eax, ebx, ecx, edx;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            machine_config.cpu_features.popcnt = ecx & bit_POPCNT;
        }

        config->machine_handle = spidir_codegen_create_x64_machine_with_config(&machine_config);
    }
    m_spidir_machine = config->machine_handle;
}

static void* jit_get_indirect(void* func) {
    func -= 4;

    // place an endbr64
    // TODO: ignore when IBT is not supported
    uint8_t* opcode = func;
    opcode[0] = 0xF3;
    opcode[1] = 0x0F;
    opcode[2] = 0x1E;
    opcode[3] = 0xFA;

    return func;
}

static wasm_err_t jit_get_function_addr(
    uint32_t funcidx, 
    wasm_module_jit_t* jit,
    jit_context_t* ctx, codegen_ctx_t* codegen, 
    void** out_addr
) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(ctx->functions[funcidx].inited);
    spidir_funcref_t funcref = ctx->functions[funcidx].spidir;

    if (spidir_funcref_is_external(funcref)) {
        *out_addr = ctx->functions[funcidx].address;

    } else if (spidir_funcref_is_internal(funcref)) {
        // get the entry
        uint64_t index;
        CHECK(hmap_lookup(&codegen->func_to_idx, spidir_funcref_get_internal(funcref).id, &index));
        function_codegen_t* func = &codegen->functions.elements[index];

        // add the exported function as an indirect target
        *out_addr = jit_get_indirect(jit->binary + func->code_offset);
    } else {
        CHECK_FAIL();
    }

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Final linking in memory
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_apply_reloc(uint8_t* code, size_t code_size, const spidir_codegen_reloc_t* reloc, void* target) {
    wasm_err_t err = WASM_NO_ERROR;

    uint64_t F = (uint64_t)target;
    uint64_t P = (uint64_t)code + reloc->offset;
    int64_t A = reloc->addend;

    switch (reloc->kind) {
        case SPIDIR_RELOC_X64_PC32: {
            CHECK(reloc->offset + 4 <= code_size);
            ptrdiff_t value = F + A - P;

            // ensure within signed 32bit range
            CHECK(INT32_MIN <= value);
            CHECK(value <= INT32_MAX);

            // place it
            POKE(uint32_t, P) = value;
        } break;

        case SPIDIR_RELOC_X64_ABS64: {
            CHECK(reloc->offset + 8 <= code_size);
            POKE(uint64_t, P) = F + A;
        } break;

        default:
            CHECK_FAIL();
    }

cleanup:
    return err;
}

static wasm_err_t jit_codegen_link(wasm_module_jit_t* jit, jit_context_t* ctx, codegen_ctx_t* codegen) {
    wasm_err_t err = WASM_NO_ERROR;

    void* jit_code = jit->binary;
    void* jit_rodata = jit->binary + codegen->code_size;

    for (int i = 0; i < codegen->functions.length; i++) {
        function_codegen_t* func = &codegen->functions.elements[i];
        spidir_codegen_blob_handle_t blob = func->blob;

        //
        // copy the blobs
        //

        size_t blob_code_size = spidir_codegen_blob_get_code_size(blob);
        size_t blob_const_size = spidir_codegen_blob_get_constpool_size(blob);

        if (blob_code_size != 0) {
            memcpy(
                jit_code + func->code_offset,
                spidir_codegen_blob_get_code(blob),
                blob_code_size
            );
        }

        if (blob_const_size != 0) {
            memcpy(
                jit_rodata + func->constpool_offset,
                spidir_codegen_blob_get_constpool(blob),
                blob_const_size
            );
        }

        // go over the relocations of the function
        size_t relocs_count = spidir_codegen_blob_get_reloc_count(blob);
        const spidir_codegen_reloc_t* relocs = spidir_codegen_blob_get_relocs(blob);
        for (int j = 0; j < relocs_count; j++) {
            const spidir_codegen_reloc_t* reloc = &relocs[j];

            // Resolve the target.
            void* target = nullptr;

            switch (reloc->target_kind) {
                case SPIDIR_RELOC_TARGET_CONSTPOOL: {
                    target = jit_rodata + func->constpool_offset;
                } break;

                case SPIDIR_RELOC_TARGET_GLOBAL: {
                    uint64_t rodata_offset;
                    CHECK(hmap_lookup(&codegen->global_offsets, reloc->target.global.id, &rodata_offset));
                    target = jit_rodata + rodata_offset;
                } break;

                case SPIDIR_RELOC_TARGET_INTERNAL_FUNCTION: {
                    // get the callee entry
                    uint64_t index;
                    CHECK(hmap_lookup(&codegen->func_to_idx, reloc->target.internal.id, &index));
                    target = jit_code + codegen->functions.elements[index].code_offset;

                    // we assume that the ABS64 will 
                    // have an indirect access
                    if (reloc->kind == SPIDIR_RELOC_X64_ABS64) {
                        target = jit_get_indirect(target);
                    }
                } break;

                case SPIDIR_RELOC_TARGET_EXTERNAL_FUNCTION: {
                    // Externals are either a JIT helper or a wasm import;
                    // both come through this target kind.
                    target = jit_helper_lookup_address(ctx, reloc->target.external.id);
                    if (target == nullptr) {
                        // try to look at imports
                        uint64_t addr;
                        CHECK(hmap_lookup(&codegen->imports, reloc->target.external.id, &addr));
                        target = (void*)addr;
                    }
                } break;

                case SPIDIR_RELOC_TARGET_LIBCALL: {
                    target = jit_resolve_libcall(reloc->target.libcall);
                    CHECK(target != NULL);
                } break;

                default:
                    CHECK_FAIL();
            }

            // actually apply the reloc
            RETHROW(jit_apply_reloc(
                jit_code + func->code_offset,
                blob_code_size,
                reloc,
                target
            ));
        }

        // no longer need the blob, can free it
        func->blob = nullptr;
        spidir_codegen_blob_destroy(blob);
    }

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Codegen for tables
//----------------------------------------------------------------------------------------------------------------------

static wasm_err_t jit_codegen_tables(jit_context_t* ctx, codegen_ctx_t* codegen) {
    wasm_err_t err = WASM_NO_ERROR;

    // lay out the tables in memory, tables are read-only right now and 
    // only contain functions, so we can just add them to the rodata
    for (int i = 0; i < ctx->module->tables_count; i++) {
        jit_table_t* table = &ctx->tables[i];
        if (!table->used) {
            continue;
        }

        codegen->rodata_size = ALIGN_UP(codegen->rodata_size, _Alignof(void*));
        size_t offset = codegen->rodata_size;
        codegen->rodata_size += sizeof(void*) * table->length;

        RETHROW(hmap_insert(&codegen->global_offsets, table->global.id, offset));
    }

cleanup:
    return err;
}

static wasm_err_t jit_codegen_fill_tables(wasm_module_jit_t* jit, jit_context_t* ctx, codegen_ctx_t* codegen) {
    wasm_err_t err = WASM_NO_ERROR;

    for (int64_t i = 0; i < ctx->module->elems_count; i++) {
        wasm_elem_segment_t* elem = &ctx->module->elems[i];
        CHECK(elem->tableidx < ctx->module->tables_count);
        jit_table_t* table = &ctx->tables[elem->tableidx];
        if (!table->used) {
            continue;
        }
        
        // get the rodata offset from the global
        uint64_t rodata_offset = -1;
        CHECK(hmap_lookup(&codegen->global_offsets, table->global.id, &rodata_offset));

        // segment must fit within the table's reserved range
        size_t end_slot;
        CHECK(!__builtin_add_overflow((size_t)elem->offset, (size_t)elem->funcs_count, &end_slot));
        CHECK(end_slot <= table->length);

        // lay out all of the functions in the elements
        void** slots = jit->binary + codegen->code_size + rodata_offset;
        for (int64_t j = 0; j < elem->funcs_count; j++) {
            RETHROW(jit_get_function_addr(elem->funcs[j], jit, ctx, codegen, &slots[elem->offset + j]));
        }
    }

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Codegen for functions
//----------------------------------------------------------------------------------------------------------------------

static bool jit_codegen_visited_function(codegen_ctx_t* codegen, spidir_function_t function) {
    uint64_t index;
    return hmap_lookup(&codegen->func_to_idx, function.id, &index);
}

static wasm_err_t jit_codegen_function(jit_context_t* ctx, codegen_ctx_t* codegen, spidir_function_t function) {
    wasm_err_t err = WASM_NO_ERROR;

    // make sure we don't get any doubles
    if (jit_codegen_visited_function(codegen, function)) {
        goto cleanup;
    }

    // add the function into the list
    function_codegen_t* func = vec_add(&codegen->functions, 1);
    CHECK(func != NULL);
    func->function = function;
    RETHROW(hmap_insert(&codegen->func_to_idx, function.id, codegen->functions.length - 1));

    // actually emit the function
    spidir_codegen_config_t config = {
        .verify_ir = true,
        .verify_regalloc = true,
    };
    spidir_codegen_status_t status = spidir_codegen_emit_function(
        m_spidir_machine, &config, 
        ctx->spidir, function, 
        &func->blob
    );
    CHECK(status == SPIDIR_CODEGEN_OK, "Spidir codegen failed with error %d", status);

    // Allocate code space, align the code to 16 bytes, which are used for both 
    // padding and for adding the ENDBR when needed
    codegen->code_size = ALIGN_UP(codegen->code_size, 16);
    codegen->code_size += 16;
    func->code_offset = codegen->code_size;
    codegen->code_size += spidir_codegen_blob_get_code_size(func->blob);

    // Allocate constpool space
    size_t constpool_size = spidir_codegen_blob_get_constpool_size(func->blob);
    if (constpool_size != 0) {
        codegen->rodata_size += ALIGN_UP(codegen->rodata_size, spidir_codegen_blob_get_constpool_align(func->blob));
        func->constpool_offset = codegen->rodata_size;
        codegen->rodata_size += constpool_size;
    } else {
        func->constpool_offset = -1;
    }

    // now go over the relocations and queue any functions that also need codegen
    size_t reloc_count = spidir_codegen_blob_get_reloc_count(func->blob);
    const spidir_codegen_reloc_t* relocs = spidir_codegen_blob_get_relocs(func->blob);
    for (size_t i = 0; i < reloc_count; i++) {
        const spidir_codegen_reloc_t* reloc = &relocs[i];
        
        if (reloc->target_kind == SPIDIR_RELOC_TARGET_INTERNAL_FUNCTION) {
            // only queue if we didn't visit it yet
            if (!jit_codegen_visited_function(codegen, reloc->target.internal)) {
                vec_push(&codegen->queue, reloc->target.internal);
            }

        }
    }

cleanup:
    return err;
}

static wasm_err_t jit_try_codegen_function(jit_context_t* ctx, codegen_ctx_t* codegen, uint32_t funcidx) {
    wasm_err_t err = WASM_NO_ERROR;

    jit_function_t* func = &ctx->functions[funcidx];
    CHECK(func->inited);

    if (spidir_funcref_is_internal(func->spidir)) {
        RETHROW(jit_codegen_function(ctx, codegen, spidir_funcref_get_internal(func->spidir)));
    }

cleanup:
    return err;
}

static wasm_err_t jit_codegen_functions(jit_context_t* ctx, codegen_ctx_t* codegen) {
    wasm_err_t err = WASM_NO_ERROR;

    // add all the exported functions into the functions
    // that we want to jit
    for (size_t i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_t* export = &ctx->module->exports[i];
        if (export->kind == WASM_EXPORT_FUNC) {
            RETHROW(jit_try_codegen_function(ctx, codegen, export->index));
        }
    }

    // add all the imported functions into the import map
    for (size_t i = 0; i < ctx->module->imports_count; i++) {
        wasm_import_t* import = &ctx->module->imports[i];
        if (import->kind == WASM_EXTERN_FUNC) {
            CHECK(ctx->functions[i].inited);
            CHECK(spidir_funcref_is_external(ctx->functions[i].spidir))
            spidir_function_t function = spidir_funcref_get_external(ctx->functions[i].spidir);

            RETHROW(hmap_insert(
                &codegen->imports, 
                function.id, 
                (uint64_t)ctx->functions[i].address
            ));
        }
    }

    // Anything referenced by an elem segment is also reachable through
    // call_indirect at runtime so it must be prepared and queued for
    // codegen even when no direct call exists in the module.
    for (size_t i = 0; i < ctx->module->elems_count; i++) {
        wasm_elem_segment_t* elem = &ctx->module->elems[i];
        for (uint32_t j = 0; j < elem->funcs_count; j++) {
            RETHROW(jit_try_codegen_function(ctx, codegen, elem->funcs[j]));
        }
    }

    // And the entry-function should also be jitted
    if (ctx->module->start_func >= 0) {
        RETHROW(jit_try_codegen_function(ctx, codegen, ctx->module->start_func));
    }

    // and now codegen until we are done
    while (codegen->queue.length != 0) {
        spidir_function_t func = vec_pop(&codegen->queue);
        RETHROW(jit_codegen_function(ctx, codegen, func));
    }


cleanup:
    // free the queue eagerly
    vec_free(&codegen->queue);

    return err;
}

static wasm_err_t jit_codegen_fill_functions(wasm_module_jit_t* jit, jit_context_t* ctx, codegen_ctx_t* codegen) {
    wasm_err_t err = WASM_NO_ERROR;

    // fill the export table, this will also emit the endbr64
    jit->exports = CALLOC(wasm_jit_export_t, ctx->module->exports_count);
    CHECK(jit->exports != nullptr);

    for (int i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_type_t kind = ctx->module->exports[i].kind;
        uint32_t index = ctx->module->exports[i].index;

        if (kind == WASM_EXPORT_FUNC) {
            RETHROW(jit_get_function_addr(index, jit, ctx, codegen, &jit->exports[i].func.address));

        } else if (kind == WASM_EXPORT_GLOBAL) {
            // get the global's offset
            jit->exports[i].global.offset = ctx->globals[index].offset;

        } else if (kind == WASM_EXPORT_MEMORY) {
            // nothing to do...

        } else {
            CHECK_FAIL();
        }
    }

    // save the entry function
    if (ctx->module->start_func >= 0) {
        void* entry = nullptr;
        RETHROW(jit_get_function_addr(ctx->module->start_func, jit, ctx, codegen, &entry));
        jit->start_func = entry;
    }

cleanup:
    return err;
}

//----------------------------------------------------------------------------------------------------------------------
// Top level codegen function
//----------------------------------------------------------------------------------------------------------------------

wasm_err_t jit_codegen(wasm_module_jit_t* jit, jit_context_t* ctx, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;

    codegen_ctx_t codegen = {};

    if (m_spidir_machine == nullptr) {
        wasm_jit_init(config);
    }

    //
    // jit everything 
    //
    RETHROW(jit_codegen_functions(ctx, &codegen));
    RETHROW(jit_codegen_tables(ctx, &codegen));

    //
    // now we can allocate the entire space for the code
    //

    // align everything to page size
    size_t page_size = wasm_host_page_size();
    codegen.code_size = ALIGN_UP(codegen.code_size, page_size);
    codegen.rodata_size = ALIGN_UP(codegen.rodata_size, page_size);

    // now that we know the sizes allocate the full range
    jit->rx_page_count = codegen.code_size / page_size;
    jit->ro_page_count = codegen.rodata_size / page_size;
    jit->binary = wasm_host_jit_alloc(jit->rx_page_count, jit->ro_page_count);
    CHECK(jit->binary != nullptr);

    void* jit_code = jit->binary;
    void* jit_rodata = jit->binary + codegen.code_size;

    // initialize the jit as required
    if (codegen.code_size != 0) memset(jit_code, 0xCC, codegen.code_size);
    if (codegen.rodata_size != 0) memset(jit_rodata, 0x00, codegen.rodata_size);

    //
    // finally we can link it
    //
    RETHROW(jit_codegen_link(jit, ctx, &codegen));

    //
    // and now fill in all the visible functions with their pointers
    //
    RETHROW(jit_codegen_fill_functions(jit, ctx, &codegen));
    RETHROW(jit_codegen_fill_tables(jit, ctx, &codegen));

    //
    // and now we can finally lock the entire thing
    //
    CHECK(wasm_host_jit_lock(jit->binary, jit->rx_page_count, jit->ro_page_count));
    
cleanup:
    hmap_free(&codegen.global_offsets);
    hmap_free(&codegen.imports);
    hmap_free(&codegen.func_to_idx);
    vec_free(&codegen.queue);
    for (size_t i = 0; i < codegen.functions.length; i++) {
        spidir_codegen_blob_handle_t blob = codegen.functions.elements[i].blob;
        if (blob != nullptr) {
            spidir_codegen_blob_destroy(blob);
        }
    }
    vec_free(&codegen.functions);

    return err;
}
