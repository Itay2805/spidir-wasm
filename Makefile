########################################################################################################################
# Wasm
########################################################################################################################

# Nuke built-in rules and variables.
override MAKEFLAGS += -rR

#-----------------------------------------------------------------------------------------------------------------------
# General Config
#-----------------------------------------------------------------------------------------------------------------------

# Additional cflags passed from the user
CFLAGS 			?=

# Build the host tool
HOST 			?= y

#-----------------------------------------------------------------------------------------------------------------------
# General build flags
#-----------------------------------------------------------------------------------------------------------------------

CC := clang
AS := clang
LD := clang
AR := llvm-ar

CLANG_RESOURCE_DIR := $(shell $(CC) --print-resource-dir)

BUILD := build
OBJ := $(BUILD)/obj

#-----------------------------------------------------------------------------------------------------------------------
# Targets
#-----------------------------------------------------------------------------------------------------------------------

include scripts/defs.mk

# All the errors
cflags-y += -Wall -Werror
cflags-y += -Wno-unused-label

# Use C23
cflags-y += -std=gnu23

# Hidden by default, exports will be set as external
cflags-y += -fvisibility=hidden

# Add debug symbols
cflags-y += -g

# If we are compiling the host also add sanitizers
cflags-$(HOST) += -fsanitize=undefined,address
ldflags-$(HOST) += -fsanitize=undefined,address

ldflags-y += -fuse-ld=lld

PHONY += all
all:

quiet_cmd_clean = CLEAN   $(BUILD)
      cmd_clean = rm -rf $(BUILD)

PHONY += clean
clean:
	$(call cmd,clean)

include libs/spidir.mk
include src/Makefile
include scripts/build.mk
