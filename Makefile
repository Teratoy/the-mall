# The Mall. Builds against raylib found by pkg-config.
# On Bazzite, build inside the toolbox that has raylib-devel:
#   toolbox run -c claude-build make run

CXX      := g++
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -MMD -MP
CPPFLAGS := -Isrc $(shell pkg-config --cflags raylib)
LDLIBS   := $(shell pkg-config --libs raylib) -lm

SRC := $(shell find src -name '*.cpp')
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := build/themall
# Headless check for humans stuck, looping or blocking each other.
STUCK := build/stuck_soak

.PHONY: all run clean stuck

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

$(STUCK): tests/stuck_soak.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

stuck: $(STUCK)
	./$(STUCK)

clean:
	rm -rf build

-include $(OBJ:.o=.d)
