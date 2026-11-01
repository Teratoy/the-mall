#pragma once
#include "world/blood.h"
#include "world/chunk.h"
#include "world/pile.h"
#include "world/server_room.h"
#include "world/smell.h"
#include "world/trap.h"
#include "world/vending.h"
#include <algorithm>
#include <array>
#include <optional>
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
        Vec2i slot = slotOf(cell);
        if (slot != last_.slot || !last_.tiles) {
            auto it = slots_.find(slot);
            if (it == slots_.end()) return Tile::Void;
            last_.slot = slot;
            last_.tiles = it->second.data();
        }
        return last_.tiles[index(cell)];
    }

    void see(Vec2i cell, Tile t) {
        auto& slot = slots_[slotOf(cell)];
        if (slot.empty()) slot.assign(CHUNK * CHUNK, Tile::Void);
        Tile& was = slot[index(cell)];
        known_[size_t(was)]--;
        known_[size_t(t)]++;
        was = t;
        if (!hearsay.empty()) hearsay.erase(cell);
    }
    /// How lit a cell was when it last saw it, to draw it so; the sim
    /// doesn't plan on it yet.
    struct SeenLight {
        int level;
        LightKind from;
    };
    void seeLight(Vec2i cell, int level, LightKind from) {
        auto& slot = lightSlots_[slotOf(cell)];
        if (slot.empty()) slot.assign(CHUNK * CHUNK, 0);
        slot[index(cell)] = uint8_t(1 + level + (int(from) << 2));
    }
    /// Nothing for a cell it has never seen lit or dark, only heard of.
    std::optional<SeenLight> lightSeen(Vec2i cell) const {
        auto it = lightSlots_.find(slotOf(cell));
        if (it == lightSlots_.end()) return {};
        uint8_t v = it->second[index(cell)];
        if (v == 0) return {};
        return SeenLight{(v - 1) & 3, LightKind((v - 1) >> 2)};
    }

    /// Learns a cell's tile from someone else: it is planned on all the
    /// same, but unless it saw the cell itself, it stays in hearsay until seen.
    void hear(Vec2i cell, Tile t) {
        bool unseen = tile(cell) == Tile::Void || hearsay.count(cell);
        see(cell, t);
        if (unseen) hearsay.insert(cell);
    }

    /// How many cells it knows to be of this tile: none, and nothing it
    /// looks for of that tile can be found.
    int known(Tile t) const { return known_[size_t(t)]; }

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
    /// A vending machine as it last saw it, or was told of it: what it
    /// sells, how many items were behind the glass, and when, by its own
    /// clock.
    struct SeenMachine {
        Vend kind = Vend::Cake;
        int stock = 0;
        float seenAt = 0.0f;
    };
    /// The machines it knows, by their first cell.
    std::unordered_map<Vec2i, SeenMachine, Vec2iHash> machines;
    /// The airlock doors of server rooms it has seen, by cell, and the way
    /// into the room through each: from that side a door opens at a push,
    /// from the other only with a key.
    std::unordered_map<Vec2i, Vec2i, Vec2iHash> airlocks;
    /// The airlock doors it has pushed open from inside, so knows give
    /// from there (AI/door_trying_plan.md).
    std::unordered_set<Vec2i, Vec2iHash> pushes;
    /// The server rooms it has seen the machines of, by their inner door:
    /// where the warm hum is, kept for later uses (user).
    std::unordered_map<Vec2i, ServerRoom, Vec2iHash> serverRooms;
    /// The water dispensers it knows, by cell, and how much water it saw in
    /// each through the jug (or was told of): one only heard of, with no
    /// level, is taken to hold some.
    std::unordered_map<Vec2i, float, Vec2iHash> dispensers;
    /// What it saw lying put down on the floor, as it was then; and, of
    /// those, the ones it knows are someone's stash, and whose: seen being
    /// stashed, or told of.
    std::unordered_map<Vec2i, Pile, Vec2iHash> piles;
    std::unordered_map<Vec2i, int, Vec2iHash> stashes;
    /// Stashes it was told of and hasn't seen yet, and who told it; and
    /// those it saw being made, which nobody can talk it out of.
    std::unordered_map<Vec2i, int, Vec2iHash> stashTips;
    std::unordered_set<Vec2i, Vec2iHash> stashSeenMade;
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
    /// A smell it smelt, seen or not, as strong as it was, and the clock
    /// time it last smelt it there.
    struct Whiff {
        SmellKind kind;
        float strength;
        int who;
        float at;
    };
    /// The smells it knows of, by cell (Human::sniff).
    std::unordered_map<Vec2i, Whiff, Vec2iHash> smells;
    /// The cells on and next to the smells it knows of, and the strongest
    /// smell there: what walking or stopping there is like.
    std::unordered_map<Vec2i, float, Vec2iHash> reek;
    void rebuildReek() {
        reek.clear();
        for (const auto& [c, w] : smells)
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++) {
                    float& r = reek[c + Vec2i{x, y}];
                    r = std::max(r, w.strength);
                }
    }

private:
    std::unordered_map<Vec2i, std::vector<Tile>, Vec2iHash> slots_;
    /// How lit each cell was when last seen, 0 where never seen, else 1 +
    /// its level + its source's kind times 4.
    std::unordered_map<Vec2i, std::vector<uint8_t>, Vec2iHash> lightSlots_;
    /// Cells by the tile it knows them to be (Void's count runs negative).
    std::array<int, 256> known_{};
    /// The slot last looked up, kept at hand since searches ask for cell
    /// after cell in one slot. Only slots it holds are kept, and a slot's
    /// tiles never move once made; a copy starts with nothing at hand.
    struct LastSlot {
        Vec2i slot{};
        const Tile* tiles = nullptr;
        LastSlot() = default;
        LastSlot(const LastSlot&) {}
        LastSlot& operator=(const LastSlot& o) {
            if (this != &o) tiles = nullptr;
            return *this;
        }
    };
    mutable LastSlot last_;

    static Vec2i slotOf(Vec2i c) { return {floorDiv(c.x, CHUNK), floorDiv(c.y, CHUNK)}; }
    static size_t index(Vec2i c) {
        Vec2i l = c - slotOf(c) * CHUNK;
        return size_t(l.y * CHUNK + l.x);
    }
};
