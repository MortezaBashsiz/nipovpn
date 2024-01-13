CXX       := g++
CXX_FLAGS := -Wall -Wextra -std=c++20 -ggdb -lyaml-cpp

BIN     := bin
SRC     := src
LIB     := lib
LIBRARIES   := 
EXECUTABLE  := nipovpn


all: $(BIN)/$(EXECUTABLE)

run: clean all
		clear
		@echo "Executing..."
		./$(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.cpp
		@echo "Building..."
		$(CXX) $(CXX_FLAGS) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
		@echo "Clearing..."
		-rm $(BIN)/*
