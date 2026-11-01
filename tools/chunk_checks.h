#pragma once
// What tools/genprobe.cpp checks in a chunk just grown in a world, and
// tools/genview.cpp shows: what can be reached from where it was entered,
// and what is wrong when something can't.
#include "../tests/probe.h"
#include <string>
#include <vector>

/// What a chunk was grown with, added up over chunks.
struct Tally {
    int chunks = 0, doors = 0, keys = 0, cakes = 0, machines = 0, servers = 0;
    /// Cakes only water keeps from the entry.
    int overWater = 0;
    double slowest = 0.0;
};

/// Every cell of the chunk walkable from the entry, locked doors and gates
/// as if open; and with swim, what only water cuts off, which another door
/// may reach. Indexed y * c.w + x.
inline std::vector<bool> reach(const Chunk& c, Vec2i entry, bool swim) {
    std::vector<bool> reached(size_t(c.w * c.h), false);
    std::vector<Vec2i> stack{entry - c.origin()};
    reached[size_t(stack[0].y * c.w + stack[0].x)] = true;
    while (!stack.empty()) {
        Vec2i p = stack.back();
        stack.pop_back();
        for (Vec2i d : DIRS4) {
            Vec2i n = p + d;
            if (!c.owns(n) || reached[size_t(n.y * c.w + n.x)]) continue;
            Tile k = c.get(n);
            if (!passable(k) && k != Tile::DoorLocked && !(swim && k == Tile::Water)) continue;
            reached[size_t(n.y * c.w + n.x)] = true;
            stack.push_back(n);
        }
    }
    return reached;
}

/// What is wrong with a chunk just grown in a world, entered at entry,
/// counted in its tally: a door, key, machine or server room cell that
/// can't be reached. Empty when nothing is.
inline std::string check(const World& w, const Chunk& c, Vec2i entry, Tally& t) {
    using P = SoakProbe;
    const std::vector<bool> walked = reach(c, entry, false), swum = reach(c, entry, true);
    // A key or cake on a table is reached from beside it.
    auto near = [&](const std::vector<bool>& reached, Vec2i cell) {
        Vec2i l = cell - c.origin();
        if (reached[size_t(l.y * c.w + l.x)]) return true;
        for (Vec2i d : DIRS8)
            if (c.owns(l + d) && reached[size_t((l + d).y * c.w + l.x + d.x)]) return true;
        return false;
    };
    std::string why;
    for (const auto& [key, d] : P::doors(w)) {
        Vec2i side = c.covers(d.slot) ? d.slot : d.other();
        if (!c.covers(side) || w.generated(d.slot) + w.generated(d.other()) != 1) continue;
        t.doors++;
        if (!near(walked, d.cellsIn(side)[0])) why = "a new door out of reach";
    }
    for (Vec2i k : w.keys())
        if (w.chunkIdAt(k) == c.id) {
            t.keys++;
            if (!near(walked, k)) why = "a key out of reach";
        }
    for (Vec2i k : w.cakes())
        if (w.chunkIdAt(k) == c.id) {
            t.cakes++;
            if (!near(walked, k)) t.overWater++;
            if (!near(swum, k)) why = "a cake sealed off";
        }
    // A machine stands with wall behind both cells and is worked from
    // floor that can be reached, as a cake can, maybe from another door.
    for (const Machine& m : w.machines())
        if (w.chunkIdAt(m.at) == c.id) {
            t.machines++;
            bool reached = false;
            for (Vec2i cell : {m.at, m.at + m.along()}) {
                if (w.tile(cell) != Tile::Vending || w.tile(cell - m.facing) != Tile::Wall)
                    why = "a machine not against a wall";
                Vec2i front = cell + m.facing - c.origin();
                reached |= c.owns(front) && swum[size_t(front.y * c.w + front.x)];
            }
            if (m.facing.y < 0) why = "a machine with its back to the viewer";
            if (!reached) why = "a machine sealed off";
            // A data centre's stands in an alcove, wall at both its ends,
            // so the corridor keeps its width.
            if (c.kind == ChunkKind::DataCentre &&
                (w.tile(m.at - m.along()) != Tile::Wall || w.tile(m.at + m.along() * 2) != Tile::Wall) &&
                !w.machine(m.at - m.along()) && !w.machine(m.at + m.along() * 2))
                why = "a data centre machine out of its alcove";
        }
    // Every cell of a server room's floor can be walked to through its
    // airlock: no row of racks or desk walls any of it off.
    for (const ServerRoom& r : w.serverRooms())
        if (w.chunkIdAt({r.x, r.y}) == c.id) {
            t.servers++;
            for (int y = r.y; y < r.y + r.h; y++)
                for (int x = r.x; x < r.x + r.w; x++) {
                    Vec2i l = Vec2i{x, y} - c.origin();
                    if (passable(c.get(l)) && !swum[size_t(l.y * c.w + l.x)]) why = "a server room cell walled off";
                }
        }
    return why;
}

