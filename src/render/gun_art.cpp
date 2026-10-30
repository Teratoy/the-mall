#include "render/gun_art.h"
#include <raylib.h>

namespace {

/// Seen from its side: n the slide's lit top, N the slide, r the grip, t the
/// trigger guard.
constexpr const char* GUN_GLYPH[GUN_H] = {
    "......",
    "nnnnnn",
    "NNNNNN",
    "rr.t..",
    "rrtt..",
    "rr....",
};

constexpr Rgba SLIDE_LIT = hex(0xb4b8c0);
constexpr Rgba SLIDE = hex(0x5c6070);
constexpr Rgba GRIP = hex(0x9a6a40);
constexpr Rgba GUARD = hex(0x8a8e98);
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

} // namespace

Rgba gunInk(char g) {
    switch (g) {
    case 'n': return SLIDE_LIT;
    case 'N': return SLIDE;
    case 'r': return GRIP;
    case 't': return GUARD;
    default: return {};
    }
}

Rgba gunPixel(int gx, int gy) { return gunInk(GUN_GLYPH[gy][gx]); }

void drawGun(int x, int y, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < GUN_H; gy++)
        for (int gx = 0; gx < GUN_W; gx++) {
            Rgba c = gunPixel(gx, gy);
            if (c.solid()) DrawPixel(x + gx, y + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}
