#include "render/toilet_art.h"
#include "render/mess_art.h"
#include "world/chunk.h"
#include <algorithm>
#include <raylib.h>

namespace {

/// Seen from above with the tank at the top: T tank, t its shaded edge,
/// R the seat's rim, H the bowl's water.
constexpr const char* TOILET_GLYPH[CELL_PX] = {
    ".TTTT.",
    ".tttt.",
    ".RRRR.",
    "RHHHHR",
    "RHHHHR",
    ".RRRR.",
};
constexpr Color PORCELAIN = {228, 228, 222, 255};
constexpr Color PORCELAIN_SHADE = {168, 168, 162, 255};
constexpr Color RIM = {206, 204, 198, 255};
constexpr Color BOWL_WATER = {118, 146, 164, 255};
/// Streaks on the porcelain, yellowed.
constexpr Color STREAK = {150, 128, 70, 255};
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

Color shade(Color c, bool lit) {
    if (lit) return c;
    return {(unsigned char)(c.r * REMEMBERED), (unsigned char)(c.g * REMEMBERED), (unsigned char)(c.b * REMEMBERED), c.a};
}

} // namespace

// The glyph is turned so its top lies toward back.
void drawToilet(int x, int y, Vec2i back, float dirt, uint32_t seed, bool lit) {
    float bowl = std::min(1.0f, dirt / BOWL_FULL), rim = std::min(0.5f, dirt / RIM_FULL);
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++) {
            char g = TOILET_GLYPH[gy][gx];
            if (g == '.') continue;
            Color c = g == 'T' ? PORCELAIN : g == 't' ? PORCELAIN_SHADE : g == 'R' ? RIM : BOWL_WATER;
            float r = unit(hash(seed, gx, gy));
            if (g == 'H' && r < bowl) c = fecesColor(1.0f - r);
            else if ((g == 'R' || g == 't') && r < rim) c = STREAK;
            // Back up as drawn; turned for the other walls.
            int px = gx, py = gy, e = CELL_PX - 1;
            if (back.y > 0) px = e - gx, py = e - gy;
            else if (back.x < 0) px = gy, py = e - gx;
            else if (back.x > 0) px = e - gy, py = gx;
            DrawPixel(x + px, y + py, shade(c, lit));
        }
}
