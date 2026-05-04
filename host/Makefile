bins-$(HOST) += main

main-y += host/main.c
main-y += host/spidir_platform.c

cflags-main-y += -Iinclude
cflags-main-y += -Ilibs/spidir/c-api/include
cflags-main-y += -Isrc

ldbuiltlibs-main-y += libwasm
ldbuiltlibs-main-y += libspidir
