libs-y += libwasm

libwasm-y += src/util/vec.c
libwasm-y += src/buffer.c

# Don't depend on any system header
#cflags-libwasm-y += -nostdinc
#cflags-libwasm-y += -isystem $(CLANG_RESOURCE_DIR)/include
cflags-libwasm-y += -Iinclude -Isrc
cflags-libwasm-y += -Ilibs/spidir/c-api/include

# Append the cflags from outside
cflags-libwasm-y += $(CFLAGS)
