#pragma once
#include "core/color.h"
#include <cstdint>

/// What is drawn on top of a wall. Purely looks, like floors.
enum class WallPattern : uint8_t {
    Plain,
    Bricks,    // small bricks, every other row shifted half a brick
    Blocks,    // big stone blocks, laid the same way
    Tiles,     // small square tiles
    Panels,    // a seam every cell, up and down
    Planks,    // close lines across
    Hazard,    // thick diagonal stripes
    Plates,    // riveted metal plates
    Weave,     // a basket weave
    Count
};

/// A wall's colours: its top, the pattern on it, and the south face seen
/// from the floor below. Kept brighter than floors so the two never mix up.
struct WallPalette {
    Rgba top, ink, face;
};

inline constexpr WallPalette WALL_PALETTES[] = {
    {hex(0x707070), hex(0x626262), hex(0x545454)}, // the old grey
    {hex(0x857a68), hex(0x766c5c), hex(0x5e5548)}, // sandstone
    {hex(0x86605a), hex(0x74514c), hex(0x5c403c)}, // brick
    {hex(0x6a7280), hex(0x5c6370), hex(0x4a505a)}, // blue concrete
    {hex(0x9a9a96), hex(0x868682), hex(0x6a6a66)}, // white tile
    {hex(0x66766a), hex(0x58665c), hex(0x46524a)}, // green
    {hex(0x5c5a62), hex(0x6e6c76), hex(0x46444c)}, // dark, lighter ink
};
inline constexpr int WALL_PALETTE_COUNT = int(sizeof WALL_PALETTES / sizeof WALL_PALETTES[0]);

struct WallStyle {
    WallPattern pattern = WallPattern::Plain;
    uint8_t palette = 0;
};
