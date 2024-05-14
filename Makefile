#---------------------------------------------------------------------------------------------------
# Main build Makefile
#
# Requires GNU make version >= v4.0 and basic UNIX tools (for Windows
# simply install GIT globally, so that tools like rm.exe are in the PATH;
# Linux/BSD: no action needed).
#---------------------------------------------------------------------------------------------------
# Optional tool chain prefix path, sometimes also referred to as CROSS_COMPILE, mainly suitable
# to switch between compilers.
TOOLCHAIN=

# Compiler and dependency selection
CXX=$(TOOLCHAIN)g++
LD=$(CXX)
CXX_STD=c++17
FLAGSCXX=-std=$(CXX_STD) -W -Wall -Wextra -pedantic
FLAGSCXX+=-I./include
SCM_COMMIT:=$(shell git log --pretty=format:%h -1 2>/dev/null || echo 0000000)
BUILD_DIRECTORY=build

#---------------------------------------------------------------------------------------------------
ifdef DEBUG
 FLAGSCXX+=-O0 -g
 FLAGSLD+=-O0 -g
else
 FLAGSCXX+=-O3
 FLAGSLD+=-O3
endif

# make command line overrides from the known teminologies
# (CXXFLAGS, LDFLAGS) without completely replacing the
# previous settings.
FLAGSCXX+=$(FLAGS)
FLAGSCXX+=$(CXXFLAGS)
FLAGSLD+=$(LDFLAGS)

# Pick windows or unix-like
ifeq ($(OS),Windows_NT)
 BUILDDIR:=./$(BUILD_DIRECTORY)
 BINARY_EXTENSION=.exe
 LDSTATIC+=-static -static-libstdc++ -static-libgcc
 FLAGSCXX+=
 LIBS+=
else
 BUILDDIR:=./$(BUILD_DIRECTORY)
 BINARY_EXTENSION=.elf
 LIBS+=
 ifdef STATIC
  LDSTATIC+=-static
 endif
endif

ifeq (GDBTRACE,1)
 GDB_TRACE:=$(TOOLCHAIN)gdb -q -batch -ex "set print thread-events off" \
	-ex "handle SIGALRM nostop pass" -ex "handle SIGCHLD nostop pass" -ex "run" -ex "thread apply all bt" \
	-ex "set backtrace past-main on" \
	--args
endif

#---------------------------------------------------------------------------------------------------
# make targets
#---------------------------------------------------------------------------------------------------
.PHONY: default clean all test-all mrproper help

default:
	@$(MAKE) -j test

clean:
	@$(MAKE) test-clean
	-@rm -rf $(BUILDDIR)

mrproper: clean

test-all:
	@mkdir -p $(BUILDDIR)
	@$(MAKE) -j test BUILD_DIRECTORY=$(BUILD_DIRECTORY)/std17 CXX_STD=c++17 TOOLCHAIN=$(TOOLCHAIN)
	@$(MAKE) -j test BUILD_DIRECTORY=$(BUILD_DIRECTORY)/std20 CXX_STD=c++20 TOOLCHAIN=$(TOOLCHAIN)

all: clean
	@echo "[info] Commit #$(SCM_COMMIT), compiler $(CXX), standard $(CXX_STD)"
	@echo "[info] Build c++ flags: $(FLAGSCXX)"
	@mkdir -p $(BUILDDIR)
	@$(MAKE) test-all
	@$(MAKE) -j code-analysis BUILD_DIRECTORY=$(BUILD_DIRECTORY)/analysis TOOLCHAIN=$(TOOLCHAIN)
	@$(MAKE) -j format-check BUILD_DIRECTORY=$(BUILD_DIRECTORY)/format TOOLCHAIN=$(TOOLCHAIN)
 ifneq ($(OS),Windows_NT)
	@$(MAKE) coverage BUILD_DIRECTORY=$(BUILD_DIRECTORY)/cov TOOLCHAIN=$(TOOLCHAIN)
 endif

help:
	@echo "Usage: make [ clean all test ]"
	@echo ""
	@echo " - test:           Build test binaries, run all tests that have changed."
	@echo " - all:            Run tests for standards c++11, c++14, c++17, c++20"
	@echo " - clean:          Clean binaries, temporary files and tests."
	@echo ""


include test/testenv.mk
include test/sanitize.mk

#--
