#include "render/cable_art.h"
#include "world/chunk.h"
#include <raylib.h>

namespace {

/// The straight runs, lying across the cell from west to east; a run from
/// north to south is the same turned. The lanes keep to rows 1 to 4 at
/// both ends, so any look meets any other. a a thin cable, k the thick
/// one, K a shine on it, w a tie.
constexpr const char* STRAIGHT[CABLE_VARIANTS][CELL_PX] = {
    {"......", "aaaaaa", "kkKkkk", "kkkkkk", "aaaaaa", "......"},
    {"..aa..", "aa..aa", "kkkkkK", "kkkkkk", "a...aa", ".aaa.."},
    {"..w...", "aawaaa", "kkwkkk", "kkwkkk", "aawaaa", "......"},
};

/// The corner from west to south; the other three are it flipped. Each
/// lane leaves by the same rows and columns as a straight run, the outer
/// thin cable going round the outside.
constexpr const char* CORNER[CELL_PX] = {"......", "aaaa..", "kkkka.", "kkkka.", "aakka.", ".akka."};
/// A branch joining a run lies over it only this far from its own edge,
/// so its thick cable meets the run's and its thin ones part the run's.
constexpr int BRANCH_REACH = 2;

constexpr Rgba THIN = hex(0xd8ccaa);
constexpr Rgba THICK = hex(0x1e1e22);
constexpr Rgba SHINE = hex(0x4a4c54);
constexpr Rgba TIE = hex(0xe8e8e0);
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

Rgba ink(char g) {
    switch (g) {
    case 'a': return THIN;
    case 'k': return THICK;
    case 'K': return SHINE;
    case 'w': return TIE;
    default: return {};
    }
}

/// A straight run's pixel, across the cell, or down it when not across.
Rgba straight(int variant, bool across, int gx, int gy) {
    return ink(across ? STRAIGHT[variant][gy][gx] : STRAIGHT[variant][gx][gy]);
}

/// A corner's pixel, flipped from west-to-south as the edges it leaves by say.
Rgba corner(bool east, bool north, int gx, int gy) {
    return ink(CORNER[north ? CELL_PX - 1 - gy : gy][east ? CELL_PX - 1 - gx : gx]);
}

/// Whether gx, gy lies within reach of the branch leaving by the one edge in branch.
bool onBranch(uint8_t branch, int gx, int gy) {
    switch (branch) {
    case CABLE_N: return gy < BRANCH_REACH;
    case CABLE_S: return gy >= CELL_PX - BRANCH_REACH;
    case CABLE_W: return gx < BRANCH_REACH;
    default: return gx >= CELL_PX - BRANCH_REACH;
    }
}

} // namespace

Rgba cablePixel(uint8_t links, int variant, int gx, int gy) {
    bool across = (links & (CABLE_E | CABLE_W)) == (CABLE_E | CABLE_W);
    bool down = (links & (CABLE_N | CABLE_S)) == (CABLE_N | CABLE_S);
    if (across || down) {
        uint8_t branches = links & ~(across ? CABLE_E | CABLE_W : CABLE_N | CABLE_S);
        if (!branches) return straight(variant, across, gx, gy);
        for (uint8_t b : {CABLE_N, CABLE_E, CABLE_S, CABLE_W}) {
            if (!(branches & b) || !onBranch(b, gx, gy)) continue;
            Rgba over = straight(0, !across, gx, gy);
            if (over.solid()) return over;
        }
        return straight(0, across, gx, gy);
    }
    switch (links) {
    case CABLE_N | CABLE_E: return corner(true, true, gx, gy);
    case CABLE_E | CABLE_S: return corner(true, false, gx, gy);
    case CABLE_S | CABLE_W: return corner(false, false, gx, gy);
    case CABLE_W | CABLE_N: return corner(false, true, gx, gy);
    default: return {};
    }
}

void drawCable(int x, int y, uint8_t links, int variant, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++) {
            Rgba c = cablePixel(links, variant, gx, gy);
            if (!c.solid()) continue;
            DrawPixel(x + gx, y + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}
