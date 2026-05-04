########################################################################################################################
# Wasm tests
########################################################################################################################

# Nuke built-in rules and variables.
override MAKEFLAGS += -rR

#-----------------------------------------------------------------------------------------------------------------------
# General build flags
#-----------------------------------------------------------------------------------------------------------------------

CC := clang
AS := clang
LD := clang
AR := llvm-ar

# Pinned wasi-sdk release — both the sysroot and the compiler-rt
# builtins archive are fetched from this release to keep the build
# reproducible and independent of the host distro's packaging.
WASI_SDK_VERSION := 32
WASI_SDK_URL_BASE := https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-$(WASI_SDK_VERSION)
WASI_SYSROOT_URL := $(WASI_SDK_URL_BASE)/wasi-sysroot-$(WASI_SDK_VERSION).0.tar.gz
WASI_RT_URL      := $(WASI_SDK_URL_BASE)/libclang_rt-$(WASI_SDK_VERSION).0.tar.gz

# wat2wasm (from wabt) is used to assemble .wat tests. Picked up from PATH.
WAT2WASM ?= wat2wasm

BUILD := build
ARTIFACTS := artifacts
OBJ := $(BUILD)/obj

WASI_SYSROOT := $(abspath $(ARTIFACTS)/wasi-sysroot)
WASI_SYSROOT_STAMP := $(WASI_SYSROOT)/lib/wasm32-wasi/libc.a

WASI_RT_DIR := $(ARTIFACTS)/wasi-rt
WASI_RT_LIB := $(WASI_RT_DIR)/lib/wasm32-unknown-wasi/libclang_rt.builtins.a

#-----------------------------------------------------------------------------------------------------------------------
# Targets
#-----------------------------------------------------------------------------------------------------------------------

include ../scripts/defs.mk

# Common cflags for all apps
cflags-y += --target=wasm32
cflags-y += -Wall -Werror -std=gnu23
cflags-y += -flto -g
cflags-y += -fvisibility=hidden

ldflags-y += --target=wasm32
ldflags-y += -resource-dir=$(abspath $(WASI_RT_DIR))
ldflags-y += -flto
ldflags-y += -nostdlib

cflags-$(OPTIMIZE) += -O3

cflags-$(DEBUG) += -D__DEBUG__
cflags-$(DEBUG) += -Wno-unused-function -Wno-unused-label -Wno-unused-variable

PHONY += all
all:

quiet_cmd_clean = CLEAN   $(BUILD)
      cmd_clean = rm -rf $(BUILD)

PHONY += clean
clean:
	$(call cmd,clean)

quiet_cmd_fetch_rt = FETCH   $(WASI_RT_URL)
      cmd_fetch_rt = rm -rf $(WASI_RT_DIR) && \
      				 mkdir -p $(WASI_RT_DIR)/lib && \
                     curl -sSfL $(WASI_RT_URL) \
                       | tar -xz -C $(WASI_RT_DIR)/lib --strip-components=1

$(WASI_RT_LIB): FORCE
	$(call cmd,fetch_rt)

targets += $(WASI_RT_LIB)

quiet_cmd_fetch_sysroot = FETCH   $(WASI_SYSROOT_URL)
      cmd_fetch_sysroot = rm -rf $(WASI_SYSROOT) && \
                          mkdir -p $(WASI_SYSROOT) && \
                          curl -sSfL $(WASI_SYSROOT_URL) \
                            | tar -xz -C $(WASI_SYSROOT) --strip-components=1

# Auto-discover tests:
#   - cases/<name>.c   → build/<name> (compiled+linked via clang/wasi-sdk)
#   - cases/<name>.wat → build/<name> (assembled directly via wat2wasm)
# For tests that need multiple sources or custom flags, add cases/<name>/Makefile
# and drop the matching source from cases/.
cases-c-src := $(wildcard cases/*.c)
bins-y += $(notdir $(basename $(cases-c-src)))
$(foreach src,$(cases-c-src),$(eval $(basename $(notdir $(src)))-y += $(src)))

include ../scripts/build.mk

# Ensure sysroot + compiler-rt builtins are staged before any C-built bin
# is linked. WAT-built bins don't go through ld so they don't need this.
$(bin-outputs): $(WASI_RT_LIB)

# WAT cases: assemble cases/<name>.wat → build/<name> directly.
cases-wat-src := $(wildcard cases/*.wat)
wat-bin-outputs := $(patsubst cases/%.wat,$(BUILD)/%,$(cases-wat-src))
targets += $(wat-bin-outputs)

quiet_cmd_wat = WAT     $@
      cmd_wat = $(WAT2WASM) $< -o $@

$(wat-bin-outputs): $(BUILD)/%: cases/%.wat FORCE
	$(call cmd,wat)

all: $(wat-bin-outputs)
