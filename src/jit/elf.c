#include "elf.h"

#include "util/defs.h"
#include "util/elf64.h"
#include "util/except.h"
#include "util/string.h"
#include "wasm/host.h"

#include <stdint.h>
#include <stddef.h>

// Section indices (file-local layout). Keep in sync with the writer below.
enum {
    SEC_NULL = 0,
    SEC_TEXT,
    SEC_RODATA,
    SEC_SYMTAB,
    SEC_STRTAB,
    SEC_SHSTRTAB,
    SEC_COUNT,
};

// Tiny string-table builder used for both .strtab and .shstrtab. We grow the
// buffer geometrically and return offsets into it.
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} strtab_t;

static wasm_err_t strtab_init(strtab_t* st) {
    wasm_err_t err = WASM_NO_ERROR;
    st->capacity = 64;
    st->data = wasm_host_calloc(1, st->capacity);
    CHECK(st->data != nullptr);
    // ELF strtabs always start with a NUL byte at offset 0.
    st->length = 1;
cleanup:
    return err;
}

static wasm_err_t strtab_add(strtab_t* st, const char* s, uint32_t* out_offset) {
    wasm_err_t err = WASM_NO_ERROR;

    size_t len = 0;
    while (s[len] != '\0') len++;

    size_t needed = st->length + len + 1;
    if (needed > st->capacity) {
        size_t new_cap = st->capacity * 2;
        while (new_cap < needed) new_cap *= 2;
        char* new_data = wasm_host_realloc(st->data, new_cap);
        CHECK(new_data != nullptr);
        st->data = new_data;
        // zero the freshly-allocated tail; realloc doesn't.
        memset(st->data + st->capacity, 0, new_cap - st->capacity);
        st->capacity = new_cap;
    }

    *out_offset = (uint32_t)st->length;
    memcpy(st->data + st->length, s, len + 1);
    st->length += len + 1;

cleanup:
    return err;
}

static void strtab_free(strtab_t* st) {
    wasm_host_free(st->data);
    st->data = nullptr;
    st->length = 0;
    st->capacity = 0;
}

