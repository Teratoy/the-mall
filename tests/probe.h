#pragma once
// What the tests may see and set of humans and the world that the game
// keeps to itself. SoakProbe is a friend of Human and World.
#include "human/human.h"
#include "world/world.h"
#include <deque>

struct SoakProbe {
    // Reading how walks go (stuck_soak.cpp).
    static const std::deque<Vec2i>& path(const Human& h) { return h.path_; }
    static Vec2i target(const Human& h) { return h.target_; }
    static int stuck(const Human& h) { return h.stuck_; }
    static bool rethink(const Human& h) { return h.rethink_; }
    static int partner(const Human& h) { return h.social_.partner; }
    static int leader(const Human& h) { return h.social_.leader; }
    static int foe(const Human& h) { return h.fight_.foe; }
    static bool ateFlesh(const Human& h) { return !h.mind_.ateFrom.empty(); }
    static int companion(const Human& h) { return h.social_.companion; }
    static float thirstOf(const Human& h) { return h.thirst; }
    static int keysOf(const Human& h) { return h.keys; }
    // What it could still go for (tools/lifelog.cpp): somewhere left to
    // explore, a locked door it knows, water it would drink, a toilet.
    static bool knowsFrontier(const World& w, const Human& h) {
        return h.search(w, [&](Vec2i c) { return (c != h.cell && h.isFrontier(c)) || h.openDoorBeside(c).has_value(); })
            .has_value();
    }
    static bool knowsLockedDoor(const World& w, const Human& h) {
        return h.search(w, [&](Vec2i c) { return h.lockedDoorBeside(c).has_value(); }).has_value();
    }
    static bool knowsWater(const World& w, const Human& h) {
        return h.search(w, [&](Vec2i c) { return h.waterBeside(c).has_value(); }).has_value();
    }
    static bool knowsToilet(const World& w, const Human& h) {
        return h.search(w, [&](Vec2i c) { return h.toiletBefore(w, c).has_value(); }).has_value();
    }

    // How chunks came out (tools/genprobe.cpp).
    static const std::vector<std::unique_ptr<Chunk>>& chunks(const World& w) { return w.chunks_; }
    static const std::unordered_map<EdgeKey, Door, EdgeKeyHash>& doors(const World& w) { return w.doors_; }

