CC      := g++
CXX     := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDFLAGS  :=
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
INC := -Iinclude

BIN := tsdb

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(INC)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: all clean
