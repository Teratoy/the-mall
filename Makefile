# The Mall. Builds against raylib found by pkg-config.
# On Bazzite, build inside the toolbox that has raylib-devel:
#   toolbox run -c claude-build make run

CXX      := g++
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -MMD -MP
# Quiet when missing: make apk runs on the host, which has no raylib-devel.
CPPFLAGS := -Isrc $(shell pkg-config --cflags raylib 2>/dev/null)
LDLIBS   := $(shell pkg-config --libs raylib 2>/dev/null) -lm

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
# A human drawn at twice the detail, beside the one the game draws now.
HIDEF := build/hidef
# Human frames with bottles, scaled up, to check the bottle art by eye.
BOTTLESHEET := build/bottle_sheet
# A human beside itself gone dirty, in every pose, with all that rides on it.
DIRTYSHEET := build/dirty_sheet

.PHONY: all run clean stuck check golden coverage shot scenarios genprobe hidef bottlesheet dirtysheet spritesheet spriteref fontdump lines sprites trapprobe icon apk install-apk logcat

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

# The soak's output, remade only when the soak or what it reads has changed,
# so make golden after a failed make check copies it instead of running it again.
build/soak.txt: $(STUCK) data/lines.txt data/names.txt Makefile
	./$(STUCK) $(SOAK_ARGS) > $@.tmp && mv $@.tmp $@

# The scenarios run on a spare core while the soak runs, and print after it.
check: $(STUCK) $(SCENARIOS)
	@./$(SCENARIOS) > build/scenarios.txt & sc=$$!; \
	$(MAKE) --no-print-directory build/soak.txt; soak=$$?; \
	wait $$sc; ok=$$?; cat build/scenarios.txt; [ $$ok = 0 ] && [ $$soak = 0 ]
	@diff -u $(GOLDEN) build/soak.txt > build/soak.diff \
		&& echo "check: soak output matches $(GOLDEN)" \
		|| (head -40 build/soak.diff; echo "check: soak output CHANGED (full diff in build/soak.diff)"; exit 1)

golden: build/soak.txt
	@mkdir -p $(dir $(GOLDEN))
	cp build/soak.txt $(GOLDEN)
	@echo "golden: $(GOLDEN) rewritten; commit it with the change that moved it"

coverage: $(STUCK) $(SCENARIOS)
	LOGLINES=build/loglines.txt ./$(STUCK) $(SOAK_ARGS) > /dev/null
	LOGLINES=build/loglines-scenarios.txt ./$(SCENARIOS) > /dev/null || true
	python3 tools/coverage.py build/loglines.txt build/loglines-scenarios.txt

$(SHOT): tools/shot.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

shot: $(SHOT)
	./$(SHOT) build/shot.png $(SHOT_ARGS)

$(GENPROBE): tools/genprobe.cpp tests/probe.h $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out tests/probe.h,$^) -o $@ $(LDLIBS)

genprobe: $(GENPROBE)
	./$(GENPROBE) $(GENPROBE_ARGS)

$(HIDEF): tools/hidef.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

hidef: $(HIDEF)
	./$(HIDEF) build/hidef.png $(HIDEF_ARGS)

$(BOTTLESHEET): tools/bottle_sheet.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

bottlesheet: $(BOTTLESHEET)
	./$(BOTTLESHEET) build/bottle-sheet.png

$(DIRTYSHEET): tools/dirty_sheet.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

dirtysheet: $(DIRTYSHEET)
	./$(DIRTYSHEET) build/dirty-sheet.png $(DIRTYSHEET_ARGS)

# Every sprite from data/sprites.txt at 1x, in all the ways the game draws it (tools/sprite_sheet.cpp).
SPRITESHEET := build/sprite_sheet

$(SPRITESHEET): tools/sprite_sheet.cpp $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

spritesheet: $(SPRITESHEET)
	./$(SPRITESHEET) build/sprite-sheet.png

# What the sprite editor's previews need: a human for size, and where the machines hold their goods.
spriteref: $(SPRITESHEET)
	./$(SPRITESHEET) --ref tools/sprite_editor

# The sprite editor: a local web page for data/sprites.txt, at http://localhost:8766.
# Python only, so it runs on the host too.
sprites:
	python3 tools/sprite_editor.py

# raylib's default font, saved for the lines editor (tools/font_dump.cpp).
FONTDUMP := build/font_dump

$(FONTDUMP): tools/font_dump.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

fontdump: $(FONTDUMP)
	./$(FONTDUMP) tools/lines_editor

# The lines editor: a local web page for data/lines.txt, at http://localhost:8765.
# Python only, so it runs on the host too.
lines:
	python3 tools/lines_editor.py

# Life logs of one soak world: build/lifelog seed [minutes] [crowd] [name] [every].
LIFELOG := build/lifelog
$(LIFELOG): tools/lifelog.cpp tests/probe.h $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out tests/probe.h,$^) -o $@ $(LDLIBS)

clean:
	rm -rf build

-include $(OBJ:.o=.d)

# Every trap set off in the soak worlds: same plate again, or a new one.
TRAPPROBE := build/trap_probe

$(TRAPPROBE): tools/trap_probe.cpp tests/probe.h $(filter-out build/main.o,$(OBJ))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter-out tests/probe.h,$^) -o $@ $(LDLIBS)

trapprobe: $(TRAPPROBE)
	./$(TRAPPROBE) $(TRAPPROBE_ARGS)

# The icon, written where the desktop shortcut and the APK find it (tools/icon.cpp).
ICON := build/icon
$(ICON): tools/icon.cpp build/render/icon_art.o build/render/sprites.o build/core/files.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

icon: $(ICON)
	./$(ICON)

# The phone build: see android/apk.mk. Run these on the host, not in the toolbox.
apk install-apk logcat:
	$(MAKE) -f android/apk.mk $@
