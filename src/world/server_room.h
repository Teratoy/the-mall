#pragma once
#include "core/vec.h"
#include <algorithm>
#include <cstdint>

/// What stands in a server room: racks two cells wide, which block walking
/// and sight; terminals on one cell and the console desk, which block
/// walking only.
enum class Rig : uint8_t { Rack, Terminal, Desk };

/// The console desk stands six cells wide and four deep. Its top fills its
/// first two rows and its wings the outer two cells each side of the third;
/// the chair's cells and the fourth row are floor (user: the chair is walked
/// over).
inline constexpr int DESK_W = 6, DESK_D = 4;
/// Cells a rack's body is drawn over above its foot, and the desk's screens.
inline constexpr int RACK_RISE = 4, DESK_RISE = 2;

/// One machine of a server room.
struct ServerPiece {
    Rig kind;
    /// A rack's or terminal's left cell, in the row it stands on; the desk's top-left cell.
    Vec2i at;
    /// So their lights don't blink in step.
    uint32_t seed = 0;

    /// Whether the piece blocks walking on the cell, its offset from at.
    static bool covers(Rig kind, Vec2i d) {
        switch (kind) {
        case Rig::Rack: return d.y == 0 && (d.x == 0 || d.x == 1);
        case Rig::Terminal: return d == Vec2i{0, 0};
        case Rig::Desk:
            if (d.x < 0 || d.x >= DESK_W || d.y < 0) return false;
            return d.y < 2 || (d.y == 2 && (d.x < 2 || d.x >= DESK_W - 2));
        }
        return false;
    }
    /// Whether a human at c can use the piece: a terminal from either side,
    /// the desk from its chair, the walkable middle of its third row.
    bool usedFrom(Vec2i c) const {
        switch (kind) {
        case Rig::Rack: return false;
        case Rig::Terminal: return c.y == at.y && (c.x == at.x - 1 || c.x == at.x + 1);
        case Rig::Desk: return c.y == at.y + 2 && (c.x == at.x + 2 || c.x == at.x + 3);
        }
        return false;
    }
    /// The row the piece is drawn standing on, for drawing back to front.
    int footRow() const { return kind == Rig::Desk ? at.y + DESK_D - 2 : at.y; }
};

/// One door of an airlock. It opens freely to whoever comes from inside,
/// costs a key from outside, and shuts and locks again once nobody stands
/// in it for a moment (user).
struct AirlockDoor {
    /// The way into the server room through it.
    Vec2i in;
    /// Seconds it stays open with nobody in it.
    float open = 0.0f;
};

/// A server room's box of floor inside its walls, in world cells, with
/// the outer and inner doors of its airlock.
struct ServerRoom {
    int x, y, w, h;
    Vec2i outer, inner;

    bool contains(Vec2i c) const { return c.x >= x && c.y >= y && c.x < x + w && c.y < y + h; }
    /// Its floor, its doors or the lobby between them.
    bool encloses(Vec2i c) const {
        if (contains(c)) return true;
        Vec2i lo{std::min(outer.x, inner.x), std::min(outer.y, inner.y)};
        Vec2i hi{std::max(outer.x, inner.x), std::max(outer.y, inner.y)};
        return c.x >= lo.x && c.y >= lo.y && c.x <= hi.x && c.y <= hi.y;
    }
};
