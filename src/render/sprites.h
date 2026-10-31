#pragma once
#include "core/color.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

/// A picture drawn in letters, read from data/sprites.txt: each row a
/// string, each letter an ink, '.' see-through. The code that draws it
/// knows its size and what some letters mean (which ones take dirt, which
/// blink); the picture and its colours are the file's.
struct Sprite {
    int w = 0, h = 0;
    std::vector<std::string> rows;
    /// The colour of each letter; clear for letters with no ink.
    std::array<Rgba, 128> inks{};
    /// Two letters that trade colours on every other blink, as LEDs running
    /// round a sign; 0 when none do.
    char blinkA = 0, blinkB = 0;

    char letter(int x, int y) const { return rows[size_t(y)][size_t(x)]; }
    Rgba ink(char c) const { return inks[size_t(uint8_t(c) & 127)]; }
    /// The colour at x, y, the blinking letters swapped on odd blinks.
    Rgba pixel(int x, int y, int blink = 0) const {
        char c = letter(x, y);
        if (blink % 2 && blinkA) c = c == blinkA ? blinkB : c == blinkB ? blinkA : c;
        return ink(c);
    }
};

namespace sprites {

/// The sprite of this name, w x h as the code draws it. The file is read the
/// first time one is asked for. One missing or the wrong size is reported
/// and drawn as a magenta box, so it shows. The reference stays good across
/// reloads, so callers may keep it.
const Sprite& get(std::string_view name, int w, int h);
/// Reads data/sprites.txt again (F5 in the game), keeping any sprite it
/// can't make out as it was. False when the file isn't found.
bool reload();

} // namespace sprites
