#include "world/gen.h"
#include "world/world.h"
#include <algorithm>

// Vending machines are uncommon: now and then one in a room network or a
// plaza, cake or bottle at even odds, sometimes two side by side. None in
// the spawn room or in mazes, and no gun machines (user). Each stands
// against a straight stretch of wall with floor all round the rest of it,
// so it blocks nothing: against a north wall, facing the viewer, or
// against a west or east wall, side on. Never against a south wall, where
// only its back would show. They are placed with dice of their own, so the
// rest of the mall comes out as it would without them.
//
// A data centre's corridors have more, mostly in pairs, each in an alcove
// cut one cell into the corridor wall, so the corridor keeps its width
// (user).

namespace {

/// Chance per slot of a room network or plaza that it has a machine (user).
constexpr float MACHINE_CHANCE = 1.0f / 3.0f;
/// Chance a machine stands with a second beside it.
constexpr float PAIR_CHANCE = 0.25f;
/// Items a machine is made with, of its six (user).
constexpr int STOCK_MIN = 2, STOCK_MAX = 6;
/// Tries at a spot for each.
constexpr int MACHINE_TRIES = 300;
/// A machine keeps this many cells from any door, and from a slam room's gate.
constexpr int MACHINE_DOOR_GAP = 3;
/// Rows of cells its body rises over: behind it against a north wall,
/// above it against a side wall.
constexpr int RISE_FRONT = 3, RISE_SIDE = 2;
/// The ways a machine may face: away from a north, a west and an east wall.
constexpr Vec2i FACINGS[] = {{0, 1}, {1, 0}, {-1, 0}};
/// Machine spots wanted per slot of a data centre (user: commoner there).
constexpr int CENTRE_MACHINES_PER_SLOT = 1;
/// Chance a data centre's machine stands with a second beside it (user: usually).
constexpr float CENTRE_PAIR_CHANCE = 0.75f;
/// Cells of solid wall behind an alcove, so the niche opens on nothing else
/// and the machine's body rises over wall.
constexpr int ALCOVE_BACK = 3;

bool sameFloor(Floor a, Floor b) { return a.pattern == b.pattern && a.palette == b.palette; }

} // namespace

void World::placeMachines(Chunk& c, const std::vector<gen::SlamRoom>& slams) {
    const bool centre = c.kind == ChunkKind::DataCentre;
    if (c.kind != ChunkKind::Rooms && c.kind != ChunkKind::Plaza && !centre) return;
    Rng dice(seed_ ^ uint64_t(c.id + 1) * 0x9E3779B97F4A7C15ull);
    int want = 0;
    if (centre)
        want = CENTRE_MACHINES_PER_SLOT * c.slotCount();
    else
        for (int i = 0; i < c.slotCount(); i++) want += dice.chance(MACHINE_CHANCE);
    // In a data centre only its corridors, laid in the chunk's own floor.
    auto floorAt = [&](Vec2i l) {
        return c.interior(l) && c.get(l) == Tile::Floor && !behindStatues_.count(c.origin() + l) &&
               (!centre || sameFloor(c.floor(l), c.paint));
    };
    auto wallAt = [&](Vec2i l) { return c.interior(l) && c.get(l) == Tile::Wall; };
    for (int t = 0; t < MACHINE_TRIES && want > 0; t++) {
        Vec2i l{dice.range(1, c.w - 2), dice.range(1, c.h - 2)};
        Vec2i facing = FACINGS[dice.range(0, 2)];
        int count = dice.chance(centre ? CENTRE_PAIR_CHANCE : PAIR_CHANCE) ? 2 : 1;
        Vend kinds[] = {Vend(dice.range(0, 1)), Vend(dice.range(0, 1))};
        Vec2i along = Machine{Vend::Cake, {}, facing}.along(), back = -facing;
        int length = 2 * count;
        // The cells it stands on, wall behind them and the cells past both
        // ends, floor at both ends and two deep in front.
        // In an alcove: its cells and both ends wall, solid behind, and
        // corridor floor in front of the alcove and two deep before it.
        bool fits = true;
        for (int i = -1; i <= length && fits; i++) {
            Vec2i p = l + along * i;
            bool ends = i < 0 || i >= length;
            if (centre) {
                fits = wallAt(p) && floorAt(p + facing) && (ends || floorAt(p + facing * 2));
                for (int d = 1; d <= ALCOVE_BACK && fits; d++) fits = wallAt(p + back * d);
            } else {
                fits = c.inside(p + back) && c.get(p + back) == Tile::Wall && floorAt(p) && floorAt(p + facing) &&
                       (ends || floorAt(p + facing * 2));
            }
            Vec2i w = c.origin() + p;
            fits = fits && !hasKey(w) && !hasCake(w) && !occupant(w) && !traps_.count(w) && !traps_.count(w + facing);
        }
        Vec2i end = l + along * (length - 1);
        int top = std::min(l.y, end.y) - MACHINE_DOOR_GAP, bottom = std::max(l.y, end.y) + MACHINE_DOOR_GAP;
        for (int y = top; y <= bottom && fits; y++)
            for (int x = std::min(l.x, end.x) - MACHINE_DOOR_GAP; x <= std::max(l.x, end.x) + MACHINE_DOOR_GAP; x++) {
                Vec2i p{x, y};
                if (c.inside(p) && (c.get(p) == Tile::DoorOpen || c.get(p) == Tile::DoorLocked)) fits = false;
            }
        for (const gen::SlamRoom& r : slams)
            for (int i = 0; i < length && fits; i++) {
                Vec2i p = l + along * i;
                fits = !r.contains(p) && !r.contains(p + facing) && cheb(p, r.gate) > MACHINE_DOOR_GAP;
            }
        if (!fits) continue;
        if (centre)
            for (int i = 0; i < length; i++) gen::carve(c, l + along * i);
        for (int i = 0; i < count; i++)
            standMachine(c, kinds[i], c.origin() + l + along * (2 * i), facing, dice.range(STOCK_MIN, STOCK_MAX));
        want--;
    }
}

bool World::standMachine(Chunk& c, Vend kind, Vec2i at, Vec2i facing, int stock) {
    Machine m{kind, at, facing, stock, MACHINE_DAY};
    int id = int(machines_.size());
    for (Vec2i cell : {at, at + m.along()})
        if (tile(cell) != Tile::Floor) return false;
    machines_.push_back(m);
    for (Vec2i cell : {at, at + m.along()}) {
        machineCells_[cell] = id;
        c.set(cell - c.origin(), Tile::Vending);
    }
    if (m.side())
        for (int y = 1; y <= RISE_SIDE; y++) behindStatues_.insert(at - Vec2i{0, y});
    else
        for (int y = 1; y <= RISE_FRONT; y++)
            for (Vec2i cell : {at, at + m.along()}) behindStatues_.insert(cell - Vec2i{0, y});
    return true;
}
