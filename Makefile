# Pre-compiler and Compiler flags
CXX_FLAGS := -Wall -Wextra -std=c++20 -ggdb
PRE_FLAGS := -MMD -MP

# Project directory structure
BIN := nipovpn/usr/bin
SRC := src
LIB := lib
INC := include
MAINFILE := $(SRC)/main.cpp

# Build directories and output
TARGET := $(BIN)/nipovpn
BUILD := build

# Library search directories and flags
EXT_LIB :=
LDFLAGS := -lyaml-cpp
LDPATHS := $(addprefix -L,$(LIB) $(EXT_LIB))

# Include directories
INC_DIRS := $(INC) $(shell find $(SRC) -type d) 
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# Construct build output and dependency filenames
SRCS := $(shell find $(SRC) -name *.cpp)
OBJS := $(subst $(SRC)/,$(BUILD)/,$(addsuffix .o,$(basename $(SRCS))))
DEPS := $(OBJS:.o=.d)

# Build task
build: all

# Main task
all: $(TARGET)

# Task producing target from built files
$(TARGET): $(OBJS)
	@echo "Building..."
	mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDPATHS) $(LDFLAGS)

# Compile all cpp files
$(BUILD)/%.o: $(SRC)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(PRE_FLAGS) $(INC_FLAGS) -c -o $@ $< $(LDPATHS) $(LDFLAGS)

# Clean task
.PHONY: clean
clean:
	@echo "Clearing..."
	rm -rf $(BUILD)
	rm -rf $(BIN)

# Include all dependencies
-include $(DEPS)