CXX := clang++
CC := clang

CXXFLAGS := -std=c++20 -Wno-unused-variable -Wno-unused-parameter \
	-flto -ffunction-sections -fdata-sections

CFLAGS := -std=c99 -Wno-unused-variable -Wno-unused-parameter \
	-flto -ffunction-sections -fdata-sections

LDFLAGS := -flto -Wl,--gc-sections -fuse-ld=lld

# Enable generation of dependency files for C++ source files.
CXXFLAGS += -MMD -MP
# Enable generation of dependency files for C source files.
CFLAGS   += -MMD -MP

ifeq ($(OS), Windows_NT)
	LDFLAGS += -mconsole \
			   -static -static-libgcc -static-libstdc++ \
			   -lgcc -lgcc_eh -lsupc++
else
	CXXFLAGS += -pthread
	LDFLAGS += -static -static-libgcc -static-libstdc++ -pthread
endif

BUILD_TYPE ?= Release
BUILD_BASE := build
BUILD_DIR := $(BUILD_BASE)/$(BUILD_TYPE)

MKDIR := mkdir -p

SRC_CPP := $(shell find . -type f -name "*.cpp" ! -path "*/$(BUILD_DIR)/*" | sed 's|^\./||')
SRC_C   := $(shell find . -type f -name "*.c" ! -path "*/$(BUILD_DIR)/*" | sed 's|^\./||')
SRC := $(SRC_CPP) $(SRC_C)

OBJ_CPP := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC_CPP))
OBJ_C   := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC_C))
OBJ := $(OBJ_CPP) $(OBJ_C)

EXE := $(BUILD_DIR)/Qapla

ifeq ($(BUILD_TYPE), Debug)
	CXXFLAGS += -D_DEBUG -g
	CFLAGS   += -D_DEBUG -g
endif

ifeq ($(BUILD_TYPE), Release)
	CXXFLAGS += -DNDEBUG -O3 -march=x86-64-v2 -funroll-loops -fno-rtti
	CFLAGS   += -DNDEBUG -O3 -march=x86-64-v2 -funroll-loops
endif

ifeq ($(BUILD_TYPE), WhatifRelease)
	CXXFLAGS += -DNDEBUG -DWHATIF_RELEASE -O3 -march=x86-64 -funroll-loops -fno-rtti
	CFLAGS   += -DNDEBUG -DWHATIF_RELEASE -O3 -march=x86-64 -funroll-loops
endif

ifeq ($(BUILD_TYPE), Release_NO_POPCOUNT)
	CXXFLAGS += -DNDEBUG -D__OLD_HW__ -O3 -march=x86-64 -funroll-loops -fno-rtti
	CFLAGS   += -DNDEBUG -D__OLD_HW__ -O3 -march=x86-64 -funroll-loops
endif

all: $(EXE)

$(EXE): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | create-dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c | create-dirs
	$(CC) $(CFLAGS) -c $< -o $@

create-dirs:
	@find . -type d ! -path "./$(BUILD_DIR)" | sed 's|^./|$(BUILD_DIR)/|' | xargs -I {} $(MKDIR) {}

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

clan-all:
	rm -rf $(BUILD_BASE) 

Debug:
	$(MAKE) BUILD_TYPE=Debug

Release:
	$(MAKE) BUILD_TYPE=Release

Whatif:
	$(MAKE) BUILD_TYPE=WhatifRelease

OldHW:
	$(MAKE) BUILD_TYPE=Release_NO_POPCOUNT

-include $(OBJ:.o=.d)
