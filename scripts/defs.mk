ifeq ($(V),1)
quiet :=
else
quiet := quiet_
endif

squote := '
empty  :=
space  := $(empty) $(empty)
pound  := \#

# Used when comparing saved commands potentially containing spaces.
space_escape := _-_SPACE_-_

escsq = $(subst $(squote),'\$(squote)',$1)

# Check if both commands are the same including their order. Result is empty
# string if equal. If the target does not exist, the *.cmd file will not be included
# and $(savedcmd_$@) will be empty. The target will then be built even if $(newer-prereqs)
# is empty.
cmd-changed = $(filter-out $(subst $(space),$(space_escape),$(strip $(savedcmd_$@))), \
                           $(subst $(space),$(space_escape),$(strip $(cmd_$1))))

# Replace `$` with `$$` to preserve $ when reloading the .cmd file
# (needed for make)
# Replace `#` with `$(pound)` to avoid starting a comment in the .cmd file
# (needed for make)
# Replace `'` with `'\''` to be able to enclose the whole string in '...'
# (needed for the shell)
make-cmd = $(call escsq,$(subst $(pound),$$(pound),$(subst $$,$$$$,$(cmd_$(1)))))

cmd-name = $($(quiet)cmd_$(1))
log-cmd = $(if $(cmd-name),echo '  $(call escsq,$(cmd-name))',:)

newer-prereqs = $(filter-out $(PHONY),$?)

force-check = $(if $(filter FORCE,$^),,$(error File rule for $@ missing FORCE prerequisite))
target-check = $(if $(filter $@,$(targets)),,$(error File rule for $@ missing from $$(targets)))

file-checks = $(force-check)$(target-check)

check-cmd = $(if $@,,$(error $$(cmd) invoked outside of rule; did you indent with spaces instead of tabs?))

cmd-file = $(file-checks)$(if $(newer-prereqs)$(cmd-changed),set -e; $(log-cmd); mkdir -p $(@D); $(cmd_$(1)); printf '%s\n' 'savedcmd_$@ := $(make-cmd)' >$@.cmd)
cmd-phony = set -e; $(log-cmd); $(cmd_$(1))

is-phony = $(filter $@,$(PHONY))

cmd = @$(check-cmd)$(if $(is-phony),$(cmd-phony),$(cmd-file))

cflags-y :=
bins-y :=

targets :=
depfiles :=

PHONY :=