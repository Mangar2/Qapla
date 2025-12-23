# Makefile (clean, macOS-fix for debugger)

# Compilers
ifeq ($(OS),Windows_NT)
  CXX := clang-cl
  CC  := clang-cl
  COMPILER_MODE := MSVC
else
  CXX := clang++
  CC  := clang
  COMPILER_MODE := GCC
endif

# Detect OS
UNAME_S := $(shell uname -s)

# Project
PROJECT_ROOT := $(abspath .)
BUILD_TYPE   ?= Debug
BUILD_BASE   := build
BUILD_DIR    := $(BUILD_BASE)/$(BUILD_TYPE)
ifeq ($(OS),Windows_NT)
  EXE        := $(BUILD_DIR)/Qapla.exe
else
  EXE        := $(BUILD_DIR)/Qapla
endif

# Source discovery (exclude build dir)
ifeq ($(OS),Windows_NT)
  SRC_CPP := $(shell C:/msys64/usr/bin/find . -type f -name "*.cpp" ! -path "$(BUILD_BASE)/*" | C:/msys64/usr/bin/sed 's|^\./||')
  SRC_C   := $(shell C:/msys64/usr/bin/find . -type f -name "*.c"   ! -path "$(BUILD_BASE)/*" | C:/msys64/usr/bin/sed 's|^\./||')
else
  SRC_CPP := $(shell find . -type f -name "*.cpp" ! -path "$(BUILD_BASE)/*" | sed 's|^\./||')
  SRC_C   := $(shell find . -type f -name "*.c"   ! -path "$(BUILD_BASE)/*" | sed 's|^\./||')
endif
SRC     := $(SRC_CPP) $(SRC_C)

OBJ_CPP := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC_CPP))
OBJ_C   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC_C))
OBJ     := $(OBJ_CPP) $(OBJ_C)

# Base flags
ifeq ($(OS),Windows_NT)
  # clang-cl accepts - prefix in makefiles to avoid / being treated as path
  CXXFLAGS_BASE := -std:c++20 -W3 -wd4100 -wd4101 -MT -EHsc
  CFLAGS_BASE   := -W3 -wd4100 -wd4101 -MT
else
  CXXFLAGS_BASE := -std=c++20 -Wno-unused-parameter -Wno-unused-variable \
                   -ffunction-sections -fdata-sections -MMD -MP
  CFLAGS_BASE   := -std=c99 -Wno-unused-parameter -Wno-unused-variable \
                   -ffunction-sections -fdata-sections -MMD -MP
endif


# Threading (portable, not needed on Windows)
ifneq ($(OS),Windows_NT)
  CXXFLAGS_THREAD := -pthread
  CFLAGS_THREAD   := -pthread
  LDFLAGS_THREAD  := -pthread
endif

# Build-type flags
ifeq ($(BUILD_TYPE),Debug)
  ifeq ($(OS),Windows_NT)
    CXXFLAGS_BT := -D_DEBUG -Od -Zi
    CFLAGS_BT   := -D_DEBUG -Od -Zi
  else
    CXXFLAGS_BT := -D_DEBUG -g -O0 -fno-omit-frame-pointer \
                   -fdebug-compilation-dir=$(PROJECT_ROOT)
    CFLAGS_BT   := -D_DEBUG -g -O0 -fno-omit-frame-pointer \
                   -fdebug-compilation-dir=$(PROJECT_ROOT)
  endif
else ifeq ($(BUILD_TYPE),WhatifRelease)
  ifeq ($(OS),Windows_NT)
    CXXFLAGS_BT := -DNDEBUG -DWHATIF_RELEASE -O2 -arch:AVX2
    CFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O2
  else
    CXXFLAGS_BT := -DNDEBUG -DWHATIF_RELEASE -O3 -funroll-loops -fno-rtti
    CFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O3 -funroll-loops
  endif
else ifeq ($(BUILD_TYPE),Release_NO_POPCOUNT)
  ifeq ($(OS),Windows_NT)
    CXXFLAGS_BT := -DNDEBUG -D__OLD_HW__ -O2
    CFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O2
  else
    CXXFLAGS_BT := -DNDEBUG -D__OLD_HW__ -O3 -funroll-loops -fno-rtti
    CFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O3 -funroll-loops
  endif
else # Release
  ifeq ($(OS),Windows_NT)
    CXXFLAGS_BT := -DNDEBUG -O2 -Oi -Ot -flto -arch:AVX2 -DUSE_POPCNT -DUSE_AVX2
    CFLAGS_BT   := -DNDEBUG -O2 -Oi -Ot -flto
  else
    CXXFLAGS_BT := -DNDEBUG -O3 -flto -funroll-loops -fno-rtti
    CFLAGS_BT   := -DNDEBUG -O3 -flto -funroll-loops
  endif
endif

# Platform-specific link flags (keep macOS simple)
ifeq ($(UNAME_S),Darwin)
  LDFLAGS_PLAT :=
else ifeq ($(OS),Windows_NT)
  LDFLAGS_PLAT := -MT -flto -fuse-ld=lld -link -SUBSYSTEM:CONSOLE
else
  LDFLAGS_PLAT := -Wl,--gc-sections -flto
endif

# Final flags
CXXFLAGS := $(CXXFLAGS_BASE) $(CXXFLAGS_THREAD) $(CXXFLAGS_BT)
CFLAGS   := $(CFLAGS_BASE)   $(CFLAGS_THREAD)   $(CFLAGS_BT)
LDFLAGS  := $(LDFLAGS_THREAD) $(LDFLAGS_PLAT)

# Targets
.PHONY: all clean distclean Debug Release Whatif OldHW

all: $(EXE)

$(EXE): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

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

Whatif:
	$(MAKE) BUILD_TYPE=WhatifRelease

OldHW:
	$(MAKE) BUILD_TYPE=Release_NO_POPCOUNT

# Auto dependencies
-include $(OBJ:.o=.d)
