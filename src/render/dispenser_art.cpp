#include "render/dispenser_art.h"
#include "render/sprites.h"
#include <cmath>
#include <raylib.h>

namespace {

/// The jug's rows that hold water, top to bottom: drawn as water up from
/// the bottom as far as it is full, the rest as the empty jug. The neck
/// below them holds water while any is left.
constexpr int JUG_TOP = 1, JUG_ROWS = 6;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

} // namespace

Rgba dispenserPixel(int gx, int gy, float level, bool side) {
    const Sprite& s = sprites::get(side ? "water dispenser side" : "water dispenser", DISPENSER_W, DISPENSER_H);
    char c = s.letter(gx, gy);
    if (c == 'w') {
        const int full = level <= 0.0f ? 0 : std::max(1, int(std::ceil(level * JUG_ROWS - 0.01f)));
        const bool neck = gy >= JUG_TOP + JUG_ROWS;
        if (neck ? full == 0 : gy < JUG_TOP + JUG_ROWS - full) c = 'e';
    }
    return s.ink(c);
}

void drawDispenser(int x, int y, float level, bool side, bool flip, bool inSight) {
    const float k = inSight ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < DISPENSER_H; gy++)
        for (int gx = 0; gx < DISPENSER_W; gx++) {
            Rgba c = dispenserPixel(gx, gy, level, side);
            if (!c.solid()) continue;
            DrawPixel(x + (flip ? DISPENSER_W - 1 - gx : gx), y - DISPENSER_H + gy,
                      {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}
