#include "libcall.h"

#include <spidir/x64.h>

static uint64_t spidir_libcall_popcnt32(uint32_t value) { return __builtin_popcount(value); }
static uint64_t spidir_libcall_popcnt64(uint64_t value) { return __builtin_popcountll(value); }

void* jit_resolve_libcall(spidir_libcall_kind_t kind) {
    switch (kind) {
        case SPIDIR_LIBCALL_X64_POPCNT32: return spidir_libcall_popcnt32;
        case SPIDIR_LIBCALL_X64_POPCNT64: return spidir_libcall_popcnt64;
        default: return nullptr;
    }
}

const char* jit_get_libcall_name(spidir_libcall_kind_t kind) {
    switch (kind) {
        case SPIDIR_LIBCALL_X64_POPCNT32: return "__popcountsi2";
        case SPIDIR_LIBCALL_X64_POPCNT64: return "__popcountdi2";
        default: return nullptr;
    }
}