wasm_err_t jit_emit_elf(const jit_elf_input_t* input, void** out_buffer, size_t* out_size) {
    wasm_err_t err = WASM_NO_ERROR;
    void* buffer = nullptr;

    strtab_t shstrtab = {};
    strtab_t strtab = {};
    Elf64_Sym* symbols = nullptr;
    size_t sym_count = 0;

    RETHROW(strtab_init(&shstrtab));
    RETHROW(strtab_init(&strtab));

    // Section name offsets.
    uint32_t shn_text = 0, shn_rodata = 0, shn_symtab = 0, shn_strtab = 0, shn_shstrtab = 0;
    RETHROW(strtab_add(&shstrtab, ".text", &shn_text));
    RETHROW(strtab_add(&shstrtab, ".rodata", &shn_rodata));
    RETHROW(strtab_add(&shstrtab, ".symtab", &shn_symtab));
    RETHROW(strtab_add(&shstrtab, ".strtab", &shn_strtab));
    RETHROW(strtab_add(&shstrtab, ".shstrtab", &shn_shstrtab));

    // Build the symbol table: NULL + one per function.
    sym_count = 1 + input->funcs_count;
    symbols = wasm_host_calloc(sym_count, sizeof(Elf64_Sym));
    CHECK(symbols != nullptr);

    size_t s = 1; // index 0 is the NULL symbol (already zeroed)
    for (size_t i = 0; i < input->funcs_count; i++) {
        uint32_t name_off = 0;
        if (input->funcs[i].name != nullptr) {
            RETHROW(strtab_add(&strtab, input->funcs[i].name, &name_off));
        } else {
            char name[32];
            wasm_host_snprintf(name, sizeof(name), "func%u", input->funcs[i].wasm_funcidx);
            RETHROW(strtab_add(&strtab, name, &name_off));
        }

        symbols[s++] = (Elf64_Sym){
            .st_name = name_off,
            .st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC),
            .st_other = 0,
            .st_shndx = SEC_TEXT,
            .st_value = input->funcs[i].offset,
            .st_size = input->funcs[i].size,
        };
    }

    //
    // File layout:
    //   [0]                ELF header
    //   [text_offset]      .text bytes (only when include_content)
    //   [rodata_offset]    .rodata bytes (only when include_content)
    //   [symtab_offset]    .symtab entries
    //   [strtab_offset]    .strtab bytes
    //   [shstrtab_offset]  .shstrtab bytes
    //   [shoff]            section headers
    //

    size_t off = sizeof(Elf64_Ehdr);

    off = ALIGN_UP(off, 16);
    size_t text_offset = off;
    off += input->code_size;

    off = ALIGN_UP(off, 16);
    size_t rodata_offset = off;
    off += input->rodata_size;

    off = ALIGN_UP(off, 8);
    size_t symtab_offset = off;
    size_t symtab_size = sym_count * sizeof(Elf64_Sym);
    off += symtab_size;

    size_t strtab_offset = off;
    off += strtab.length;

    size_t shstrtab_offset = off;
    off += shstrtab.length;

    off = ALIGN_UP(off, 8);
    size_t shoff = off;
    size_t shsize = SEC_COUNT * sizeof(Elf64_Shdr);
    size_t total_size = off + shsize;

    buffer = wasm_host_calloc(1, total_size);
    CHECK(buffer != nullptr);

    // ELF header
    Elf64_Ehdr* ehdr = buffer;
    ehdr->e_ident[EI_MAG0] = ELFMAG0;
    ehdr->e_ident[EI_MAG1] = ELFMAG1;
    ehdr->e_ident[EI_MAG2] = ELFMAG2;
    ehdr->e_ident[EI_MAG3] = ELFMAG3;
    ehdr->e_ident[EI_CLASS] = ELFCLASS64;
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr->e_type = ET_REL;
    ehdr->e_machine = EM_X86_64;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_shentsize = sizeof(Elf64_Shdr);
    ehdr->e_shnum = SEC_COUNT;
    ehdr->e_shstrndx = SEC_SHSTRTAB;
    ehdr->e_shoff = shoff;

    // Copy section payloads.
    if (input->code_size != 0) {
        memcpy(buffer + text_offset, input->code_addr, input->code_size);
    }
    if (input->rodata_size != 0) {
        memcpy(buffer + rodata_offset, input->rodata_addr, input->rodata_size);
    }
    memcpy(buffer + symtab_offset, symbols, symtab_size);
    memcpy(buffer + strtab_offset, strtab.data, strtab.length);
    memcpy(buffer + shstrtab_offset, shstrtab.data, shstrtab.length);

    // Section headers
    Elf64_Shdr* shdrs = (Elf64_Shdr*)(buffer + shoff);

    // [0] NULL - already zero.

    // [1] .text - sh_addr is the runtime address so GDB and IDA report
    //      addresses that match what's actually executing. When we omit the
    //      content we keep sh_size at the real size so address ranges match;
    //      sh_offset is set to past-EOF (well, to text_offset which has no
    //      backing bytes), but with no readers actually trying to read the
    //      bytes (GDB uses live memory) this is fine.
    shdrs[SEC_TEXT] = (Elf64_Shdr){
        .sh_name = shn_text,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_addr = (uint64_t)(uintptr_t)input->code_addr,
        .sh_offset = text_offset,
        .sh_size = input->code_size,
        .sh_addralign = 16,
    };

    // [2] .rodata
    shdrs[SEC_RODATA] = (Elf64_Shdr){
        .sh_name = shn_rodata,
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC,
        .sh_addr = (uint64_t)(uintptr_t)input->rodata_addr,
        .sh_offset = rodata_offset,
        .sh_size = input->rodata_size,
        .sh_addralign = 16,
    };

    // [3] .symtab - every non-NULL symbol is global, so sh_info (the index of
    //      the first non-local symbol) is just 1.
    shdrs[SEC_SYMTAB] = (Elf64_Shdr){
        .sh_name = shn_symtab,
        .sh_type = SHT_SYMTAB,
        .sh_flags = 0,
        .sh_addr = 0,
        .sh_offset = symtab_offset,
        .sh_size = symtab_size,
        .sh_link = SEC_STRTAB,
        .sh_info = 1,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Sym),
    };

    // [4] .strtab
    shdrs[SEC_STRTAB] = (Elf64_Shdr){
        .sh_name = shn_strtab,
        .sh_type = SHT_STRTAB,
        .sh_offset = strtab_offset,
        .sh_size = strtab.length,
        .sh_addralign = 1,
    };

    // [5] .shstrtab
    shdrs[SEC_SHSTRTAB] = (Elf64_Shdr){
        .sh_name = shn_shstrtab,
        .sh_type = SHT_STRTAB,
        .sh_offset = shstrtab_offset,
        .sh_size = shstrtab.length,
        .sh_addralign = 1,
    };

    *out_buffer = buffer;
    *out_size = total_size;
    buffer = nullptr;

cleanup:
    wasm_host_free(buffer);
    wasm_host_free(symbols);
    strtab_free(&strtab);
    strtab_free(&shstrtab);
    return err;
}
