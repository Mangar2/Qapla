# Makefile (clean, macOS-fix for debugger)

# OS-independent project settings
PROJECT_ROOT := $(abspath .)
BUILD_TYPE   ?= Debug
BUILD_BASE   := build
BUILD_DIR    := $(BUILD_BASE)/$(BUILD_TYPE)
EXTRA_DEFINES ?=

# Version from git tags, overridable: make QAPLA_VERSION=0.4.0 Release
QAPLA_VERSION  ?= $(shell git describe --tags --always 2>/dev/null || echo unknown)
VERSION_DEFINE := -DQAPLA_VERSION=\"$(QAPLA_VERSION)\"

# PGO: 'make ReleasePGO' builds an instrumented binary, trains it on the internal
# wmtest (run from test/epd, where wmtest.epd lives), merges the profile and
# rebuilds with it. Result: build/ReleasePGO/Qapla[.exe]
PGO_PROFDIR     := $(abspath $(BUILD_BASE)/pgo)
PGO_DATA        := $(PGO_PROFDIR)/qapla.profdata
PGO_TRAIN_DEPTH ?= 16

# ============================================================================
# WINDOWS SECTION
# ============================================================================
ifeq ($(OS),Windows_NT)

# Compilers
CXX := clang-cl
CC  := clang-cl
COMPILER_MODE := MSVC

# Executable name
EXE := $(BUILD_DIR)/Qapla.exe

# Source discovery (exclude build dir)
SRC_CPP := $(shell C:/msys64/usr/bin/find . -type f -name "*.cpp" ! -path "$(BUILD_BASE)/*" | C:/msys64/usr/bin/sed 's|^\./||')
SRC_C   := $(shell C:/msys64/usr/bin/find . -type f -name "*.c"   ! -path "$(BUILD_BASE)/*" | C:/msys64/usr/bin/sed 's|^\./||')
SRC     := $(SRC_CPP) $(SRC_C)

OBJ_CPP := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC_CPP))
OBJ_C   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))
OBJ     := $(OBJ_CPP) $(OBJ_C)

# Threading (not needed on Windows)
CXXFLAGS_THREAD :=
CFLAGS_THREAD   :=
LDFLAGS_THREAD  :=

# Build-type flags (including runtime library selection)
ifeq ($(BUILD_TYPE),Debug)
  # Debug: use debug runtime library (-MTd) and debug flags
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MTd -EHsc /clang:-MMD /clang:-MP 
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MTd /clang:-MMD /clang:-MP 
  CXXFLAGS_BT   := -D_DEBUG -Od -Zi -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2
  CFLAGS_BT     := -D_DEBUG -Od -Zi
  LDFLAGS_PLAT  := -MTd -fuse-ld=lld -link -DEBUG -PDB:$(BUILD_DIR)/Qapla.pdb -SUBSYSTEM:CONSOLE
else ifeq ($(BUILD_TYPE),WhatifRelease)
  # Release: use release runtime library (-MT)
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP 
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP 
  CXXFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O2 -arch:AVX2
  CFLAGS_BT     := -DNDEBUG -DWHATIF_RELEASE -O2
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else ifeq ($(BUILD_TYPE),Release_NO_POPCOUNT)
  # Release: use release runtime library (-MT)
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP 
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP 
  CXXFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O2
  CFLAGS_BT     := -DNDEBUG -D__OLD_HW__ -O2
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else ifeq ($(BUILD_TYPE),ReleaseOpt)
  # ReleaseOpt: release flags + PARAM_OPTIMIZE define
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP 
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP 
  CXXFLAGS_BT   := -DNDEBUG -DPARAM_OPTIMIZE -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2
  CFLAGS_BT     := -DNDEBUG -DPARAM_OPTIMIZE -O2 -Oi -Ot -flto
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else ifeq ($(BUILD_TYPE),ReleasePGOGen)
  # Release + profile instrumentation. The instrumented objects carry a defaultlib
  # directive for the profile runtime, so the link needs no extra library.
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP
  CXXFLAGS_BT   := -DNDEBUG -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2 /clang:-fprofile-generate=$(PGO_PROFDIR)
  CFLAGS_BT     := -DNDEBUG -O2 -Oi -Ot -flto /clang:-fprofile-generate=$(PGO_PROFDIR)
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else ifeq ($(BUILD_TYPE),ReleasePGO)
  # Release built with the profile collected by ReleasePGOGen
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP
  CXXFLAGS_BT   := -DNDEBUG -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2 /clang:-fprofile-use=$(PGO_DATA)
  CFLAGS_BT     := -DNDEBUG -O2 -Oi -Ot -flto /clang:-fprofile-use=$(PGO_DATA)
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else # Release
  # Release: use release runtime library (-MT)
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP
  CXXFLAGS_BT   := -DNDEBUG -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2
  CFLAGS_BT     := -DNDEBUG -O2 -Oi -Ot -flto
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
endif

