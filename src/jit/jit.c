#include "wasm/jit.h"

#include "function.h"
#include "jit/helpers.h"
#include "jit_internal.h"
#include "libcall.h"
#include "buffer.h"

#include "spidir/module.h"
#include "util/defs.h"
#include "util/except.h"
#include "util/string.h"

#include "spidir/log.h"
#include "spidir/codegen.h"
#include "spidir/x64.h"
#include "spidir/opt.h"
#include "wasm/error.h"
#include "wasm/host.h"
#include <stdint.h>
#include <cpuid.h>

#include "util/hmap.h"
#include "wasm/wasm.h"

static spidir_codegen_machine_handle_t g_spidir_machine = nullptr;

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
    g_spidir_machine = config->machine_handle;
}

void wasm_module_jit_free(wasm_module_jit_t* jit) {
    if (jit->binary != nullptr) {
        wasm_host_jit_free(jit->binary, jit->rx_page_count, jit->ro_page_count);
        jit->binary = nullptr;
    }
    wasm_host_free(jit->exports);
    wasm_host_free(jit->state_init);
    wasm_host_free(jit->debug.funcs);
    wasm_host_free(jit->debug.relocs);
    jit->exports = nullptr;
    jit->state_init = nullptr;
    jit->debug.funcs = nullptr;
    jit->debug.relocs = nullptr;
    jit->debug.funcs_count = 0;
    jit->debug.relocs_count = 0;
}

static wasm_err_t jit_prepare_table(jit_context_t* ctx, uint32_t id) {
    wasm_err_t err = WASM_NO_ERROR;

    // generate a name for the table
    char name[64];
    name[0] = 't';
    name[1] = 'a';
    name[2] = 'b';
    name[3] = 'l';
    name[4] = 'e';
    name[5] = '_';
    int digits = u64toa(id, &name[6]);
    name[6 + digits] = '\0';

    // create it as a relocation
    // TODO: when we support expanding tables we will need to do something 
    //       about it
    ctx->tables[id].global = spidir_module_create_extern_global(ctx->spidir, name);
    ctx->tables[id].length = ctx->module->tables[id].min;

cleanup:
    return err;
}

static wasm_err_t jit_emit_spidir(jit_context_t* ctx) {
    wasm_err_t err = WASM_NO_ERROR;

    // add all the exported functions into the functions
    // that we want to jit
    for (int i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_t* export = &ctx->module->exports[i];
        if (export->kind == WASM_EXPORT_FUNC) {
            RETHROW(jit_prepare_function(ctx, export->index));
        }
    }

    // prepare the tables
    for (int i = 0; i < ctx->module->tables_count; i++) {
        RETHROW(jit_prepare_table(ctx, i));
    } 

    // Anything referenced by an elem segment is also reachable through
    // call_indirect at runtime so it must be prepared and queued for
    // codegen even when no direct call exists in the module.
    for (int i = 0; i < ctx->module->elems_count; i++) {
        wasm_elem_segment_t* elem = &ctx->module->elems[i];
        for (uint32_t j = 0; j < elem->funcs_count; j++) {
            RETHROW(jit_prepare_function(ctx, elem->funcs[j]));
        }
    }

    // And the entry-function should also be jitted
    if (ctx->module->start_func >= 0) {
        RETHROW(jit_prepare_function(ctx, ctx->module->start_func));
    }

    // prepare the IR for everything
    while (ctx->queue.length != 0) {
        uint32_t funcidx = vec_pop(&ctx->queue);
        RETHROW(jit_function(ctx, funcidx));
    }

cleanup:
    return err;
}

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

typedef union code_map_entry {
    struct {
        uint32_t code_offset;
        uint32_t rodata_offset;
    };
    uint64_t value;
} code_map_entry_t;

static wasm_err_t jit_get_function_addr(uint32_t index, jit_context_t* ctx, void* jit_code, hmap_t* code_map, void** out_addr) {
    wasm_err_t err = WASM_NO_ERROR;

    CHECK(ctx->functions[index].inited);
    spidir_funcref_t funcref = ctx->functions[index].spidir;

    if (spidir_funcref_is_external(funcref)) {
        *out_addr = ctx->functions[index].address;

    } else if (spidir_funcref_is_internal(funcref)) {
        // get the entry
        code_map_entry_t entry = {};
        CHECK(hmap_lookup(code_map, spidir_funcref_get_internal(funcref).id, &entry.value));

        // add the exported function as an indirect target
        *out_addr = jit_get_indirect(jit_code + entry.code_offset);
    } else {
        CHECK_FAIL();
    }

cleanup:
    return err;
}

