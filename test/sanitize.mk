#---------------------------------------------------------------------------------------------------
# Sanitizing
#---------------------------------------------------------------------------------------------------
MAKEFLAGS+= --no-print-directory
CLANG_VERSIONS=18 17 16 15 14
clang_tool=$(firstword $(foreach v,$(CLANG_VERSIONS),$(shell which $1-$v 2>/dev/null)) $(shell which $1 2>/dev/null) $1-is-not-installed)
CLANG_CXX:=$(call clang_tool,clang++)
CLANG_FORMAT:=$(call clang_tool,clang-format)
CLANG_TIDY:=$(call clang_tool,clang-tidy)
SCAN_BUILD:=$(firstword $(shell which scan-build 2>/dev/null) scan-build-is-not-installed)
CLANG_TIDY_CONFIG=$(abspath ./.clang-tidy)

wildcardr=$(foreach d,$(wildcard $1*),$(call wildcardr,$d/,$2) $(filter $(subst *,%,$2),$d))
SANITIZE_SOURCES=$(call wildcardr,include,*.hh) $(call wildcardr,include,*.h)
SANITIZE_TESTS_SOURCES=$(call wildcardr,test,*/test.cc)

#---------------------------------------------------------------------------------------------------
# Main targets
#---------------------------------------------------------------------------------------------------

.PHONY: sanitize code-analysis format-code format-check vardump format
sanitize: clean | code-analysis format-code

#---------------------------------------------------------------------------------------------------
# Code formatting
#---------------------------------------------------------------------------------------------------

format: format-code

format-code: ./.clang-format
	@echo "[note] Formatting code ..."
	@$(CLANG_FORMAT) -i -style=file $(SANITIZE_SOURCES)
	@$(CLANG_FORMAT) -i -style=file $(SANITIZE_TESTS_SOURCES)

format-check: ./.clang-format
	@echo "[note] Checking code formatting ..."
	@$(CLANG_FORMAT) -i -style=file --dry-run --Werror -- $(SANITIZE_SOURCES)
	@$(CLANG_FORMAT) -i -style=file --dry-run --Werror -- $(SANITIZE_TESTS_SOURCES)
	@echo "[note] Done format checking."

#---------------------------------------------------------------------------------------------------
# Code analysis
#---------------------------------------------------------------------------------------------------

SANITIZE_TIDY_RESULTS:=$(patsubst %.cc,$(BUILDDIR)/clang-tidy/%.tidy.log,$(SANITIZE_TESTS_SOURCES))

code-analysis: $(SANITIZE_TIDY_RESULTS)
	@echo "[note] Code analysis finished."

$(BUILDDIR)/clang-tidy/test/%/test.tidy.log: test/%/test.cc
	@echo "[note] Running clang-tidy on $< ..."
	@mkdir -p $(dir $@)
	@$(CLANG_TIDY) --config-file=$(CLANG_TIDY_CONFIG) -p="$(dir $@)" "$<" -- $(FLAGSCXX) -I. -I./test $(OPTS) >$@ 2>&1
 ifeq ($(CI),1)
	-@cat $@
 endif

#---------------------------------------------------------------------------------------------------

vardump:
	@echo "CLANG_CXX=$(CLANG_CXX)"
	@echo "CLANG_FORMAT=$(CLANG_FORMAT)"
	@echo "CLANG_TIDY=$(CLANG_TIDY)"
	@echo "SCAN_BUILD=$(SCAN_BUILD)"
	@echo "CLANG_TIDY_CONFIG=$(CLANG_TIDY_CONFIG)"
