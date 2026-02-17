# Makefile (clean, macOS-fix for debugger)

# OS-independent project settings
PROJECT_ROOT := $(abspath .)
BUILD_TYPE   ?= Debug
BUILD_BASE   := build
BUILD_DIR    := $(BUILD_BASE)/$(BUILD_TYPE)
EXTRA_DEFINES ?=

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
  LDFLAGS_PLAT  := -MTd -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
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
else # Release
  # Release: use release runtime library (-MT)
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc /clang:-MMD /clang:-MP 
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT /clang:-MMD /clang:-MP 
  CXXFLAGS_BT   := -DNDEBUG -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2
  CFLAGS_BT     := -DNDEBUG -O2 -Oi -Ot -flto
  LDFLAGS_PLAT  := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
endif

# Final flags
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_THREAD) $(CXXFLAGS_BT) $(EXTRA_DEFINES)
CFLAGS   := $(CFLAGS_BASE)   $(CFLAGS_THREAD)   $(CFLAGS_BT) $(EXTRA_DEFINES)
LDFLAGS  := $(LDFLAGS_THREAD) $(LDFLAGS_PLAT)

# Dependency file flag (target-specific)
DEPFILE_FLAG = /clang:-MF$@.d

# ============================================================================
# UNIX/LINUX/MACOS SECTION
# ============================================================================
else

# Detect specific Unix variant
UNAME_S := $(shell uname -s)

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
  CXXFLAGS_BT := -DNDEBUG -DWHATIF_RELEASE -O3 -march=x86-64 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O3 -march=x86-64 -funroll-loops
else ifeq ($(BUILD_TYPE),Release_NO_POPCOUNT)
  CXXFLAGS_BT := -DNDEBUG -D__OLD_HW__ -O3 -march=x86-64 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O3 -march=x86-64 -funroll-loops
else ifeq ($(BUILD_TYPE),ReleaseOpt)
  CXXFLAGS_BT := -DNDEBUG -DPARAM_OPTIMIZE -O3 -flto -march=x86-64-v2 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -DPARAM_OPTIMIZE -O3 -flto -march=x86-64-v2 -funroll-loops
else # Release
  CXXFLAGS_BT := -DNDEBUG -O3 -flto -march=x86-64-v2 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -O3 -flto -march=x86-64-v2 -funroll-loops
endif

# Platform-specific link flags
ifeq ($(UNAME_S),Darwin)
  # macOS: static link C++ stdlib and compiler runtime for portability
  LDFLAGS_PLAT := -static-libgcc -static-libstdc++
else
  # Linux: static linking to avoid runtime dependencies
  LDFLAGS_PLAT := -Wl,--gc-sections -flto -fuse-ld=lld -static -static-libgcc -static-libstdc++
endif

# Final flags
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_THREAD) $(CXXFLAGS_BT) $(EXTRA_DEFINES)
CFLAGS   := $(CFLAGS_BASE)   $(CFLAGS_THREAD)   $(CFLAGS_BT) $(EXTRA_DEFINES)
LDFLAGS  := $(LDFLAGS_THREAD) $(LDFLAGS_PLAT)

# Dependency file flag (target-specific)
DEPFILE_FLAG = -MF$@.d

endif
# ============================================================================
# END OF OS-SPECIFIC SECTIONS
# ============================================================================

# Targets
.PHONY: all clean distclean Debug Release ReleaseOpt Whatif OldHW

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

Whatif:
	$(MAKE) BUILD_TYPE=WhatifRelease

OldHW:
	$(MAKE) BUILD_TYPE=Release_NO_POPCOUNT

# Auto dependencies
-include $(OBJ:=.d)