static wasm_err_t jit_build_state_init(jit_context_t* ctx, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    if (jit->state_size == 0) {
        goto cleanup;
    }

    jit->state_init = wasm_host_calloc(1, jit->state_size);
    CHECK(jit->state_init != nullptr);

    //
    // lay out the global values
    //

    for (int64_t i = 0; i < ctx->module->globals_count; i++) {
        wasm_global_t* global = &ctx->module->globals[i];
        if (!global->mutable) continue;
        jit_global_t* jit_global = &ctx->globals[i];

        void* data = jit->state_init + jit_global->offset;
        switch (global->value.kind) {
            case WASM_VALUE_TYPE_F32: POKE(float, data) = global->value.value.f32; break;
            case WASM_VALUE_TYPE_F64: POKE(double, data) = global->value.value.f64; break;
            case WASM_VALUE_TYPE_I32: POKE(int32_t, data) = global->value.value.i32; break;
            case WASM_VALUE_TYPE_I64: POKE(int64_t, data) = global->value.value.i64; break;
            default: CHECK_FAIL();
        }
    }

    //
    // lay out data entries
    //

    for (int64_t i = 0; i < ctx->module->data_count; i++) {
        wasm_data_t* data = &ctx->module->data[i];
        if (!data->active) {
            jit_data_t* jit_data = &ctx->data[i];

            // TODO: make them be their own allocation maybe? how should memory management 
            //       for these work? do we even really need it to be deallocated on 
            //       data.drop?
            void** data_ptr = jit->state_init + jit_data->offset;
            *data_ptr = ctx->module->data[i].data;
        }
    }

cleanup:
    return err;
}

