########################################################################################################################
# Spidir
########################################################################################################################

#-----------------------------------------------------------------------------------------------------------------------
# Spidir Config
#-----------------------------------------------------------------------------------------------------------------------

# Enable spidir logs to be captured via logging API
SPIDIR_ENABLE_LOGGING 	?= n

# Override target passed to cargo invocations
SPIDIR_CARGO_TARGET 	?= x86_64-unknown-linux-none

# Additional flags to pass to cargo
SPIDIR_CARGO_FLAGS 		?= "-Zbuild-std=core,alloc"

# Override rust toolchain used to build spidir
SPIDIR_RUSTUP_TOOLCHAIN ?= nightly-2025-10-09

#-----------------------------------------------------------------------------------------------------------------------
# Spidir build setup
#-----------------------------------------------------------------------------------------------------------------------

SPIDIR_DIR := libs/spidir/c-api

ifeq ($(SPIDIR_ENABLE_LOGGING),y)
spidir-variant := logged
spidir-features :=
else
spidir-variant := unlogged
spidir-features := --features no_logging
endif

SPIDIR_TARGET_DIR := $(BUILD)/spidir/$(spidir-variant)/cargo-target

ifneq ($(SPIDIR_CARGO_TARGET),)
spidir-cargo-target := --target $(SPIDIR_CARGO_TARGET)
SPIDIR_LIB_SRC := $(SPIDIR_TARGET_DIR)/$(SPIDIR_CARGO_TARGET)/release/libspidir.a
else
spidir-cargo-target :=
SPIDIR_LIB_SRC := $(SPIDIR_TARGET_DIR)/release/libspidir.a
endif

ifneq ($(SPIDIR_RUSTUP_TOOLCHAIN),)
spidir-toolchain := +$(SPIDIR_RUSTUP_TOOLCHAIN)
else
spidir-toolchain :=
endif

# The lib output, named so consumers can pull it in via `ldbuiltlibs-<bin>-y += spidir`.
SPIDIR_LIB := $(OBJ)/libspidir.a
targets += $(SPIDIR_LIB)

quiet_cmd_cargo_spidir = CARGO   $(SPIDIR_LIB)
      cmd_cargo_spidir = cd $(SPIDIR_DIR) && \
                         cargo $(spidir-toolchain) build --release -p c-api \
                             $(spidir-features) $(spidir-cargo-target) $(SPIDIR_CARGO_FLAGS) \
                             --target-dir $(abspath $(SPIDIR_TARGET_DIR)) && \
                         mkdir -p $(abspath $(OBJ)) && \
                         cp -p $(abspath $(SPIDIR_LIB_SRC)) $(abspath $(SPIDIR_LIB))

# Always invoke cargo so it can manage its own incremental builds. cp -p keeps
# the destination's mtime aligned with the cargo output so downstream consumers
# only relink when cargo actually rebuilt the archive.
PHONY += $(SPIDIR_LIB)
$(SPIDIR_LIB):
	$(call cmd,cargo_spidir)
