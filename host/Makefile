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

# The fuzzer: a lean binary that only loads + JITs the input, so it links just
# the platform glue it needs (host_platform.c for allocation/atomics/jit-mapping,
# spidir_platform.c) plus libwasm/libspidir. It skips runtime.c/wasi.c/gdb_jit.c
# and instead stubs the two memory callbacks the JIT references (see fuzz.c).
# libFuzzer provides main, so host/main.c is intentionally absent.
bins-$(FUZZ) += fuzz

fuzz-y += host/fuzz.c
fuzz-y += host/spidir_platform.c
fuzz-y += host/host_platform.c

cflags-fuzz-y += -Iinclude
cflags-fuzz-y += -Ilibs/spidir/c-api/include
cflags-fuzz-y += -Isrc

ldbuiltlibs-fuzz-y += libwasm
ldbuiltlibs-fuzz-y += libspidir