static wasm_err_t jit_emit_code(jit_context_t* ctx, wasm_module_jit_t* jit, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;

    // TODO: non-lazy init or something
    if (g_spidir_machine == nullptr) {
        wasm_jit_init(config);
    }

    // Whether we're populating jit->debug. Off by default — capturing per-
    // function layout and a vec of resolved relocations adds an allocation
    // per function plus one per reloc, which the runtime never reads. Hosts
    // that want a debug ELF / GDB JIT registration set the flag and pay
    // those bytes; everyone else gets the lean path.
    bool capture_debug = config->emit_debug_info;

    vec(spidir_codegen_blob_handle_t) blobs = {};
    vec(spidir_function_t) functions = {};

    // Debug-only buffers. Left zero-initialized when the flag is off — the
    // empty vec / HMAP_INIT cost nothing to free.
    vec(wasm_jit_reloc_t) debug_relocs = {};

    // maps
    hmap_t code_map = HMAP_INIT;
    hmap_t import_map = HMAP_INIT;
    hmap_t global_map = HMAP_INIT;

    // Reverse maps: spidir function id → wasm funcidx. Only populated when
    // capture_debug is on, so the relocations vec can name a wasm-level
    // target rather than an opaque spidir id. Internal funcs go into
    // func_to_idx; imports get a separate map keyed by
    // spidir_extern_function_t.id (jit_function_t.spidir is an external
    // funcref for those).
    hmap_t func_to_idx = HMAP_INIT;
    hmap_t extern_to_idx = HMAP_INIT;

    spidir_codegen_config_t codgen_config = {
        .verify_ir = true,
        .verify_regalloc = true
    };

    // TODO: generate functions lazily based on relocations
    //       so once spidir can inline we only generate whatever
    //       that is needed (we already only generate functions that
    //       are called at the wasm level)

    size_t code_size = 0;
    size_t rodata_size = 0;

    // now go over and jit everything. The functions array is sized as
    // imports_count + functions_count and indexed in wasm-funcidx order
    // (imports first, then internal funcs), so the loop bound has to span
    // both ranges.
    size_t total_funcs = ctx->module->imports_count + ctx->module->functions_count;
    for (int64_t i = 0; i < total_funcs; i++) {
        if (!ctx->functions[i].inited) {
            continue;
        }

        spidir_funcref_t funcref = ctx->functions[i].spidir;
        if (!spidir_funcref_is_internal(funcref)) {
            // remember in the hmap that this exists
            spidir_function_t func = spidir_funcref_get_external(funcref);
            hmap_insert(&import_map, func.id, (uintptr_t)ctx->functions[i].address);
            // Track the import so the debug ELF can label external relocations
            // against it. Imports use spidir extern ids, which are disjoint
            // from internal ids — keep them in their own map.
            if (capture_debug) {
                hmap_insert(&extern_to_idx, func.id, (uint64_t)i);
            }
            continue;
        }

        spidir_function_t func = spidir_funcref_get_internal(funcref);
        if (capture_debug) {
            hmap_insert(&func_to_idx, func.id, (uint64_t)i);
        }

        // actually emit the function
        spidir_codegen_blob_handle_t blob;
        spidir_codegen_status_t status = spidir_codegen_emit_function(
            g_spidir_machine,
            &codgen_config,
            ctx->spidir, func,
            &blob
        );
        CHECK(status == SPIDIR_CODEGEN_OK, "%d", status);

        // push the blob and the function next to
        // it for easier lookups later
        vec_push(&blobs, blob);
        vec_push(&functions, func);

        //
        // constpool
        //

        size_t constpool_size = spidir_codegen_blob_get_constpool_size(blob);
        size_t rodata_offset = -1;
        if (constpool_size != 0) {
            rodata_size += ALIGN_UP(rodata_size, spidir_codegen_blob_get_constpool_align(blob));
            rodata_offset = rodata_size;
            rodata_size += constpool_size;
        }

        //
        // code
        //

        // we align the code and pad it with 16 bytes, this
        // place will also be used for indirect jumps
        code_size = ALIGN_UP(code_size, 16);
        code_size += 16;
        size_t code_offset = code_size;
        code_size += spidir_codegen_blob_get_code_size(blob);

        code_map_entry_t entry = {
            .code_offset = code_offset,
            .rodata_offset = rodata_offset,
        };
        hmap_insert(&code_map, func.id, entry.value);
    }
    
    // lay out the tables in memory, tables are read-only right now and 
    // only contain functions, so we can just add them to the rodata
    for (int i = 0; i < ctx->module->tables_count; i++) {
        jit_table_t* table = &ctx->tables[i];
        if (!table->used) {
            continue;
        }

        rodata_size = ALIGN_UP(rodata_size, _Alignof(void*));
        size_t offset = rodata_size;
        rodata_size += sizeof(void*) * table->length;

        hmap_insert(&global_map, table->global.id, offset);
    }

    size_t code_size_orig = code_size;
    size_t rodata_size_orig = rodata_size;

    // align everything to page size
    size_t page_size = wasm_host_page_size();
    code_size = ALIGN_UP(code_size, page_size);
    rodata_size = ALIGN_UP(rodata_size, page_size);

    // now that we know the sizes allocate the full range
    jit->rx_page_count = code_size / page_size;
    jit->ro_page_count = rodata_size / page_size;
    jit->binary = wasm_host_jit_alloc(jit->rx_page_count, jit->ro_page_count);
    CHECK(jit->binary != nullptr);

    void* jit_code = jit->binary;
    void* jit_rodata = jit->binary + code_size;

    // initialize the jit as required
    if (code_size != 0) memset(jit_code, 0xCC, code_size);
    if (rodata_size != 0) memset(jit_rodata, 0x00, rodata_size);

    // Record the segment bounds and reserve the per-function layout array
    // up front so the debug ELF emitter can use them without recomputing
    // from page counts. Both are skipped entirely when debug capture is off.
    if (capture_debug) {
        jit->debug.code_base = jit_code;
        jit->debug.code_size = code_size_orig;
        jit->debug.rodata_base = jit_rodata;
        jit->debug.rodata_size = rodata_size_orig;
        if (functions.length != 0) {
            jit->debug.funcs = CALLOC(wasm_jit_func_layout_t, functions.length);
            CHECK(jit->debug.funcs != nullptr);
        }
    }

    // now go over all the blobs
    // and apply all the relocs
    for (int i = 0; i < functions.length; i++) {
        spidir_function_t func = functions.elements[i];
        spidir_codegen_blob_handle_t blob = blobs.elements[i];

        // get the offsets
        code_map_entry_t entry = {};
        CHECK(hmap_lookup(&code_map, func.id, &entry.value));
        size_t func_code_offset = entry.code_offset;
        size_t const_pool_offset = entry.rodata_offset;

        //
        // copy the blobs
        //

        size_t blob_code_size = spidir_codegen_blob_get_code_size(blob);
        size_t blob_const_size = spidir_codegen_blob_get_constpool_size(blob);

        if (blob_code_size != 0) {
            memcpy(
                jit_code + func_code_offset,
                spidir_codegen_blob_get_code(blob),
                blob_code_size
            );
        }

        if (blob_const_size != 0) {
            memcpy(
                jit_rodata + const_pool_offset,
                spidir_codegen_blob_get_constpool(blob),
                blob_const_size
            );
        }

        // Record the layout for the debug ELF. Owner funcidx comes from
        // func_to_idx (only populated when capture is on); skip the whole
        // dance when nobody asked for debug info.
        uint64_t owner_funcidx = UINT64_MAX;
        if (capture_debug) {
            CHECK(hmap_lookup(&func_to_idx, func.id, &owner_funcidx));

            wasm_jit_func_layout_t* layout = &jit->debug.funcs[jit->debug.funcs_count++];
            layout->funcidx = (uint32_t)owner_funcidx;
            layout->address = jit_code + func_code_offset;
            layout->code_size = blob_code_size;
            layout->const_address = blob_const_size != 0 ? jit_rodata + const_pool_offset : nullptr;
            layout->const_size = blob_const_size;
        }

        // go over the relocations of the function
        const spidir_codegen_reloc_t* relocs = spidir_codegen_blob_get_relocs(blob);
        for (int j = 0; j < spidir_codegen_blob_get_reloc_count(blob); j++) {
            const spidir_codegen_reloc_t* reloc = &relocs[j];

            // Resolve the target. The debug-only fields stay UINT32_MAX /
            // unset when capture is off so we don't pay for hmap lookups
            // we'll never read. target_kind is captured verbatim from spidir
            // (see wasm_jit_reloc_t), so we only compute the supplementary
            // wasm-level fields here.
            void* target = nullptr;
            uint32_t dbg_target_funcidx = UINT32_MAX;
            spidir_libcall_kind_t dbg_target_libcall = 0;

            switch (reloc->target_kind) {
                case SPIDIR_RELOC_TARGET_CONSTPOOL: {
                    target = jit_rodata + const_pool_offset;
                } break;

                case SPIDIR_RELOC_TARGET_GLOBAL: {
                    uint64_t rodata_offset;
                    CHECK(hmap_lookup(&global_map, reloc->target.global.id, &rodata_offset));
                    target = jit_rodata + rodata_offset;
                } break;

                case SPIDIR_RELOC_TARGET_INTERNAL_FUNCTION: {
                    // get the callee entry
                    code_map_entry_t callee_entry = {};
                    CHECK(hmap_lookup(&code_map, reloc->target.internal.id, &callee_entry.value));
                    target = jit_code + callee_entry.code_offset;

                    // we currently expect local functions to only ever have direct accesses
                    // indirect functions only exist as part of tables which are initialized
                    // in a different place
                    // NOTE: this assumes a direct call won't have a 64bit constant, which is
                    //       true without APX
                    CHECK(reloc->kind == SPIDIR_RELOC_X64_PC32);

                    if (capture_debug) {
                        uint64_t callee_funcidx = UINT64_MAX;
                        CHECK(hmap_lookup(&func_to_idx, reloc->target.internal.id, &callee_funcidx));
                        dbg_target_funcidx = (uint32_t)callee_funcidx;
                    }
                } break;

                case SPIDIR_RELOC_TARGET_EXTERNAL_FUNCTION: {
                    // Externals are either a JIT helper or a wasm import;
                    // both come through this target kind.
                    target = jit_helper_lookup_address(ctx, reloc->target.external.id);
                    if (target == nullptr) {
                        // try to look at imports
                        uint64_t addr;
                        CHECK(hmap_lookup(&import_map, reloc->target.external.id, &addr));
                        target = (void*)addr;
                    }

                    if (capture_debug) {
                        // Imports round-trip back to a wasm-level funcidx;
                        // helpers don't, so they stay UINT32_MAX and the debug
                        // ELF names them from their host address instead.
                        uint64_t import_funcidx = UINT64_MAX;
                        if (hmap_lookup(&extern_to_idx, reloc->target.external.id, &import_funcidx)) {
                            dbg_target_funcidx = (uint32_t)import_funcidx;
                        }
                    }
                } break;

                case SPIDIR_RELOC_TARGET_LIBCALL: {
                    target = jit_resolve_libcall(reloc->target.libcall);
                    CHECK(target != NULL);
                    if (capture_debug) {
                        dbg_target_libcall = reloc->target.libcall;
                    }
                } break;

                default:
                    CHECK_FAIL();
            }

            // actually apply the reloc
            RETHROW(jit_apply_reloc(
                jit_code + func_code_offset,
                blob_code_size,
                reloc,
                target
            ));

            // Record the reloc for the debug ELF. Kind and target_kind are
            // stored as spidir's own types (the ELF emitter only emits ELF
            // relocations for ABS64, but PC32 entries help label call sites if
            // we ever surface them).
            if (capture_debug) {
                wasm_jit_reloc_t entry = {
                    .address = jit_code + func_code_offset + reloc->offset,
                    .addend = reloc->addend,
                    .kind = reloc->kind,
                    .target_kind = reloc->target_kind,
                    .target_funcidx = dbg_target_funcidx,
                    .target_libcall = dbg_target_libcall,
                    .target_address = target,
                    .owner_funcidx = (uint32_t)owner_funcidx,
                };
                vec_push(&debug_relocs, entry);
            }
        }
    }

    //
    // Fill in the tables in the rodata, we do that from the element segment, we need
    // to do that after all the functions were created and before we lock the RX region,
    // because this will add endbrs to all the functions that need it
    //
    for (int64_t i = 0; i < ctx->module->elems_count; i++) {
        wasm_elem_segment_t* elem = &ctx->module->elems[i];
        CHECK(elem->tableidx < ctx->module->tables_count);
        jit_table_t* table = &ctx->tables[elem->tableidx];
        if (!table->used) {
            continue;
        }
        
        // get the rodata offset from the global
        uint64_t rodata_offset = -1;
        CHECK(hmap_lookup(&global_map, table->global.id, &rodata_offset));

        // segment must fit within the table's reserved range
        size_t end_slot;
        CHECK(!__builtin_add_overflow((size_t)elem->offset, (size_t)elem->funcs_count, &end_slot));
        CHECK(end_slot <= table->length);

        // lay out all of the functions in the elements
        void** slots = jit_rodata + rodata_offset;
        for (int64_t j = 0; j < elem->funcs_count; j++) {
            RETHROW(jit_get_function_addr(
                elem->funcs[j], ctx, 
                jit_code, &code_map, 
                &slots[elem->offset + j]
            ));
        }
    }

    // Hand the captured reloc list off to the JIT result. We move the buffer
    // rather than copy it to keep this hot path allocation-light. When debug
    // capture is off, debug_relocs is empty and this is just a couple of
    // null assignments.
    if (capture_debug) {
        jit->debug.relocs = debug_relocs.elements;
        jit->debug.relocs_count = debug_relocs.length;
        debug_relocs.elements = nullptr;
        debug_relocs.length = 0;
        debug_relocs.capacity = 0;
    }

    // fill the export table, this will also emit the endbr64
    jit->exports = CALLOC(wasm_jit_export_t, ctx->module->exports_count);
    CHECK(jit->exports != nullptr);

    for (int i = 0; i < ctx->module->exports_count; i++) {
        wasm_export_type_t kind = ctx->module->exports[i].kind;
        uint32_t index = ctx->module->exports[i].index;

        if (kind == WASM_EXPORT_FUNC) {
            RETHROW(jit_get_function_addr(index, ctx, jit_code, &code_map, &jit->exports[i].func.address));

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
        RETHROW(jit_get_function_addr(ctx->module->start_func, ctx, jit_code, &code_map, &entry));
        jit->start_func = entry;
    }

    // we are finished with jitting, we can lock the region and
    // let everything use it
    CHECK(wasm_host_jit_lock(jit->binary, jit->rx_page_count, jit->ro_page_count));

    // build the runtime state initializer
    RETHROW(jit_build_state_init(ctx, jit));

cleanup:
    // free the global stuff
    if (IS_ERROR(err)) {
        wasm_module_jit_free(jit);
    }

    // free the local stuff
    for (int i = 0; i < blobs.length; i++) {
        spidir_codegen_blob_destroy(blobs.elements[i]);
    }
    vec_free(&blobs);
    vec_free(&functions);
    vec_free(&debug_relocs);
    hmap_free(&code_map);
    hmap_free(&global_map);
    hmap_free(&import_map);
    hmap_free(&func_to_idx);
    hmap_free(&extern_to_idx);

    return err;
}

static wasm_err_t jit_prepare_state(jit_context_t* ctx, wasm_module_jit_t* jit) {
    wasm_err_t err = WASM_NO_ERROR;

    size_t offset = 0;

    if (ctx->module->globals_count != 0) {
        ctx->globals = CALLOC(jit_global_t, ctx->module->globals_count);
        CHECK(ctx->globals != nullptr);
        for (int64_t i = 0; i < ctx->module->globals_count; i++) {
            wasm_global_t* global = &ctx->module->globals[i];
            spidir_value_type_t type = jit_get_spidir_value_type(global->value.kind);
            ctx->globals[i].type = type;
            if (global->mutable) {
                // mutable, we need space for it
                offset = ALIGN_UP(offset, jit_get_spidir_size(type));
                ctx->globals[i].offset = offset;
                offset += jit_get_spidir_size(type);
            } else {
                // immutable, not going to be allocated, mark it as such
                ctx->globals[i].offset = -1;
            }
        }
    }

    //
    // Layout passive data
    //

    if (ctx->module->data_count != 0) {
        ctx->data = CALLOC(jit_data_t, ctx->module->data_count);
        CHECK(ctx->data != nullptr);
        for (int64_t i = 0; i < ctx->module->data_count; i++) {
            wasm_data_t* data = &ctx->module->data[i];
            if (!data->active) {
                // passive segments get a single void* entry
                offset = ALIGN_UP(offset, sizeof(void*));
                ctx->data[i].offset = offset;
                offset += sizeof(void*);
            } else {
                // active segments don't get an offset
                ctx->data[i].offset = -1;
            }
        }
    }

    jit->state_size = offset;

cleanup:
    return err;
}


wasm_err_t wasm_module_jit(wasm_module_t* module, wasm_module_jit_t* jit, wasm_jit_config_t* config) {
    wasm_err_t err = WASM_NO_ERROR;
    // use a default config when one is not provided
    if (config == nullptr) {
        static wasm_jit_config_t default_config = {
            .optimize = true
        };
        config = &default_config;
    }

    jit_context_t ctx = {
        .module = module,
        .config = config,
    };

    // it should be cheap enough to allocate it linearly
    ctx.functions = CALLOC(jit_function_t, module->functions_count + module->imports_count);
    CHECK(ctx.functions != nullptr);

    ctx.tables = CALLOC(jit_table_t, module->tables_count);
    CHECK(ctx.tables != nullptr);

    // setup the runtime state buffer (globals + tables)
    RETHROW(jit_prepare_state(&ctx, jit));

    ctx.spidir = spidir_module_create();

    // emit the spidir IR
    RETHROW(jit_emit_spidir(&ctx));

    // optimize the module
    if (config->optimize) {
        spidir_opt_run(ctx.spidir);
    }

    // dump the module if we need to
    if (config->dump_callback != nullptr) {
        spidir_module_dump(ctx.spidir, config->dump_callback, config->dump_arg);
    }

    // generate the entire thing
    RETHROW(jit_emit_code(&ctx, jit, config));

cleanup:
    if (ctx.spidir != nullptr) {
        spidir_module_destroy(ctx.spidir);
    }
    wasm_host_free(ctx.functions);
    wasm_host_free(ctx.globals);
    wasm_host_free(ctx.tables);
    wasm_host_free(ctx.data);
    vec_free(&ctx.queue);

    return err;
}
