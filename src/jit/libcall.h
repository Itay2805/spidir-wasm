#pragma once

#include "spidir/codegen.h"
#include <spidir/module.h>

/**
 * Resolve the host-side implementation address for a spidir backend libcall.
 * Returns nullptr if the libcall kind is not one we provide.
 */
void* jit_resolve_libcall(spidir_libcall_kind_t kind);

/**
 * Human-readable name for a libcall kind, used by the debug ELF to label the
 * symbol it synthesizes for the libcall target. Returns nullptr for unknown
 * kinds.
 */
const char* jit_get_libcall_name(spidir_libcall_kind_t kind);