# Merge tool for raw profiles (LLVM toolchain)
PROFDATA ?= llvm-profdata

# Final flags
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_THREAD) $(CXXFLAGS_BT) $(VERSION_DEFINE) $(EXTRA_DEFINES)
CFLAGS   := $(CFLAGS_BASE)   $(CFLAGS_THREAD)   $(CFLAGS_BT) $(EXTRA_DEFINES)
LDFLAGS  := $(LDFLAGS_THREAD) $(LDFLAGS_PLAT)

# Dependency file flag (target-specific)
DEPFILE_FLAG = /clang:-MT$@ /clang:-MF$@.d

# ============================================================================
# UNIX/LINUX/MACOS SECTION
# ============================================================================
else

# Detect platform and architecture
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Compilers
CXX := clang++
CC  := clang
COMPILER_MODE := GCC

# Executable name
EXE := $(BUILD_DIR)/Qapla

# Source discovery (exclude build dir)
SRC_CPP := $(shell find . -type f -name "*.cpp" ! -path "$(BUILD_BASE)/*" | sed 's|^\./||')
SRC_C   := $(shell find . -type f -name "*.c"   ! -path "$(BUILD_BASE)/*" | sed 's|^\./||')
SRC     := $(SRC_CPP) $(SRC_C)

OBJ_CPP := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC_CPP))
OBJ_C   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))
OBJ     := $(OBJ_CPP) $(OBJ_C)

# Architecture-specific optimization flags
ifneq ($(filter x86_64 amd64,$(UNAME_M)),)
  # x86_64: baseline (SSE2 only) and optimized (SSE4.2, POPCNT, etc.)
  ARCH_BASE := -march=x86-64
  ARCH_OPT  := -march=x86-64-v2
else ifneq ($(filter aarch64 arm64,$(UNAME_M)),)
  # ARM64 (Apple Silicon, AWS Graviton, Ampere, etc.)
  ARCH_BASE := -mcpu=native
  ARCH_OPT  := -mcpu=native
else
  # Unknown architecture: no specific tuning
  ARCH_BASE :=
  ARCH_OPT  :=
endif

# Base flags
CXXFLAGS_BASE := -std=c++20 -Wno-unused-parameter -Wno-unused-variable \
                 -ffunction-sections -fdata-sections -MMD -MP
CFLAGS_BASE   := -std=c99 -Wno-unused-parameter -Wno-unused-variable \
                 -ffunction-sections -fdata-sections -MMD -MP

# Threading
CXXFLAGS_THREAD := -pthread
CFLAGS_THREAD   := -pthread
LDFLAGS_THREAD  := -pthread

# Build-type flags
ifeq ($(BUILD_TYPE),Debug)
  CXXFLAGS_BT := -D_DEBUG -g -O0 -fno-omit-frame-pointer \
                 -fdebug-compilation-dir=$(PROJECT_ROOT)
  CFLAGS_BT   := -D_DEBUG -g -O0 -fno-omit-frame-pointer \
                 -fdebug-compilation-dir=$(PROJECT_ROOT)
else ifeq ($(BUILD_TYPE),WhatifRelease)
  CXXFLAGS_BT := -DNDEBUG -DWHATIF_RELEASE -O3 $(ARCH_BASE) -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O3 $(ARCH_BASE) -funroll-loops
else ifeq ($(BUILD_TYPE),Release_NO_POPCOUNT)
  CXXFLAGS_BT := -DNDEBUG -D__OLD_HW__ -O3 $(ARCH_BASE) -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O3 $(ARCH_BASE) -funroll-loops
