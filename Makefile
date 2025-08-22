# Makefile (clean, macOS-fix for debugger)

# Compilers
CXX := clang++
CC  := clang

# Detect OS
UNAME_S := $(shell uname -s)

# Project
PROJECT_ROOT := $(abspath .)
BUILD_TYPE   ?= Debug
BUILD_BASE   := build
BUILD_DIR    := $(BUILD_BASE)/$(BUILD_TYPE)
EXE          := $(BUILD_DIR)/Qapla

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


# Threading (portable)
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
  CXXFLAGS_BT := -DNDEBUG -DWHATIF_RELEASE -O3 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -DWHATIF_RELEASE -O3 -funroll-loops
else ifeq ($(BUILD_TYPE),Release_NO_POPCOUNT)
  CXXFLAGS_BT := -DNDEBUG -D__OLD_HW__ -O3 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -D__OLD_HW__ -O3 -funroll-loops
else # Release (default other than Debug)
  CXXFLAGS_BT := -DNDEBUG -O3 -funroll-loops -fno-rtti
  CFLAGS_BT   := -DNDEBUG -O3 -funroll-loops
endif

# Platform-specific link flags (keep macOS simple)
ifeq ($(UNAME_S),Darwin)
  LDFLAGS_PLAT :=
else
  LDFLAGS_PLAT := -Wl,--gc-sections
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
