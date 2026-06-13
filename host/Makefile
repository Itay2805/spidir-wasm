bins-$(HOST) += main

main-y += host/main.c
main-y += host/wasi.c
main-y += host/spidir_platform.c
main-y += host/runtime.c
main-y += host/host_platform.c
main-y += host/gdb_jit.c

cflags-main-y += -Iinclude
cflags-main-y += -Ilibs/spidir/c-api/include
cflags-main-y += -Isrc

ldbuiltlibs-main-y += libwasm
ldbuiltlibs-main-y += libspidir
