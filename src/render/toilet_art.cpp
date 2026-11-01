#include "render/toilet_art.h"
#include "render/mess_art.h"
#include "render/sprites.h"
#include "world/chunk.h"
#include <algorithm>
#include <raylib.h>

namespace {

/// The toilet's sprite is seen from above with its tank at the top. The
/// code knows its letters: H the bowl's water yellows with piss and browns
/// with dirt, R the rim
/// and t the tank's shaded edge streak.

/// Streaks on the porcelain, yellowed.
constexpr Color STREAK = {150, 128, 70, 255};
/// The bowl's water, pissed in till it is yellow through.
constexpr Color PISS_WATER = {204, 182, 62, 255};
/// Piss at which the water is yellow through: three or so uses.
constexpr float PISS_FULL = 0.3f;
constexpr float REMEMBERED = 0.45f;
/// Dirt at which the bowl is brown through, and the rim half streaked.
constexpr float BOWL_FULL = 2.0f, RIM_FULL = 6.0f;

uint32_t hash(uint32_t seed, int a, int b) {
    uint32_t h = seed ^ uint32_t(a) * 0x9E3779B1u ^ uint32_t(b) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

float unit(uint32_t h) { return float(h & 0xFFFF) / 65535.0f; }

Color blend(Color a, Color b, float t) {
    auto ch = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}

Color shade(Color c, bool inSight) {
    if (inSight) return c;
    return {(unsigned char)(c.r * REMEMBERED), (unsigned char)(c.g * REMEMBERED), (unsigned char)(c.b * REMEMBERED), c.a};
}

} // namespace

// The glyph is turned so its top lies toward back.
void drawToilet(int x, int y, Vec2i back, float dirt, float piss, uint32_t seed, bool inSight) {
    // Piss yellows the water rather than browning it.
    float bowl = std::min(1.0f, std::max(0.0f, dirt - piss) / BOWL_FULL), rim = std::min(0.5f, dirt / RIM_FULL);
    float yellow = std::min(1.0f, piss / PISS_FULL);
    const Sprite& toilet = sprites::get("toilet", CELL_PX, CELL_PX);
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++) {
            char g = toilet.letter(gx, gy);
            Rgba ink = toilet.ink(g);
            if (!ink.solid()) continue;
            Color c = {ink.r, ink.g, ink.b, ink.a};
            float r = unit(hash(seed, gx, gy));
            if (g == 'H') c = r < bowl ? fecesColor(1.0f - r) : blend(c, PISS_WATER, yellow);
            else if ((g == 'R' || g == 't') && r < rim) c = STREAK;
            // Back up as drawn; turned for the other walls.
            int px = gx, py = gy, e = CELL_PX - 1;
            if (back.y > 0) px = e - gx, py = e - gy;
            else if (back.x < 0) px = gy, py = e - gx;
            else if (back.x > 0) px = e - gy, py = gx;
            DrawPixel(x + px, y + py, shade(c, inSight));
        }
}