    // Setting up a situation (scenarios.cpp).
    static Human& addHuman(World& w, const HumanLook& look, Vec2i cell, uint64_t seed) { return w.addHuman(look, cell, seed); }
    /// What h thinks of o, -1 to 1, as if it had seen it all happen.
    static void setTrust(Human& h, const Human& o, float t) { h.social_.trust[o.id] = t; }
    /// Has met o, so it doesn't walk over just to greet them.
    static void setMet(Human& h, const Human& o, float at) {
        h.met.insert(o.id);
        h.social_.greetedAt[o.id] = at;
    }
    /// A real trap at the cell, armed and hidden.
    static void addTrap(World& w, Vec2i cell, TrapKind kind) { w.traps_[cell] = Trap{kind, cell, {0, 0}}; }
    /// Moves h to a free cell, as if it had come to there.
    static void place(World& w, Human& h, Vec2i cell) {
        w.moveOccupant(h.cell, cell, &h);
        h.cell = h.prev = h.target_ = cell;
        h.path_.clear();
    }
    /// h knows every tile of the chunk it stands in, doors and all, and
    /// with items the keys and cakes lying there.
    static void knowChunk(const World& w, Human& h, bool items = false) {
        Vec2i slot = World::slotOf(h.cell);
        for (int y = 0; y < CHUNK; y++)
            for (int x = 0; x < CHUNK; x++) {
                Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
                h.memory.see(c, w.tile(c));
                if (items && w.hasCake(c)) h.memory.cakes.insert(c);
                if (items && w.hasKey(c)) h.memory.keys.insert(c);
            }
    }
    /// A basin with its top left corner at the cell, as the world sets
    /// them. False if it doesn't fit there.
    static bool placeBasin(World& w, Vec2i cell) { return w.placeBasin(*w.chunkAt(World::slotOf(cell)), cell); }
    /// A cake on the cell, as if the world had made it there.
    static void addCake(World& w, Vec2i cell) { w.cakes_.insert(cell); }
    /// Takes every key and cake off the chunk the cell is in.
    static void clearChunk(World& w, Vec2i cell) {
        Vec2i slot = World::slotOf(cell);
        for (int y = 0; y < CHUNK; y++)
            for (int x = 0; x < CHUNK; x++) {
                Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
                w.takeCake(c);
                w.takeKey(c);
            }
    }
    /// Fresh blood on h's face and hands, as if from someone else.
    static void bloody(Human& h) { h.stain({BodyPart::Head, BodyPart::FrontArm, BodyPart::BackArm}, 40, 200); }
    /// Washed clean all at once.
    static void clean(Human& h) {
        h.stains.amount.fill(0);
        h.stains.wet.fill(0.0f);
        h.stains.version++;
    }
    /// Drops h dead where it stands, killed by nobody anyone saw, ago seconds
    /// since: long enough ago, bare bones.
    static void kill(World& w, Human& h, float ago = 0.0f) {
        h.health = 0.0f;
        h.die(w);
        h.deaths_.diedAt = h.clock_ - ago;
    }
    /// A slam room shut on h: a box of floor round it, walled, with its gate
    /// locked two cells off in dir, as if h had set it off. Returns the gate.
    static Vec2i shutInBox(World& w, Human& h, Vec2i dir) {
        for (int y = -2; y <= 2; y++)
            for (int x = -2; x <= 2; x++)
                w.setTile(h.cell + Vec2i{x, y}, std::max(std::abs(x), std::abs(y)) == 2 ? Tile::Wall : Tile::Floor);
        Vec2i gate = h.cell + dir * 2;
        w.setTile(gate, Tile::DoorLocked);
        w.gates_.insert(gate);
        h.shutIn(gate);
        return gate;
    }
    /// Shut in, h cries for help on its next update.
    static void cryNow(Human& h) { h.rescue_.helpTimer = 0.0f; }
    /// Kneels and puts down n cakes and these bottles where it stands.
    static void putDown(Human& h, int cakes, std::vector<size_t> bottles) { h.startPutting(cakes, std::move(bottles)); }
    /// The pile on the cell is a stash of h's, as if h had made it and seen
    /// it last as it is now.
    static void ownStash(const World& w, Human& h, Vec2i cell) {
        h.stash_.mine.push_back(cell);
        if (const Pile* p = w.pile(cell)) h.stash_.left[cell] = *p, h.memory.piles[cell] = *p;
    }
    /// h knows the pile on the cell is owner's stash, as if it had seen it made.
    static void knowStash(const World& w, Human& h, Vec2i cell, const Human& owner) {
        h.memory.stashes[cell] = owner.id;
        if (const Pile* p = w.pile(cell)) h.memory.piles[cell] = *p;
    }
    /// The same, having watched it being made.
    static void sawStashMade(const World& w, Human& h, Vec2i cell, const Human& owner) {
        knowStash(w, h, cell, owner);
        h.memory.stashSeenMade.insert(cell);
    }
    /// teller tells to where owner's stash at cell is, as at a greeting.
    static void tellStash(Human& teller, Human& to, Vec2i cell, const Human& owner) {
        Human::Tell t{Human::Tell::Kind::Stash, cell};
        t.owner = owner.id;
        teller.tell(to, t);
    }
    /// Stashes what it carries where it stands, as if that were its spot.
    static void stashHere(Human& h) {
        h.stash_.spot = h.cell;
        h.stashHere();
    }
    /// Given up on a way out, at home on home.
    static void settle(Human& h, Vec2i home) {
        h.escape_.settled = true;
        h.escape_.home = home;
        h.escape_.hope = 0.0f;
    }
};
