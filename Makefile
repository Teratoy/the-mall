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
# The soak run that make check compares with tests/golden/soak.txt. The
# sim is deterministic, so a pure refactor leaves it byte-identical; a
# change meant to alter behaviour is followed by make golden.
SOAK_ARGS := 6 15 1 6
GOLDEN := tests/golden/soak.txt
# Set-up situations for what the soak seldom reaches (tests/scenarios.cpp).
SCENARIOS := build/scenarios
# Screenshots without a visible window.
SHOT := build/shot
# Grows chunk interiors on every footprint and checks they keep to it.
GENPROBE := build/genprobe

.PHONY: all run clean stuck check golden coverage shot scenarios genprobe

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

$(SCENARIOS): tests/scenarios.cpp tests/probe.h $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out tests/probe.h,$^) -o $@ $(LDLIBS)

scenarios: $(SCENARIOS)
	./$(SCENARIOS)

$(STUCK): tests/stuck_soak.cpp tests/probe.h $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out tests/probe.h,$^) -o $@ $(LDLIBS)

stuck: $(STUCK)
	./$(STUCK)

check: $(STUCK) $(SCENARIOS)
	./$(SCENARIOS)
	./$(STUCK) $(SOAK_ARGS) > build/soak.txt
	@diff -u $(GOLDEN) build/soak.txt > build/soak.diff \
		&& echo "check: soak output matches $(GOLDEN)" \
		|| (head -40 build/soak.diff; echo "check: soak output CHANGED (full diff in build/soak.diff)"; exit 1)

golden: $(STUCK)
	@mkdir -p $(dir $(GOLDEN))
	./$(STUCK) $(SOAK_ARGS) > $(GOLDEN)
	@echo "golden: $(GOLDEN) rewritten; commit it with the change that moved it"

coverage: $(STUCK) $(SCENARIOS)
	LOGLINES=build/loglines.txt ./$(STUCK) $(SOAK_ARGS) > /dev/null
	LOGLINES=build/loglines-scenarios.txt ./$(SCENARIOS) > /dev/null || true
	python3 tools/coverage.py build/loglines.txt build/loglines-scenarios.txt

$(SHOT): tools/shot.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

shot: $(SHOT)
	./$(SHOT) build/shot.png $(SHOT_ARGS)

$(GENPROBE): tools/genprobe.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

genprobe: $(GENPROBE)
	./$(GENPROBE) $(GENPROBE_ARGS)

clean:
	rm -rf build

-include $(OBJ:.o=.d)
