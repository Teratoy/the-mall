#pragma once
#include "world/blood.h"
#include "world/chunk.h"
#include "world/pile.h"
#include "world/trap.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// What one human has seen of the world: the tile last seen in each cell,
/// where keys and cakes were lying, the traps it found out about, and the
/// blood it saw, as it looked then.
/// Humans plan on this, never on the world.
class Memory {
public:
    /// Void where the human has never looked.
    Tile tile(Vec2i cell) const {
        auto it = slots_.find(slotOf(cell));
        return it == slots_.end() ? Tile::Void : it->second[index(cell)];
    }

    void see(Vec2i cell, Tile t) {
        auto& slot = slots_[slotOf(cell)];
        if (slot.empty()) slot.assign(CHUNK * CHUNK, Tile::Void);
        slot[index(cell)] = t;
        if (!hearsay.empty()) hearsay.erase(cell);
    }
    /// Learns a cell's tile from someone else: it is planned on all the
    /// same, but unless it saw the cell itself, it stays in hearsay until seen.
    void hear(Vec2i cell, Tile t) {
        bool unseen = tile(cell) == Tile::Void || hearsay.count(cell);
        see(cell, t);
        if (unseen) hearsay.insert(cell);
    }

    /// Cells whose tiles it only has on someone's word: the way they told
    /// it, or the way they led it. Shown as hearsay on its map.
    std::unordered_set<Vec2i, Vec2iHash> hearsay;

    /// The names of the humans that told it theirs, by id. A name is only
    /// known by being told: humans choose whether to give theirs, so one may
    /// have met many and know few names. Someone may pass a name on later.
    std::unordered_map<int, std::string> names;
    /// What each teller last said of each third human, -1 to 1, keyed by
    /// gossipKey: so hearing the same warning twice from one friend counts
    /// once, while two friends warning count twice (Human::gossipFor).
    std::unordered_map<uint64_t, float> heard;
    static uint64_t gossipKey(int teller, int about) { return uint64_t(uint32_t(teller)) << 32 | uint32_t(about); }
    /// The piss and feces it has seen, as indexes into World::mess().
    std::unordered_set<int> mess;
    std::unordered_set<Vec2i, Vec2iHash> keys;
    std::unordered_set<Vec2i, Vec2iHash> cakes;
    /// What it saw lying put down on the floor, as it was then.
    std::unordered_map<Vec2i, Pile, Vec2iHash> piles;
    /// Traps by the cell that sets them off, learnt by seeing one go off,
    /// spotting it nearby, or being told of it at a greeting (Human::tell):
    /// what a human knows of traps is worth something to others, and the
    /// greedy sell it for a cake while the cunning keep it from rivals.
    std::unordered_map<Vec2i, TrapKind, Vec2iHash> traps;
    /// Who told it what lies in a cell, by cell, for what it was told of
    /// rather than saw: a cake, key or trap, or the cells along a route.
    /// When the cell turns out otherwise, nothing there or a trap where the
    /// way was said to be clear, the teller was wrong or lying and is
    /// thought worse of. Cleared once seen to be true.
    std::unordered_map<Vec2i, int, Vec2iHash> toldBy;
    /// Who told it of a cake, by its cell, kept after the cake is seen: if
    /// the table turns out rigged when it takes it, it blames them.
    std::unordered_map<Vec2i, int, Vec2iHash> cakeTips;
    std::unordered_map<Vec2i, Stain, Vec2iHash> blood;
    /// Pools of blood seen, and the cells on and round them, which it keeps
    /// off when it can.
    std::unordered_set<Vec2i, Vec2iHash> pools;
    std::unordered_set<Vec2i, Vec2iHash> shunned;

private:
    std::unordered_map<Vec2i, std::vector<Tile>, Vec2iHash> slots_;

    static Vec2i slotOf(Vec2i c) { return {floorDiv(c.x, CHUNK), floorDiv(c.y, CHUNK)}; }
    static size_t index(Vec2i c) {
        Vec2i l = c - slotOf(c) * CHUNK;
        return size_t(l.y * CHUNK + l.x);
    }
};
