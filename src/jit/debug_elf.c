#include "wasm/debug_elf.h"

#include "buffer.h"
#include "jit/helpers.h"
#include "util/defs.h"
#include "util/elf_common.h"
#include "util/except.h"
#include "util/elf64.h"
#include "util/vec.h"
#include "wasm/host.h"
#include "wasm/jit.h"

#include <stdint.h>

// --------------------------------------------------------------------------
// String-table builder
// --------------------------------------------------------------------------

// A wasm `name` section may contain identifiers a debugger / disassembler
// can't render: NUL bytes, control codes, etc. We sanitize defensively so a
// bad name section doesn't produce an unreadable ELF (it would still be
// parseable, just visually noisy). NULL/empty names fall back to a synthetic
// label upstream — this helper only ever sees a non-empty name to encode.
static wasm_err_t strtab_emit_str(buffer_t* strtab, const char* str, uint32_t* out_off) {
    wasm_err_t err = WASM_NO_ERROR;
    *out_off = (uint32_t)strtab->len;

    // We emit byte-by-byte so we can rewrite anything outside the printable
    // ASCII range as '?'. ELF strings are NUL-terminated so we always tack on
    // a trailing 0.
    for (const char* p = str; *p != '\0'; p++) {
        uint8_t b = (uint8_t)*p;
        if (b < 0x20 || b == 0x7f) b = '?';
        RETHROW(buffer_push(strtab, &b, 1));
    }
    uint8_t zero = 0;
    RETHROW(buffer_push(strtab, &zero, 1));

cleanup:
    return err;
}

// --------------------------------------------------------------------------
// Symbol-table layout
// --------------------------------------------------------------------------

// Per wasm-funcidx slot in the symbol table. UINT32_MAX means "we never
// produced a symbol for this funcidx" (e.g. an unused import). Used to
// translate relocations: a reloc whose target is funcidx N looks up
// sym_for_funcidx[N] to find the symbol index to put in the rela entry.
typedef struct func_sym_slot {
    uint32_t sym_index;
} func_sym_slot_t;

// Returns the friendliest name we can come up with for a wasm function.
// Priority:
//   1. wasm `name` custom section entry (debug names parsed at load time)
//   2. import.module_name "." item_name for an import
//   3. export name when the function is exported
//   4. a synthetic "func<idx>" label
//
// The returned buffer is allocated with wasm_host_calloc and is owned by the
// caller. We don't use the input strings directly because we want one stable
// place to apply naming policy (and the synthetic case has to allocate
// anyway).
static wasm_err_t make_func_label(
    const wasm_module_t* module,
    uint32_t funcidx,
    char** out
) {
    wasm_err_t err = WASM_NO_ERROR;
    *out = nullptr;

    // 1. debug name from the `name` section
    if (module->function_names != nullptr) {
        const char* dbg = module->function_names[funcidx];
        if (dbg != nullptr && dbg[0] != '\0') {
            size_t n = strlen(dbg);
            char* buf = wasm_host_calloc(1, n + 1);
            CHECK(buf != nullptr);
            memcpy(buf, dbg, n + 1);
            *out = buf;
            goto cleanup;
        }
    }

    // 2. import — fall back to the module-provided name pair. We join with
    // '.' since some debuggers split symbols on '@', '::', etc.
    if (funcidx < module->imports_count) {
        const wasm_import_t* imp = &module->imports[funcidx];
        size_t mn = strlen(imp->module_name);
        size_t in = strlen(imp->item_name);
        char* buf = wasm_host_calloc(1, mn + 1 + in + 1);
        CHECK(buf != nullptr);
        memcpy(buf, imp->module_name, mn);
        buf[mn] = '.';
        memcpy(buf + mn + 1, imp->item_name, in);
        buf[mn + 1 + in] = '\0';
        *out = buf;
        goto cleanup;
    }

    // 3. export — first export referencing this function wins. We don't
    // expose every export as the *primary* name (each export still gets its
    // own alias symbol below), but if there's no debug name, this is much
    // friendlier than `func<idx>`.
    for (uint32_t i = 0; i < module->exports_count; i++) {
        const wasm_export_t* exp = &module->exports[i];
        if (exp->kind == WASM_EXPORT_FUNC && exp->index == funcidx) {
            size_t n = strlen(exp->name);
            char* buf = wasm_host_calloc(1, n + 1);
            CHECK(buf != nullptr);
            memcpy(buf, exp->name, n + 1);
            *out = buf;
            goto cleanup;
        }
    }

    // 4. synthetic
    {
        char* buf = wasm_host_calloc(1, 32);
        CHECK(buf != nullptr);
        buf[0] = 'f';
        buf[1] = 'u';
        buf[2] = 'n';
        buf[3] = 'c';
        int digits = u64toa(funcidx, &buf[4]);
        buf[4 + digits] = '\0';
        *out = buf;
    }

cleanup:
    return err;
}

