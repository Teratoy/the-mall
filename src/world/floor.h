#pragma once
#include "core/color.h"
#include <cstdint>

/// What is drawn on a floor cell. Purely looks: nothing walks differently.
enum class FloorPattern : uint8_t {
    Plain,
    Grid,       // a seam round every cell
    BigGrid,    // a seam round every 2x2 cells
    Squares,    // a small square outline per 2x2 cells
    LinesH,
    LinesV,
    Diagonal,   // 45 degrees, rising
    AntiDiagonal,
    Shallow,    // about 27 degrees
    Steep,      // about 63 degrees
    Zigzag,
    Crosshatch, // both diagonals
    Lattice,    // shallow and steep crossing
    Triangles,  // horizontal, vertical and diagonal crossing
    Chess,      // one check per cell
    ChessSmall,
    ChessBig,
    Diamonds,   // a chessboard turned 45 degrees
    WavesH,
    WavesV,
    Dots,
    DotsStaggered,
    Speckle,    // terrazzo
    Count
};

/// A floor's colours: the ground, and the pattern drawn over it. Kept well
/// darker than walls so the two never mix up.
struct FloorPalette {
    Rgba base, ink;
};

inline constexpr FloorPalette FLOOR_PALETTES[] = {
    {hex(0x444444), hex(0x3a3a3a)}, // Pocket Rave's ground
    {hex(0x4a4239), hex(0x3e3730)}, // warm stone
    {hex(0x3a404c), hex(0x303540)}, // slate
    {hex(0x3c463e), hex(0x323a34)}, // moss
    {hex(0x4e3c36), hex(0x40302c)}, // rust
    {hex(0x54524e), hex(0x464440)}, // pale terrazzo
    {hex(0x323236), hex(0x404046)}, // dark, lighter ink
    {hex(0x364848), hex(0x2c3c3c)}, // teal
};
inline constexpr int FLOOR_PALETTE_COUNT = int(sizeof FLOOR_PALETTES / sizeof FLOOR_PALETTES[0]);

struct Floor {
    FloorPattern pattern = FloorPattern::Plain;
    uint8_t palette = 0;
};
