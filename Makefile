# Pre-compiler and Compiler flags
CXX_FLAGS := -Wall -Wextra -std=c++20 -ggdb 
PRE_FLAGS := -MMD -MP

# Project directory structure
BIN := nipovpn/usr/bin
SRC := src
LIB := lib
INC := include
DEBIANDIR := nipovpn
DEBDIR := files/deb
DEBFILE := $(DEBDIR)/nipovpn.deb
MAINFILE := $(SRC)/main.cpp

# Build directories and output
TARGET := $(BIN)/nipovpn
BUILD := build

# Library search directories and flags
EXT_LIB :=
LDFLAGS := -lyaml-cpp -lssl -lcrypto
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
all: $(TARGET) deb

# Task producing target from built files
$(TARGET): $(OBJS)
	@echo "Building..."
	mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDPATHS) $(LDFLAGS)

# Compile all cpp files
$(BUILD)/%.o: $(SRC)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(PRE_FLAGS) $(INC_FLAGS) -c -o $@ $< $(LDPATHS) $(LDFLAGS)

# Create deb file
.PHONY: deb
deb:
	@echo "Creating deb file..."
	dpkg-deb --build $(DEBIANDIR)/ $(DEBFILE)

# Clean task
.PHONY: clean
clean:
	@echo "Clearing..."
	rm -rf $(BUILD)
	rm -rf $(BIN)
	rm -fr $(DEBFILE)

# Include all dependencies
-include $(DEPS)