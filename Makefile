CXX := clang++
CXXFLAGS := -std=c++17 -Wno-unused-variable -Wno-unused-parameter \
    -flto -ffunction-sections -fdata-sections

LDFLAGS := -flto -Wl,--gc-sections -fuse-ld=lld

ifeq ($(OS), Windows_NT)
    LDFLAGS += -mconsole \
               -static -static-libgcc -static-libstdc++ \
               -lgcc -lgcc_eh -lsupc++
else
    CXXFLAGS += -pthread
endif

BUILD_DIR := build
BUILD_TYPE ?= Release # Standardwert, falls kein Ziel angegeben wird

MKDIR := mkdir -p

SRC := $(shell find . -type f -name "*.cpp" ! -path "*/$(BUILD_DIR)/*")
OBJ := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
EXE := $(BUILD_DIR)/Qapla

# Debug-Build
ifeq ($(BUILD_TYPE), Debug)
    CXXFLAGS += -D_DEBUG -g
endif

# Release-Build mit Optimierung
ifeq ($(BUILD_TYPE), Release)
    CXXFLAGS += -DNDEBUG -O3 -march=x86-64-v3 -funroll-loops -fno-rtti
endif

# Whatif Release-Build
ifeq ($(BUILD_TYPE), WhatifRelease)
    CXXFLAGS += -DNDEBUG -DWHATIF_RELEASE -O3 -march=x86-64-v3 -funroll-loops -fno-rtti
endif

# Old Hardware (no popcount) Release-Build
ifeq ($(BUILD_TYPE), Release_NO_POPCOUNT)
    CXXFLAGS += -DNDEBUG -D__OLD_HW__ -O3 -march=x86-64 -funroll-loops -fno-rtti
endif

all: $(EXE)

$(EXE): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | create-dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

create-dirs:
	@find . -type d ! -path "./$(BUILD_DIR)" | sed 's|^./|$(BUILD_DIR)/|' | xargs -I {} $(MKDIR) {}

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

Debug:
	$(MAKE) BUILD_TYPE=Debug

Release:
	$(MAKE) BUILD_TYPE=Release

Whatif:
	$(MAKE) BUILD_TYPE=WhatifRelease

OldHW:
	$(MAKE) BUILD_TYPE=Release_NO_POPCOUNT



