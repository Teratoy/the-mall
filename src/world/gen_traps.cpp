#include "world/gen.h"
#include "world/gen_tuning.h"

namespace gen {

namespace {

/// Floor size of a slam room, each side.
tune::Int SLAM_ROOM_MIN{"traps", "SLAM_ROOM_MIN"}, SLAM_ROOM_MAX{"traps", "SLAM_ROOM_MAX"};
/// Longest corridor from a slam room's gate to the floor it opens onto.
tune::Int SLAM_CORRIDOR_MAX{"traps", "SLAM_CORRIDOR_MAX"};
tune::Int SLAM_TRIES{"traps", "SLAM_TRIES"};
/// How far a dart shooter may sit from its plate, in cells.
tune::Int DART_RANGE_MIN{"traps", "DART_RANGE_MIN"}, DART_RANGE_MAX{"traps", "DART_RANGE_MAX"};

bool solid(const Chunk& c, int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (!c.interior({xx, yy}) || c.get({xx, yy}) != Tile::Wall) return false;
    return true;
}

} // namespace

bool SlamRoom::contains(Vec2i l) const { return l.x >= x && l.y >= y && l.x < x + w && l.y < y + h; }

// A box of solid wall, its walls included, gets a floor of its own and a
// gate in the middle of one wall, with a straight corridor out of it to
// the nearest floor that way. The plate is a cell off the far wall, with
// room all round it for the bait's table, so whoever reaches for the bait
// is well inside.
std::vector<SlamRoom> slamRooms(Chunk& c, Rng& rng, int count) {
    std::vector<SlamRoom> made;
    const Floor paint = c.paint;
    for (int t = 0; t < SLAM_TRIES * count && int(made.size()) < count; t++) {
        int w = rng.range(SLAM_ROOM_MIN, SLAM_ROOM_MAX), h = rng.range(SLAM_ROOM_MIN, SLAM_ROOM_MAX);
        int x = rng.range(2, c.w - 2 - w), y = rng.range(2, c.h - 2 - h);
        if (!solid(c, x - 1, y - 1, w + 2, h + 2)) continue;
        Vec2i d = rng.pick(DIRS4);
        Vec2i mid{x + w / 2, y + h / 2};
        Vec2i gate = d.x > 0 ? Vec2i{x + w, mid.y}
                   : d.x < 0 ? Vec2i{x - 1, mid.y}
                   : d.y > 0 ? Vec2i{mid.x, y + h}
                             : Vec2i{mid.x, y - 1};
        std::vector<Vec2i> corridor;
        bool reached = false;
        for (Vec2i p = gate + d; c.interior(p) && int(corridor.size()) <= SLAM_CORRIDOR_MAX; p = p + d) {
            if (c.get(p) == Tile::Floor) {
                reached = true;
                break;
            }
            if (c.get(p) != Tile::Wall) break;
            corridor.push_back(p);
        }
        // At least one corridor cell, so walking straight through the gate
        // lands on floor either way.
        if (!reached || corridor.empty()) continue;
        c.paint = randomFloor(rng);
        carveRect(c, x, y, w, h);
        c.paint = paint;
        for (Vec2i p : corridor) carve(c, p);
        c.set(gate, Tile::DoorOpen);
        made.push_back({gate, gate - d * ((d.x != 0 ? w : h) - 1), x, y, w, h});
    }
    return made;
}

std::optional<std::pair<Vec2i, Vec2i>> dartShooter(const Chunk& c, Rng& rng, Vec2i plate) {
    std::vector<std::pair<Vec2i, Vec2i>> found;
    for (Vec2i d : DIRS4)
        for (int i = 1; i <= DART_RANGE_MAX; i++) {
            Vec2i p = plate + d * i;
            if (!c.inside(p)) break;
            Tile t = c.get(p);
            if (t == Tile::Wall) {
                if (i >= DART_RANGE_MIN) found.push_back({p, -d});
                break;
            }
            if (t != Tile::Floor && !pooled(t) && t != Tile::Table) break;
        }
    if (found.empty()) return std::nullopt;
    return rng.pick(found);
}

bool openAround(const Chunk& c, Vec2i p) {
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            if (!c.inside(p + Vec2i{x, y}) || c.get(p + Vec2i{x, y}) != Tile::Floor) return false;
    return true;
}

} // namespace gen
