libs-y += libwasm

libwasm-y += src/jit/codegen.c
libwasm-y += src/jit/debug_elf.c
libwasm-y += src/jit/function.c
libwasm-y += src/jit/helpers.c
libwasm-y += src/jit/inst.c
libwasm-y += src/jit/jit.c
libwasm-y += src/jit/libcall.c
libwasm-y += src/util/hmap.c
libwasm-y += src/util/string.c
libwasm-y += src/util/vec.c
libwasm-y += src/buffer.c
libwasm-y += src/module.c

# Don't depend on any system header
cflags-libwasm-y += -nostdinc
cflags-libwasm-y += -isystem $(CLANG_RESOURCE_DIR)/include
cflags-libwasm-y += -Iinclude -Isrc
cflags-libwasm-y += -Ilibs/spidir/c-api/include

# Append the cflags from outside
cflags-libwasm-y += $(CFLAGS)