else ifeq ($(BUILD_TYPE),ReleaseOpt)
  CXXFLAGS_BT := -DNDEBUG -DPARAM_OPTIMIZE -O3 -flto $(ARCH_OPT) -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -DPARAM_OPTIMIZE -O3 -flto $(ARCH_OPT) -funroll-loops
else ifeq ($(BUILD_TYPE),ReleasePGOGen)
  CXXFLAGS_BT := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops -fno-rtti -fprofile-generate=$(PGO_PROFDIR)
  CFLAGS_BT   := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops -fprofile-generate=$(PGO_PROFDIR)
else ifeq ($(BUILD_TYPE),ReleasePGO)
  CXXFLAGS_BT := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops -fno-rtti -fprofile-use=$(PGO_DATA)
  CFLAGS_BT   := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops -fprofile-use=$(PGO_DATA)
else # Release
  CXXFLAGS_BT := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -O3 -flto $(ARCH_OPT) -funroll-loops
endif

# Platform-specific link flags
ifeq ($(UNAME_S),Darwin)
  # macOS: libc++/compiler-rt ship with the OS; Apple clang has no -static-lib*.
  # -flto must be repeated at link time (the link rule uses only LDFLAGS).
  LDFLAGS_PLAT := -flto
  # llvm-profdata is not on PATH on macOS, xcrun finds the toolchain copy
  PROFDATA ?= xcrun llvm-profdata
else
  # Linux: static linking to avoid runtime dependencies
  LDFLAGS_PLAT := -Wl,--gc-sections -flto -fuse-ld=lld -static -static-libgcc -static-libstdc++
  PROFDATA ?= llvm-profdata
endif

# The profile flags must be repeated at link time: generate links the profile
# runtime, use feeds the profile to the LTO code generation.
ifeq ($(BUILD_TYPE),ReleasePGOGen)
  LDFLAGS_PLAT += -fprofile-generate=$(PGO_PROFDIR)
else ifeq ($(BUILD_TYPE),ReleasePGO)
  LDFLAGS_PLAT += -fprofile-use=$(PGO_DATA)
endif

# Final flags
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_THREAD) $(CXXFLAGS_BT) $(VERSION_DEFINE) $(EXTRA_DEFINES)
CFLAGS   := $(CFLAGS_BASE)   $(CFLAGS_THREAD)   $(CFLAGS_BT) $(EXTRA_DEFINES)
LDFLAGS  := $(LDFLAGS_THREAD) $(LDFLAGS_PLAT)

# Dependency file flag (target-specific)
DEPFILE_FLAG = -MT$@ -MF$@.d

endif
# ============================================================================
# END OF OS-SPECIFIC SECTIONS
# ============================================================================

# Targets
.PHONY: all clean distclean Debug Release ReleaseOpt Whatif OldHW ReleasePGO

all: $(EXE)

$(EXE): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(DEPFILE_FLAG) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFILE_FLAG) -c $< -o $@
  
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

distclean:
	rm -rf $(BUILD_BASE)

Debug:
	$(MAKE) BUILD_TYPE=Debug

Release:
	$(MAKE) BUILD_TYPE=Release

ReleaseOpt:
	$(MAKE) BUILD_TYPE=ReleaseOpt

# Three stages: instrumented build, training run (internal wmtest, run from
# test/epd where wmtest.epd lives), rebuild with the merged profile.
# Old profiles are removed first - profiles from an older source state would
# silently distort the optimization.
ReleasePGO:
	rm -rf $(PGO_PROFDIR)
	$(MAKE) BUILD_TYPE=ReleasePGOGen
	cd test/epd && printf 'wmtest sd $(PGO_TRAIN_DEPTH)\nquit\n' | $(abspath $(BUILD_BASE))/ReleasePGOGen/$(notdir $(EXE))
	$(PROFDATA) merge -output=$(PGO_DATA) $(PGO_PROFDIR)/*.profraw
	$(MAKE) BUILD_TYPE=ReleasePGO all

Whatif:
	$(MAKE) BUILD_TYPE=WhatifRelease

OldHW:
	$(MAKE) BUILD_TYPE=Release_NO_POPCOUNT

# Auto dependencies
-include $(OBJ:=.d)
