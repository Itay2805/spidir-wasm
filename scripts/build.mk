quiet_cmd_cc = CC      $@
      cmd_cc = $(CC) -MMD -MP $(cflags-y) $(cflags-$(linktarget)-y) $(cflags-$<-y) -c $< -o $@

quiet_cmd_as = AS      $@
      cmd_as = $(AS) -MMD -MP $(asflags-y) $(asflags-$(linktarget)-y) $(asflags-$<-y) -c $< -o $@

ldbuiltlibs = $(ldbuiltlibs-$(bin-name)-y:%=$(OBJ)/%.a)
all-ldlibs = $(addprefix -l,$(ldlibs-$(bin-name)-y)) $(ldbuiltlibs)

quiet_cmd_ld = LD      $@
      cmd_ld = $(CC) $(ldflags-y) $(ldflags-$(bin-name)-y) $(objs-$(bin-name)) $(all-ldlibs) -o $@

quiet_cmd_ar = AR      $@
      cmd_ar = rm -f $@; $(AR) rcs --thin $@ $(objs-$(lib-name))

bin-outputs = $(addprefix $(BUILD)/,$(bins-y))
targets += $(bin-outputs)

define build-objs
objs-$1-c := $$(patsubst %.c,$(OBJ)/$1/%.o,$$(filter %.c,$$($1-y)))
objs-$1-S := $$(patsubst %.S,$(OBJ)/$1/%.o,$$(filter %.S,$$($1-y)))

objs-$1 := $$(objs-$1-c) $$(objs-$1-S)

$$(objs-$1): private linktarget := $1

$$(objs-$1-c): $(OBJ)/$1/%.o: %.c $(ccdeps-y) $(ccdeps-$1-y) FORCE | $(ccodeps-y) $(ccodeps-$1-y)
	$$(call cmd,cc)

$$(objs-$1-S): $(OBJ)/$1/%.o: %.S $(asdeps-y) $(asdeps-$1-y) FORCE | $(asodeps-y) $(asodeps-$1-y)
	$$(call cmd,as)

targets += $$(objs-$1)
depfiles += $$(objs-$1:%.o=%.d)
endef

define each-bin
$(call build-objs,$(bin-name))

$(BUILD)/$(bin-name): private bin-name := $(bin-name)
$(BUILD)/$(bin-name): $$(objs-$(bin-name)) $$(ldbuiltlibs) $(lddeps-y) $(lddeps-$(bin-name)-y) FORCE
	$$(call cmd,ld)
endef

$(foreach bin-name,$(bins-y),$(eval $(each-bin)))

lib-outputs = $(libs-y:%=$(OBJ)/%.a)
targets += $(lib-outputs)

define each-lib
$(call build-objs,$(lib-name))

$(OBJ)/$(lib-name).a: private lib-name := $(lib-name)
$(OBJ)/$(lib-name).a: $$(objs-$(lib-name)) FORCE
	$$(call cmd,ar)
endef

$(foreach lib-name,$(libs-y),$(eval $(each-lib)))

all: $(bin-outputs)

PHONY += FORCE
FORCE: ;

existing-targets := $(wildcard $(sort $(targets)))
depfiles += $(existing-targets:%=%.cmd)

.PHONY: $(PHONY)
.DELETE_ON_ERROR:
.SECONDARY:

-include $(depfiles)