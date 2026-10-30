#pragma once
#include "world/chunk.h"
#include <vector>

enum class Side : uint8_t { East, South };

/// Names the wall between two neighbouring slots by the one west or north of it.
struct EdgeKey {
    Vec2i slot;
    Side side;
    bool operator==(const EdgeKey&) const = default;
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const noexcept {
        return Vec2iHash{}(k.slot) * 2 + size_t(k.side);
    }
};

/// The wall between slots a and b, which must be neighbours.
inline EdgeKey edgeBetween(Vec2i a, Vec2i b) {
    Vec2i lo = (b.x < a.x || b.y < a.y) ? b : a;
    return {lo, a.y == b.y ? Side::East : Side::South};
}

/// An exit door in the wall between two slots. Each chunk draws it in its own
/// border, so the door is shared: unlocked from one side, it is open from both.
struct Door {
    Vec2i slot;
    Side side;
    /// First cell along the wall, and how many cells it spans.
    int offset;
    int width;
    bool locked = true;

    Vec2i dir() const { return side == Side::East ? Vec2i{1, 0} : Vec2i{0, 1}; }
    Vec2i other() const { return slot + dir(); }

    /// The door's cells in slot s, which is slot or other().
    std::vector<Vec2i> cellsIn(Vec2i s) const {
        std::vector<Vec2i> cells;
        Vec2i o = slot * CHUNK;
        for (int i = 0; i < width; i++) {
            if (side == Side::East)
                cells.push_back({s == slot ? o.x + CHUNK - 1 : o.x + CHUNK, o.y + offset + i});
            else
                cells.push_back({o.x + offset + i, s == slot ? o.y + CHUNK - 1 : o.y + CHUNK});
        }
        return cells;
    }
};
