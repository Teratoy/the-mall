#pragma once
#include "core/vec.h"
#include <cstdint>

/// Tables block walking but not sight. Every key and cake the world makes
/// lies on one, and whoever stands beside the table can take it. Shape and
/// material are purely looks, like floors.
enum class TableShape : uint8_t {
    Side,     // a small square one, a cell
    Pedestal, // a small round one on a post, a cell
    Round,    // a disc on a post, 2x2
    Dining,   // a rectangle on four legs, 3x2
    Bench,    // long and narrow, 3x1
    Oval,     // a long one with round ends, 4x2
    Corner,   // an L of two arms a cell wide, 3x3
    Count
};

enum class TableMaterial : uint8_t { Oak, Walnut, Pine, Steel, Marble, Granite, Glass, Plastic, Count };

struct Table {
    TableShape shape;
    TableMaterial material;
    /// Top-left cell, and size in cells as turned.
    Vec2i at, size;
    /// Corner: the corner of its square the arms meet in, clockwise from
    /// the top-left.
    uint8_t turn = 0;
    /// Varies grain, veins and plastic colours from one table to the next.
    uint32_t seed = 0;

    /// Whether a cell, in the table's own cells from its top-left, is under it.
    bool coversLocal(Vec2i l) const {
        if (l.x < 0 || l.y < 0 || l.x >= size.x || l.y >= size.y) return false;
        if (shape != TableShape::Corner) return true;
        int ex = turn == 1 || turn == 2 ? size.x - 1 : 0;
        int ey = turn >= 2 ? size.y - 1 : 0;
        return l.x == ex || l.y == ey;
    }
    bool covers(Vec2i cell) const { return coversLocal(cell - at); }
};

/// Size in cells of each shape, before turning.
inline constexpr Vec2i TABLE_SIZES[] = {{1, 1}, {1, 1}, {2, 2}, {3, 2}, {3, 1}, {4, 2}, {3, 3}};
static_assert(sizeof TABLE_SIZES / sizeof TABLE_SIZES[0] == size_t(TableShape::Count));
