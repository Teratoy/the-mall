#include "world/world.h"
#include <vector>

// Every server room, closets too, has one water dispenser against its back
// wall or a side wall (user), never by the door or on a cable, and with
// floor all round the rest of it, so it blocks nothing. It starts with one
// to three thirst bars of water (user). It is placed with dice of its own,
// so the rest of the room comes out as it would without it.

namespace {

/// Water a dispenser is made with, in thirst bars (user).
constexpr float START_MIN = 1.0f, START_MAX = DISPENSER_HOLDS;
/// It keeps this many cells from the inner door.
constexpr int DOOR_GAP = 2;
/// Rows its jug rises over above its own cell, against a side wall: kept
/// clear like those behind statues.
constexpr int RISE_SIDE = 2;
/// The ways a dispenser may face: away from a north, a west and an east wall.
constexpr Vec2i FACINGS[] = {{0, 1}, {1, 0}, {-1, 0}};

} // namespace

void World::placeDispenser(Chunk& c, const ServerRoom& r, Vec2i door,
                           const std::unordered_set<Vec2i, Vec2iHash>& cables) {
    const Vec2i o = c.origin();
    auto floorAt = [&](Vec2i w) { return c.get(w - o) == Tile::Floor; };
    std::vector<std::pair<Vec2i, Vec2i>> spots;
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++) {
            Vec2i w{x, y};
            for (Vec2i facing : FACINGS) {
                if (!r.contains(w) || r.contains(w - facing) || c.get(w - facing - o) != Tile::Wall) continue;
                if (!floorAt(w) || cables.count(w) || cheb(w, door) <= DOOR_GAP) continue;
                bool open = true;
                for (int dy = -1; dy <= 1 && open; dy++)
                    for (int dx = -1; dx <= 1 && open; dx++) {
                        Vec2i n = w + Vec2i{dx, dy};
                        if (r.contains(n)) open = floorAt(n);
                    }
                if (open) spots.push_back({w, facing});
            }
        }
    if (spots.empty()) return;
    Rng dice(seed_ ^ (uint64_t(uint32_t(r.x)) << 32 | uint32_t(r.y)) * 0xD1B54A32D192ED03ull);
    auto [at, facing] = spots[size_t(dice.range(0, int(spots.size()) - 1))];
    dispenserCells_[at] = int(dispensers_.size());
    dispensers_.push_back({at, facing, dice.range(START_MIN, START_MAX)});
    c.set(at - o, Tile::Dispenser);
    if (facing.x != 0)
        for (int y = 1; y <= RISE_SIDE; y++) behindStatues_.insert(at - Vec2i{0, y});
}
