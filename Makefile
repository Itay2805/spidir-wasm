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

# Build with LLVM source-coverage instrumentation. Set indirectly via
# `make coverage`; not intended for direct use.
COVERAGE 		?=

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

# LLVM source-based coverage. Applied to libwasm and the host so the
# `coverage` target can show which JIT code is exercised by the tests.
cflags-$(COVERAGE) += -fprofile-instr-generate -fcoverage-mapping
ldflags-$(COVERAGE) += -fprofile-instr-generate

ldflags-y += -fuse-ld=lld

PHONY += all
all:

quiet_cmd_clean = CLEAN   $(BUILD)
      cmd_clean = rm -rf $(BUILD)

PHONY += clean
clean:
	$(call cmd,clean)

quiet_cmd_runtests = TEST    tests/build
      cmd_runtests = uv run --script tests/test.py

PHONY += test
test:
	$(MAKE) HOST=y
	$(MAKE) -C tests OPTIMIZE=y
	$(call cmd,runtests)

# Coverage report: rebuild instrumented, run the test suite (capturing per-
# process .profraw files), merge them, and surface a textual + HTML report
# focused on src/ (libwasm — the JIT, module loader, helpers).
COVERAGE_DIR := $(BUILD)/coverage

PHONY += coverage
coverage:
	$(MAKE) HOST=y COVERAGE=y
	$(MAKE) -C tests OPTIMIZE=y
	rm -rf $(COVERAGE_DIR)
	mkdir -p $(COVERAGE_DIR)
	LLVM_PROFILE_FILE='$(abspath $(COVERAGE_DIR))/%p.profraw' uv run --script tests/test.py || true
	@# Drive the ELF-emission path so jit/elf.c shows up in the report.
	LLVM_PROFILE_FILE='$(abspath $(COVERAGE_DIR))/elf-%p.profraw' \
	    $(BUILD)/main -m tests/build/mem --elf-output $(COVERAGE_DIR)/sample.elf >/dev/null 2>&1 || true
	llvm-profdata merge -sparse $(COVERAGE_DIR)/*.profraw -o $(COVERAGE_DIR)/merged.profdata
	@echo
	llvm-cov report $(BUILD)/main -instr-profile=$(COVERAGE_DIR)/merged.profdata src/
	llvm-cov show $(BUILD)/main -instr-profile=$(COVERAGE_DIR)/merged.profdata \
	    -format=html -output-dir=$(COVERAGE_DIR)/html src/
	@echo
	@echo "HTML report: $(COVERAGE_DIR)/html/index.html"

include host/Makefile
include libs/spidir.mk
include src/Makefile
include scripts/build.mk