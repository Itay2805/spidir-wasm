########################################################################################################################
# Wasm
########################################################################################################################

#-----------------------------------------------------------------------------------------------------------------------
# Build Configuration
#-----------------------------------------------------------------------------------------------------------------------

# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

.PHONY: force

# Use clang by default
CC				:= clang
AR				:= llvm-ar
LD				:= ld.lld

# Should we compile in debug
DEBUG			?= 1

# Should we compile in debug or not
SPIDIR_DEBUG	?= $(DEBUG)

# The spidir compilation target (given to cargo)
SPIDIR_TARGET	?= x86_64-unknown-none

# The cflags
CFLAGS			?=

#-----------------------------------------------------------------------------------------------------------------------
# Build constants
#-----------------------------------------------------------------------------------------------------------------------

# The output directories
ifeq ($(DEBUG),1)
OUT_DIR			:= out/debug
else
OUT_DIR			:= out/release
endif

BIN_DIR 		:= $(OUT_DIR)/bin
BUILD_DIR		:= $(OUT_DIR)/build

# Add some flags that we require to work
WASM_CFLAGS		:= $(CFLAGS)
WASM_CFLAGS		+= -Wall -Werror
WASM_CFLAGS		+= -std=gnu17
WASM_CFLAGS		+= -g
WASM_CFLAGS		+= -Wno-unused-label
WASM_CFLAGS		+= -Wno-address-of-packed-member
WASM_CFLAGS		+= -Wno-unused-function -Wno-format-invalid-specifier
WASM_CFLAGS		+= -fms-extensions -Wno-microsoft-anon-tag
WASM_CFLAGS		+= -Iinclude -Isrc -Ilibs/spidir/c-api/include
WASM_CFLAGS 	+= -DWASM_API_EXTERN=

WASM_CFLAGS		+= -fsanitize=undefined
WASM_CFLAGS 	+= -fno-sanitize=alignment
WASM_CFLAGS		+= -fsanitize=address
WASM_CFLAGS 	+= -fstack-protector-all

# Get the sources along side all of the objects and dependencies
SRCS 		:= $(shell find src -name '*.c')
OBJS 		:= $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS 		:= $(OBJS:%.o=%.d)

# Add the spidir object
OBJS 		+= $(BUILD_DIR)/spidir.o

# Choose which of the spidirs we want to use
ifeq ($(SPIDIR_DEBUG),1)
LIBSPIDIR	:= libs/spidir/target/$(SPIDIR_TARGET)/debug/libspidir.a
else
LIBSPIDIR	:= libs/spidir/target/$(SPIDIR_TARGET)/release/libspidir.a
endif

# The default rule
.PHONY: default
default: all

# All the rules
.PHONY: all
all: $(BIN_DIR)/wasm

#-----------------------------------------------------------------------------------------------------------------------
# Rules
#-----------------------------------------------------------------------------------------------------------------------

-include $(DEPS)

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(WASM_CFLAGS) -MMD -c $< -o $@

$(BIN_DIR)/libwasm.a: $(OBJS)
	@echo AR $@
	@mkdir -p $(@D)
	@$(AR) rc $@ $^

$(BIN_DIR)/wasm: host/main.c $(BIN_DIR)/libwasm.a
	@echo LD $@
	@mkdir -p $(@D)
	@$(CC) -o $@ $^ $(WASM_CFLAGS)

clean:
	rm -rf out
	rm -rf libs/spidir/target

#-----------------------------------------------------------------------------------------------------------------------
# Spidir rules
#-----------------------------------------------------------------------------------------------------------------------

# We are going to compile the entire libspidir.a into a single object file for easier
# linking of the tdn library
$(BUILD_DIR)/spidir.o: $(LIBSPIDIR)
	@echo CC $@
	@mkdir -p $(@D)
	@$(LD) -r --whole-archive -o $@ $^

libs/spidir/target/$(SPIDIR_TARGET)/release/libspidir.a: force
	cd libs/spidir/c-api && cargo build --release -p c-api --target $(SPIDIR_TARGET)

libs/spidir/target/$(SPIDIR_TARGET)/debug/libspidir.a: force
	cd libs/spidir/c-api && cargo build -p c-api --target $(SPIDIR_TARGET)