// --------------------------------------------------------------------------
// ELF emission
// --------------------------------------------------------------------------

wasm_err_t wasm_jit_emit_debug_elf(
    const wasm_module_t* module,
    const wasm_module_jit_t* jit,
    void** out_data,
    size_t* out_size
) {
    wasm_err_t err = WASM_NO_ERROR;

    *out_data = nullptr;
    *out_size = 0;

    typedef struct ext_sym_cache {
        void* addr;
        uint32_t sym_index;
    } ext_sym_cache_t;
    vec(ext_sym_cache_t) ext_cache = {};

    // Buffers we accumulate into and then concatenate.
    buffer_t shstrtab = {};
    buffer_t strtab = {};
    buffer_t symtab = {};
    buffer_t rela_text = {};
    buffer_t out = {};
    func_sym_slot_t* slots = nullptr;
    char* label = nullptr;

    const wasm_jit_debug_info_t* dbg = &jit->debug;

    // shstrtab[0] / strtab[0] must be a NUL byte (ELF convention: an unused
    // string-table entry has a name offset of 0, which must dereference to "").
    {
        uint8_t zero = 0;
        RETHROW(buffer_push(&shstrtab, &zero, 1));
        RETHROW(buffer_push(&strtab, &zero, 1));
    }

    // Reserve slot indices for the 7 sections we emit. .rodata and
    // .rela.text are conditionally present, so we compute the section table
    // shape up front rather than hand-coding indices throughout.
    bool have_rodata = dbg->rodata_size != 0;

    // Count absolute relocations — only those need ELF rela entries (the user
    // explicitly asked us to preserve absolute relocations). PC-relative
    // relocations resolve relative to the bytes themselves and remain valid
    // for any tool that reads the .text section as-is.
    uint32_t abs_reloc_count = 0;
    for (size_t i = 0; i < dbg->relocs_count; i++) {
        if (dbg->relocs[i].kind == WASM_JIT_RELOC_X64_ABS64) {
            abs_reloc_count++;
        }
    }
    bool have_rela = abs_reloc_count != 0;

    // Section layout — keep the names list parallel with the section table
    // ordering below. The indices feed sh_link / st_shndx fields.
    uint16_t shnum = 0;
    uint16_t shidx_null = shnum++;
    uint16_t shidx_text = shnum++;
    uint16_t shidx_rodata = have_rodata ? shnum++ : 0;
    uint16_t shidx_symtab = shnum++;
    uint16_t shidx_strtab = shnum++;
    uint16_t shidx_rela = have_rela ? shnum++ : 0;
    uint16_t shidx_shstrtab = shnum++;

    // Pre-stage section name offsets into shstrtab. The order doesn't matter
    // beyond keeping the indices straight; we only use these to fill sh_name
    // when we materialize the section header table.
    uint32_t name_text, name_symtab, name_strtab, name_shstrtab;
    uint32_t name_rodata = 0, name_rela = 0;
    RETHROW(strtab_emit_str(&shstrtab, ".text",     &name_text));
    if (have_rodata) {
        RETHROW(strtab_emit_str(&shstrtab, ".rodata", &name_rodata));
    }
    RETHROW(strtab_emit_str(&shstrtab, ".symtab",   &name_symtab));
    RETHROW(strtab_emit_str(&shstrtab, ".strtab",   &name_strtab));
    if (have_rela) {
        RETHROW(strtab_emit_str(&shstrtab, ".rela.text", &name_rela));
    }
    RETHROW(strtab_emit_str(&shstrtab, ".shstrtab", &name_shstrtab));

    // Symbol table layout:
    //   [0]            STN_UNDEF (required by ELF)
    //   [1] _module    optional module-name marker (handy for objdump)
    //   [2] .text      STT_SECTION (so rela entries can target the section)
    //   [3] .rodata    STT_SECTION (only if .rodata is present)
    //
    //   Then per-funcidx symbols: imports as SHN_ABS (host code is outside
    //   our text section — SHN_ABS gives the symbol a fixed value GDB can
    //   pretty-print without trying to chase a section-relative offset),
    //   internal functions as STT_FUNC pointing into .text.
    //
    //   Finally, alias symbols for exports — each export gets its own symbol
    //   so a debugger search for the exported name lands in the right place.
    //   We keep the funcidx-keyed slot for relocations and add the export
    //   alias separately so the two never compete.
    //
    // ELF requires local symbols to come before global ones in the table; the
    // st_info STB_LOCAL / STB_GLOBAL split goes through sh_info on the symtab
    // section (see fill below).

    // Helper to push a single symbol entry.
    #define PUSH_SYM(name_off, info, other, shndx, value, size) \
        do { \
            Elf64_Sym s__ = { \
                .st_name = (name_off), \
                .st_info = (info), \
                .st_other = (other), \
                .st_shndx = (shndx), \
                .st_value = (value), \
                .st_size = (size), \
            }; \
            RETHROW(buffer_push(&symtab, &s__, sizeof(s__))); \
        } while (0)

    // Index 0: required null symbol.
    PUSH_SYM(0, ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE), STV_DEFAULT, SHN_UNDEF, 0, 0);

    // Optional file-name marker symbol.
    if (module->module_name != nullptr) {
        uint32_t off;
        RETHROW(strtab_emit_str(&strtab, module->module_name, &off));
        PUSH_SYM(off, ELF64_ST_INFO(STB_LOCAL, STT_OBJECT), 0, SHN_ABS, 0, 0);
    }

    // Section-typed locals first (so sh_info can split locals from globals
    // cleanly). These let .rela.text reference the section symbol directly,
    // which is the standard pattern when an absolute reloc lands inside .text
    // but doesn't correspond to a single named function (e.g. a const-pool
    // pointer into .rodata).
    // SECTION symbols carry st_name = 0 by convention — readers display them
    // using the section's own name (.text / .rodata) which lives in shstrtab,
    // not strtab. Putting a strtab offset here would print a garbage name.
    PUSH_SYM(0, ELF64_ST_INFO(STB_LOCAL, STT_SECTION), STV_DEFAULT,
             shidx_text, (uint64_t)dbg->code_base, 0);

    uint32_t sym_rodata_section = 0;
    if (have_rodata) {
        sym_rodata_section = (uint32_t)(symtab.len / sizeof(Elf64_Sym));
        PUSH_SYM(0, ELF64_ST_INFO(STB_LOCAL, STT_SECTION), STV_DEFAULT,
                 shidx_rodata, (uint64_t)dbg->rodata_base, 0);
    }

    // Per-funcidx slot map: maps wasm funcidx → symbol-table index. Filled
    // below as we materialize each function symbol; UINT32_MAX means "no
    // symbol emitted" (which can happen for imports the JIT never resolved
    // and for internal funcs that weren't included in codegen).
    size_t total_funcs = (size_t)module->imports_count + module->functions_count;
    if (total_funcs != 0) {
        slots = wasm_host_calloc(total_funcs, sizeof(func_sym_slot_t));
        CHECK(slots != nullptr);
        for (size_t i = 0; i < total_funcs; i++) slots[i].sym_index = UINT32_MAX;
    }

    // sh_info on the symtab is "one greater than the symbol table index of
    // the last local symbol". We push imports as STB_GLOBAL too (their value
    // is meaningful and we want them visible across the symbol space), so the
    // boundary is right after the section symbols / module marker.
    uint32_t first_global = (uint32_t)(symtab.len / sizeof(Elf64_Sym));

    // Imports: SHN_ABS, value = host code address. STT_FUNC so debuggers know
    // to symbolize calls into them. We always emit a symbol for every import,
    // even if no relocation references it — they're observably present in the
    // wasm module and a user inspecting the ELF would expect to see them.
    for (uint32_t i = 0; i < module->imports_count; i++) {
        if (module->imports[i].kind != WASM_EXTERN_FUNC) continue;

        wasm_host_free(label);
        label = nullptr;
        RETHROW(make_func_label(module, i, &label));

        uint32_t off;
        RETHROW(strtab_emit_str(&strtab, label, &off));

        // Walk the JIT debug.relocs to find the resolved host address for
        // this import. Imports without a recorded relocation simply weren't
        // referenced by any compiled function — emit them with value 0 so
        // the symbol still exists for tooling.
        uint64_t addr = 0;
        for (size_t r = 0; r < dbg->relocs_count; r++) {
            if (dbg->relocs[r].target_kind == WASM_JIT_RELOC_TARGET_FUNC &&
                dbg->relocs[r].target_funcidx == i) {
                addr = (uint64_t)(uintptr_t)dbg->relocs[r].target_address;
                break;
            }
        }

        slots[i].sym_index = (uint32_t)(symtab.len / sizeof(Elf64_Sym));
        PUSH_SYM(off, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC), 0, SHN_UNDEF, addr, 0);
    }

    // Internal functions: STT_FUNC pointing at .text. Use the per-function
    // layout the JIT recorded so size is exact.
    for (size_t i = 0; i < dbg->funcs_count; i++) {
        const wasm_jit_func_layout_t* fl = &dbg->funcs[i];

        wasm_host_free(label);
        label = nullptr;
        RETHROW(make_func_label(module, fl->funcidx, &label));

        uint32_t off;
        RETHROW(strtab_emit_str(&strtab, label, &off));

        slots[fl->funcidx].sym_index = (uint32_t)(symtab.len / sizeof(Elf64_Sym));
        PUSH_SYM(off, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC), 0,
                 shidx_text, (uint64_t)fl->address, (uint64_t)fl->code_size);
    }

    // Per-export aliases: a separate symbol whose name is exactly the wasm
    // export name. We keep this distinct from the funcidx-primary symbol
    // because a function can be exported under a different name than its
    // debug name (or under multiple names — each gets its own alias).
    // Globals/memories are also surfaced so they're visible to anyone
    // inspecting the ELF, even though they don't carry executable code.
    for (uint32_t i = 0; i < module->exports_count; i++) {
        const wasm_export_t* exp = &module->exports[i];

        if (exp->kind == WASM_EXPORT_FUNC) {
            uint32_t funcidx = exp->index;
            if (funcidx >= total_funcs) continue;
            uint32_t prim = slots[funcidx].sym_index;
            if (prim == UINT32_MAX) continue;

            // Skip the alias when the export name is identical to the
            // primary symbol — this happens when the module has no debug
            // name and make_func_label has already adopted the export name.
            // Emitting both would just produce a duplicate row in `nm`.
            Elf64_Sym* primary = (Elf64_Sym*)((uint8_t*)symtab.data
                + (size_t)prim * sizeof(Elf64_Sym));
            const char* prim_name = (const char*)strtab.data + primary->st_name;
            if (strcmp(prim_name, exp->name) == 0) continue;

            uint32_t off;
            RETHROW(strtab_emit_str(&strtab, exp->name, &off));

            // Note: pushing into symtab can realloc strtab/symtab buffers, so
            // recompute `primary` from the (possibly moved) base.
            primary = (Elf64_Sym*)((uint8_t*)symtab.data
                + (size_t)prim * sizeof(Elf64_Sym));

            // Mirror the primary symbol's address/size so the alias lights up
            // for the same range. We use the live primary entry as the source
            // of truth (it already knows whether it's an import or internal).
            PUSH_SYM(off, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC), 0,
                     primary->st_shndx, primary->st_value, primary->st_size);
        } else if (exp->kind == WASM_EXPORT_GLOBAL) {
            // The exported global lives at jit->state_init's region, but
            // that buffer is host-side state, not part of our JIT binary —
            // we have nothing meaningful to point at, so skip.
            (void)0;
        }
        // memories/tables: nothing useful to surface here.
    }

    // Now emit the rela.text entries for absolute relocations. The bytes in
    // .text already contain the linked value, but having the rela entry lets
    // disassemblers display the symbolic target — which is the whole point
    // of the debug ELF. We only emit ABS64 entries (per the user
    // requirement); PC32 sites are already self-describing.
    //
    // Helper / external targets that have no corresponding wasm-level symbol
    // get an SHN_ABS symbol synthesized on demand so the rela entry can name
    // them. We dedupe by host address so the same helper called from many
    // places only adds one symbol.
    if (have_rela) {
        for (size_t i = 0; i < dbg->relocs_count; i++) {
            const wasm_jit_reloc_t* r = &dbg->relocs[i];
            if (r->kind != WASM_JIT_RELOC_X64_ABS64) continue;

            // Resolve the relocation's target to a symbol index. For a
            // function target we have the slot map; for a const-pool target
            // we fall back to the .rodata section symbol with an addend that
            // recovers the absolute offset within the section. External /
            // helper targets get a synthesized SHN_ABS symbol so their
            // address still shows up symbolically.
            uint32_t sym_index = 0;
            int64_t addend = r->addend;
            uint32_t reloc_type = R_X86_64_64;

            if (r->target_kind == WASM_JIT_RELOC_TARGET_FUNC) {
                if (r->target_funcidx < total_funcs &&
                    slots[r->target_funcidx].sym_index != UINT32_MAX) {
                    sym_index = slots[r->target_funcidx].sym_index;
                }
            } else if (r->target_kind == WASM_JIT_RELOC_TARGET_CONSTPOOL) {
                if (have_rodata) {
                    sym_index = sym_rodata_section;
                    // The addend should bring the section symbol up to the
                    // exact target byte. Section symbols use sh_addr as their
                    // value, so subtracting it from target_address yields
                    // the offset to add on top of the recorded addend.
                    addend += (int64_t)((uint64_t)(uintptr_t)r->target_address
                                        - (uint64_t)(uintptr_t)dbg->rodata_base);
                }
            } else {
                // External target with no funcidx (a helper). Look it up in
                // the dedupe cache; if we haven't synthesized a symbol yet,
                // do it now and remember the index.
                for (size_t c = 0; c < ext_cache.length; c++) {
                    if (ext_cache.elements[c].addr == r->target_address) {
                        sym_index = ext_cache.elements[c].sym_index;
                        break;
                    }
                }
                if (sym_index == 0) {
                    uint32_t name_off;
                    RETHROW(strtab_emit_str(&strtab, jit_get_helper_name(r->target_address), &name_off));

                    sym_index = (uint32_t)(symtab.len / sizeof(Elf64_Sym));
                    PUSH_SYM(name_off, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC), STV_DEFAULT, SHN_UNDEF, (uint64_t)r->target_address, 0);

                    vec_push(&ext_cache, (ext_sym_cache_t){
                        .addr = r->target_address,
                        .sym_index = sym_index,
                    });
                }
            }

            // r_offset for .rela.text is the byte offset *within* .text.
            // VAs would also resolve correctly here (sh_addr matches the
            // live VA), but section-relative is the convention every reader
            // expects, matches what `readelf -r` prints out of the box, and
            // stays correct if the section is ever rebased.
            Elf64_Rela entry = {
                .r_offset = (uint64_t)r->address,
                .r_info = ELF64_R_INFO((Elf64_Xword)sym_index, reloc_type),
                .r_addend = addend,
            };
            RETHROW(buffer_push(&rela_text, &entry, sizeof(entry)));
        }
    }
    vec_free(&ext_cache);

    #undef PUSH_SYM

    // --------------------------------------------------------------------
    // Now we have all the section payloads. Stitch them into a single ELF.
    // --------------------------------------------------------------------

    // Layout in the file:
    //   ehdr | text | rodata? | symtab | strtab | rela? | shstrtab | shdrs
    //
    // Section addresses (sh_addr) for .text/.rodata point at the live JIT
    // memory — same VAs the running code uses, so GDB JIT can correlate
    // backtrace IPs back to symbols without translation. The other sections
    // get sh_addr = 0 because they aren't loaded into the address space.

    Elf64_Ehdr ehdr = {};
    ehdr.e_ident[EI_MAG0]    = ELFMAG0;
    ehdr.e_ident[EI_MAG1]    = ELFMAG1;
    ehdr.e_ident[EI_MAG2]    = ELFMAG2;
    ehdr.e_ident[EI_MAG3]    = ELFMAG3;
    ehdr.e_ident[EI_CLASS]   = ELFCLASS64;
    ehdr.e_ident[EI_DATA]    = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI]   = 0;
    ehdr.e_type      = ET_DYN;
    ehdr.e_machine   = EM_X86_64;
    ehdr.e_version   = EV_CURRENT;
    ehdr.e_ehsize    = (uint16_t)sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum     = 0;
    ehdr.e_shentsize = (uint16_t)sizeof(Elf64_Shdr);
    ehdr.e_shnum     = shnum;
    ehdr.e_shstrndx  = shidx_shstrtab;

    // First pass: compute file offsets so the section header table can
    // reference them. We pad each section to its natural alignment so the
    // resulting file passes loader / readelf consistency checks.
    uint64_t off = sizeof(Elf64_Ehdr);
    uint64_t off_text = 0;
    uint64_t off_rodata = 0;
    uint64_t off_symtab, off_strtab, off_rela = 0, off_shstrtab;

    off = ALIGN_UP(off, 16);
    off_text = off;
    off += dbg->code_size;

    if (have_rodata) {
        off = ALIGN_UP(off, 16);
        off_rodata = off;
        off += dbg->rodata_size;
    }

    off = ALIGN_UP(off, 8);
    off_symtab = off;
    off += symtab.len;

    off_strtab = off;
    off += strtab.len;

    if (have_rela) {
        off = ALIGN_UP(off, 8);
        off_rela = off;
        off += rela_text.len;
    }

    off_shstrtab = off;
    off += shstrtab.len;

    off = ALIGN_UP(off, 8);
    ehdr.e_shoff = off;
    off += (uint64_t)shnum * sizeof(Elf64_Shdr);

    // Now allocate the buffer and fill it.
    out.data = wasm_host_calloc(1, off);
    CHECK(out.data != nullptr);
    out.len = off;

    memcpy(out.data, &ehdr, sizeof(ehdr));
    if (dbg->code_size != 0) {
        memcpy((uint8_t*)out.data + off_text, dbg->code_base, dbg->code_size);
    }
    if (have_rodata) {
        memcpy((uint8_t*)out.data + off_rodata, dbg->rodata_base, dbg->rodata_size);
    }
    if (symtab.len != 0) {
        memcpy((uint8_t*)out.data + off_symtab, symtab.data, symtab.len);
    }
    if (strtab.len != 0) {
        memcpy((uint8_t*)out.data + off_strtab, strtab.data, strtab.len);
    }
    if (have_rela && rela_text.len != 0) {
        memcpy((uint8_t*)out.data + off_rela, rela_text.data, rela_text.len);
    }
    if (shstrtab.len != 0) {
        memcpy((uint8_t*)out.data + off_shstrtab, shstrtab.data, shstrtab.len);
    }

    // Section header table.
    Elf64_Shdr* shdrs = (Elf64_Shdr*)((uint8_t*)out.data + ehdr.e_shoff);

    // [0] SHT_NULL — required.
    shdrs[shidx_null] = (Elf64_Shdr){};

    // [1] .text
    shdrs[shidx_text] = (Elf64_Shdr){
        .sh_name = name_text,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = (uint64_t)(uintptr_t)dbg->code_base,
        .sh_offset = off_text,
        .sh_size = dbg->code_size,
        .sh_addralign = 16,
    };

    // [2] .rodata (optional)
    if (have_rodata) {
        shdrs[shidx_rodata] = (Elf64_Shdr){
            .sh_name = name_rodata,
            .sh_type = SHT_PROGBITS,
            .sh_flags = SHF_ALLOC,
            .sh_addr = (uint64_t)(uintptr_t)dbg->rodata_base,
            .sh_offset = off_rodata,
            .sh_size = dbg->rodata_size,
            .sh_addralign = 16,
        };
    }

    // [n] .symtab
    shdrs[shidx_symtab] = (Elf64_Shdr){
        .sh_name = name_symtab,
        .sh_type = SHT_SYMTAB,
        .sh_flags = SHF_ALLOC,
        .sh_addr = 0,
        .sh_offset = off_symtab,
        .sh_size = symtab.len,
        .sh_link = shidx_strtab,
        .sh_info = first_global,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Sym),
    };

    // [n] .strtab
    shdrs[shidx_strtab] = (Elf64_Shdr){
        .sh_name = name_strtab,
        .sh_type = SHT_STRTAB,
        .sh_offset = off_strtab,
        .sh_size = strtab.len,
        .sh_addralign = 1,
    };

    // [n] .rela.text (optional)
    if (have_rela) {
        shdrs[shidx_rela] = (Elf64_Shdr){
            .sh_name = name_rela,
            .sh_type = SHT_RELA,
            .sh_flags = SHF_INFO_LINK,
            .sh_offset = off_rela,
            .sh_size = rela_text.len,
            .sh_link = shidx_symtab,
            .sh_info = shidx_text,
            .sh_addralign = 8,
            .sh_entsize = sizeof(Elf64_Rela),
        };
    }

    // [last] .shstrtab
    shdrs[shidx_shstrtab] = (Elf64_Shdr){
        .sh_name = name_shstrtab,
        .sh_type = SHT_STRTAB,
        .sh_offset = off_shstrtab,
        .sh_size = shstrtab.len,
        .sh_addralign = 1,
    };

    *out_data = out.data;
    *out_size = out.len;
    out.data = nullptr;
    out.len = 0;

cleanup:
    wasm_host_free(label);
    wasm_host_free(slots);
    wasm_host_free(shstrtab.data);
    wasm_host_free(strtab.data);
    wasm_host_free(symtab.data);
    wasm_host_free(rela_text.data);
    wasm_host_free(out.data);
    vec_free(&ext_cache);

    return err;
}
