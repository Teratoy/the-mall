#include "render/floor_tiles.h"
#include "world/chunk.h"
#include <cmath>
#include <iterator>
#include <cstdlib>

namespace {

constexpr int T = FloorTiles::TILE_PX;
constexpr int CELLS_PER_TILE = T / CELL_PX;
static_assert(T % CELL_PX == 0, "a tile must be whole cells");

constexpr int mod(int a, int b) { return (a % b + b) % b; }

/// A wave for x in 0..T: whole pixels, one swing per tile.
int wave(int x) { return int(std::lround(2.0 * std::sin(x * 2.0 * M_PI / T))); }

/// Whether pixel x, y of a pattern's tile is ink. Every period divides T,
/// so the tiles meet without seams.
bool ink(FloorPattern p, int x, int y) {
    switch (p) {
    case FloorPattern::Plain: return false;
    case FloorPattern::Grid: return x % CELL_PX == 0 || y % CELL_PX == 0;
    case FloorPattern::BigGrid: return x % 12 == 0 || y % 12 == 0;
    case FloorPattern::Squares: {
        int lx = x % 12, ly = y % 12;
        bool in = lx >= 3 && lx <= 8 && ly >= 3 && ly <= 8;
        return in && (lx == 3 || lx == 8 || ly == 3 || ly == 8);
    }
    case FloorPattern::LinesH: return y % 4 == 0;
    case FloorPattern::LinesV: return x % 4 == 0;
    case FloorPattern::Diagonal: return (x + y) % 6 == 0;
    case FloorPattern::AntiDiagonal: return mod(x - y, 6) == 0;
    case FloorPattern::Shallow: return (x + 2 * y) % 8 < 2;
    case FloorPattern::Steep: return (2 * x + y) % 8 < 2;
    case FloorPattern::Zigzag: return mod(y - std::abs(x % 8 - 4), 6) == 0;
    case FloorPattern::Crosshatch: return (x + y) % 8 == 0 || mod(x - y, 8) == 0;
    case FloorPattern::Lattice: return (x + 2 * y) % 12 < 2 || mod(2 * x - y, 12) < 2;
    case FloorPattern::Triangles: return x % 8 == 0 || y % 8 == 0 || (x + y) % 8 == 0;
    case FloorPattern::Chess: return (x / CELL_PX + y / CELL_PX) % 2 == 1;
    case FloorPattern::ChessSmall: return (x / 3 + y / 3) % 2 == 1;
    case FloorPattern::ChessBig: return (x / 12 + y / 12) % 2 == 1;
    case FloorPattern::Diamonds: return ((x + y) / 12 + (x - y + T) / 12) % 2 == 1;
    case FloorPattern::WavesH: return mod(y - wave(x), 8) == 0;
    case FloorPattern::WavesV: return mod(x - wave(y), 8) == 0;
    case FloorPattern::Dots: return x % CELL_PX == 3 && y % CELL_PX == 3;
    case FloorPattern::DotsStaggered:
        return (x % 8 == 1 && y % 8 == 1) || (x % 8 == 5 && y % 8 == 5);
    case FloorPattern::Speckle: {
        // A fixed hash of the pixel, so the chips never move.
        uint32_t h = uint32_t(x * 374761393 + y * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (h ^ (h >> 16)) % 9 == 0;
    }
    case FloorPattern::Count: break;
    }
    return false;
}

/// Whether pixel x, y of a wall pattern's tile is ink, seamless like floors.
bool wallInk(WallPattern p, int x, int y) {
    switch (p) {
    case WallPattern::Plain: return false;
    case WallPattern::Bricks: return y % 4 == 0 || (x + y / 4 % 2 * 4) % 8 == 0;
    case WallPattern::Blocks: return y % 6 == 0 || (x + y / 6 % 2 * 6) % 12 == 0;
    case WallPattern::Tiles: return x % 4 == 0 || y % 4 == 0;
    case WallPattern::Panels: return x % CELL_PX == 0;
    case WallPattern::Planks: return y % 3 == 0;
    case WallPattern::Hazard: return (x + y) % 8 < 3;
    case WallPattern::Plates: {
        int lx = x % 12, ly = y % 12;
        bool rivet = (lx == 2 || lx == 10) && (ly == 2 || ly == 10);
        return lx == 0 || ly == 0 || rivet;
    }
    case WallPattern::Weave: {
        // 6x6 squares of two lines, turned across in every other square.
        bool across = (x / 6 + y / 6) % 2 == 0;
        int l = (across ? y : x) % 6;
        return l == 1 || l == 4;
    }
    case WallPattern::Count: break;
    }
    return false;
}

/// Light caught on still water: a short level stroke that swells from a
/// dot and fades back, starting at its phase in the loop. None reaches the
/// tile's edge, so neighbouring tiles can be at different points in the loop.
struct Glint {
    int x, y, phase;
};
constexpr Glint GLINTS[] = {{4, 2, 0}, {15, 6, 4}, {9, 11, 8}, {19, 15, 12}, {6, 19, 16}};
/// A glint's length on each frame of its life.
constexpr int GLINT_LENGTHS[] = {1, 2, 3, 4, 4, 3, 2, 1};

bool glint(int frame, int x, int y) {
    for (const Glint& g : GLINTS) {
        int age = mod(frame - g.phase, FloorTiles::WATER_FRAMES);
        if (y != g.y || age >= int(std::size(GLINT_LENGTHS))) continue;
        int len = GLINT_LENGTHS[age];
        if (x >= g.x - (len - 1) / 2 && x <= g.x + len / 2) return true;
    }
    return false;
}

} // namespace

// White ink on clear, tinted when drawn.
FloorTiles::FloorTiles() {
    const int n = int(FloorPattern::Count);
    static_assert(int(FloorPattern::Count) >= WATER_FRAMES, "water frames fit the first row's width");
    static_assert(int(FloorPattern::Count) >= int(WallPattern::Count), "walls fit the first row's width");
    Image img = GenImageColor(T * n, T * 3, BLANK);
    for (int i = 0; i < n; i++)
        for (int y = 0; y < T; y++)
            for (int x = 0; x < T; x++)
                if (ink(FloorPattern(i), x, y)) ImageDrawPixel(&img, i * T + x, y, WHITE);
    for (int f = 0; f < WATER_FRAMES; f++)
        for (int y = 0; y < T; y++)
            for (int x = 0; x < T; x++)
                if (glint(f, x, y)) ImageDrawPixel(&img, f * T + x, T + y, WHITE);
    for (int i = 0; i < int(WallPattern::Count); i++)
        for (int y = 0; y < T; y++)
            for (int x = 0; x < T; x++)
                if (wallInk(WallPattern(i), x, y)) ImageDrawPixel(&img, i * T + x, 2 * T + y, WHITE);
    atlas_ = LoadTextureFromImage(img);
    SetTextureFilter(atlas_, TEXTURE_FILTER_POINT);
    UnloadImage(img);
}

FloorTiles::~FloorTiles() { UnloadTexture(atlas_); }

void FloorTiles::draw(int x, int y, Vec2i cell, FloorPattern pattern, Color ink) const {
    if (pattern == FloorPattern::Plain) return;
    Rectangle src{float(int(pattern) * T + mod(cell.x, CELLS_PER_TILE) * CELL_PX),
                  float(mod(cell.y, CELLS_PER_TILE) * CELL_PX), float(CELL_PX), float(CELL_PX)};
    DrawTextureRec(atlas_, src, {float(x), float(y)}, ink);
}

// Each tile starts the loop at its own point, so a big pool does not glint
// in step everywhere.
void FloorTiles::drawWater(int x, int y, Vec2i cell, int frame, Color ink) const {
    Vec2i tile{floorDiv(cell.x, CELLS_PER_TILE), floorDiv(cell.y, CELLS_PER_TILE)};
    frame += int(Vec2iHash{}(tile) % WATER_FRAMES);
    Rectangle src{float(mod(frame, WATER_FRAMES) * T + mod(cell.x, CELLS_PER_TILE) * CELL_PX),
                  float(T + mod(cell.y, CELLS_PER_TILE) * CELL_PX), float(CELL_PX), float(CELL_PX)};
    DrawTextureRec(atlas_, src, {float(x), float(y)}, ink);
}

void FloorTiles::drawWall(int x, int y, Vec2i cell, WallPattern pattern, int h, Color ink) const {
    if (pattern == WallPattern::Plain) return;
    Rectangle src{float(int(pattern) * T + mod(cell.x, CELLS_PER_TILE) * CELL_PX),
                  float(2 * T + mod(cell.y, CELLS_PER_TILE) * CELL_PX), float(CELL_PX), float(h)};
    DrawTextureRec(atlas_, src, {float(x), float(y)}, ink);
}
