#pragma once
// What the tests may see and set of humans and the world that the game
// keeps to itself. SoakProbe is a friend of Human and World.
#include <optional>
#include "human/human.h"
#include "world/world.h"
#include <cstdio>
#include <cstdlib>
#include <deque>

struct SoakProbe {
    // Reading how walks go (stuck_soak.cpp).
    static const std::deque<Vec2i>& path(const Human& h) { return h.path_; }
    static Vec2i target(const Human& h) { return h.target_; }
    static int stuck(const Human& h) { return h.stuck_; }
    static bool rethink(const Human& h) { return h.rethink_; }
    static int partner(const Human& h) { return h.social_.partner; }
    static int leader(const Human& h) { return h.social_.leader; }
    static int companion(const Human& h) { return h.social_.companion; }
    static int foe(const Human& h) { return h.fight_.foe; }
    static bool ateFlesh(const Human& h) { return !h.mind_.ateFrom.empty(); }
    static float thirstOf(const Human& h) { return h.thirst; }
    static int keysOf(const Human& h) { return h.keys; }
    // What it could still go for (tools/lifelog.cpp): somewhere left to
    // explore, a locked door it knows, water it would drink, a toilet.
    // search() breaks ties with the human's dice, so they are put back.
    template <class F> static bool finds(const World& w, const Human& h, F done) {
        Rng saved = h.rng_;
        bool found = h.search(w, done).has_value();
        h.rng_ = saved;
        return found;
    }
    static bool knowsFrontier(const World& w, const Human& h) {
        return finds(w, h, [&](Vec2i c) { return (c != h.cell && h.isFrontier(c)) || h.openDoorBeside(c).has_value(); });
    }
    static bool knowsLockedDoor(const World& w, const Human& h) {
        return finds(w, h, [&](Vec2i c) { return h.lockedDoorBeside(c).has_value(); });
    }
    static bool knowsWater(const World& w, const Human& h) {
        return finds(w, h, [&](Vec2i c) { return h.waterBeside(c).has_value(); });
    }
    static bool knowsToilet(const World& w, const Human& h) {
        return finds(w, h, [&](Vec2i c) { return h.toiletBefore(w, c).has_value(); });
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
    /// Keeps with o, as if it had taken up with them.
    static void setCompanion(Human& h, const Human& o) { h.social_.companion = o.id; }
    /// A real trap at the cell, armed and hidden.
    static void addTrap(World& w, Vec2i cell, TrapKind kind) { w.traps_[cell] = Trap{kind, cell, {0, 0}}; }
    /// Moves h to a free cell, as if it had come to there.
    static void place(World& w, Human& h, Vec2i cell) {
        w.moveOccupant(h.cell, cell, &h);
        h.cell = h.prev = h.target_ = cell;
        h.path_.clear();
    }
    /// Which chunk the cell is in; a chunk may cover several slots.
    static int chunkOf(const World& w, Vec2i cell) { return w.chunkAt(World::slotOf(cell))->id; }
    /// h knows every tile of the chunk it stands in, doors and all, and
    /// with items the keys and cakes lying there.
    static void knowChunk(const World& w, Human& h, bool items = false) { knowChunkOf(w, h, h.cell, items); }
    /// The same for the chunk the cell is in.
    static void knowChunkOf(const World& w, Human& h, Vec2i cell, bool items = false) {
        Vec2i slot = World::slotOf(cell);
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
    /// A statue of this kind about the cell, kept clear of the human; false if none fits.
    static bool placeStatue(World& w, Vec2i cell, const Human& h, StatueKind kind) {
        Chunk& c = *w.chunkAt(World::slotOf(cell));
        return w.placeStatue(c, cell - c.origin(), h.cell - c.origin(), false, kind);
    }
    /// Which remarks on the room through the fog it has made, a bit each
    /// from talk::Moment::FirstStatue.
    static unsigned fogSights(const Human& h) { return h.escape_.fogSights; }
    /// As if it had already set foot in n other places (ids no chunk has).
    static void beenIn(Human& h, int n) {
        for (int i = 0; i < n; i++) h.escape_.chunksIn.insert(-100 - i);
    }
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
    /// Gone unwashed this long, 1 being long enough to turn dirty.
    static void setGrime(Human& h, float level) { h.grime_.level = level; }
    /// The blood on h dried.
    static void dry(Human& h) {
        h.stains.wet.fill(0.0f);
        h.stains.version++;
    }
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
    /// h keeps company with o, as if it had chosen to.
    static void keepWith(Human& h, const Human& o) { h.social_.companion = o.id; }
    /// h's mind has snapped.
    static void unhinge(Human& h) { h.mind_.erratic = 1.0f; }
    /// A walled box round a cell, five across, floor inside; no way in,
    /// unless a gap in its wall at at + gap.
    static void wallIn(World& w, Vec2i at, std::optional<Vec2i> gap = std::nullopt) {
        for (int y = -2; y <= 2; y++)
            for (int x = -2; x <= 2; x++)
                w.setTile(at + Vec2i{x, y}, std::max(std::abs(x), std::abs(y)) == 2 ? Tile::Wall : Tile::Floor);
        if (gap) w.setTile(at + *gap, Tile::Floor);
    }
    /// Gone erratic, with this taste for flesh.
    static void craze(Human& h, float appetite) {
        h.mind_.erratic = 1.0f;
        h.mind_.appetite = appetite;
    }
    /// Shut in, h cries for help on its next update.
    static void cryNow(Human& h) { h.rescue_.helpTimer = 0.0f; }
    /// Nothing lies put down anywhere, and h knows of nothing lying: the
    /// bottle it woke near is gone, as if never laid.
    static void clearPiles(World& w, Human& h) {
        w.piles_.clear();
        h.memory.piles.clear();
        h.water_.found.reset();
    }
    /// The bottle h found lying while it had none, if any.
    static std::optional<Vec2i> foundBottle(const Human& h) { return h.water_.found; }
    /// h has drunk somewhere, so it knows water to fill a bottle at.
    static void setDrankYet(Human& h) { h.water_.drankYet = true; }
    /// h has seen that water is scarce already.
    static void setScarce(Human& h) { h.water_.scarce = true; }
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
    /// Turns the cell into a basin's worth of water.
    static void addWater(World& w, Vec2i cell) { w.setTile(cell, Tile::Basin); }
    /// Wall on the cell, as if the chunk had been dug that way.
    static void addWall(World& w, Vec2i cell) { w.setTile(cell, Tile::Wall); }
    /// h tasted the water at the cell this foul (with feces and piss) before.
    static void tastedFoul(Human& h, Vec2i water, float filth) { h.water_.foul[water] = {filth, 0.0f}; }
    /// Wary of cakes on tables after darts, and of rooms with one door
    /// after a slam, as if caught that much (Human::Wary).
    static void setWary(Human& h, float darts, float slam) {
        h.wary_.darts = darts;
        h.wary_.slam = slam;
    }
    /// A vending machine of the kind with its first cell at the cell,
    /// facing away from a short wall put up behind it, holding stock items.
    static const Machine& standMachine(World& w, Vend kind, Vec2i at, Vec2i facing, int stock) {
        Machine shape{kind, at, facing};
        for (int i = -1; i <= 2; i++) w.setTile(at + shape.along() * i - facing, Tile::Wall);
        if (!w.standMachine(*w.chunkAt(World::slotOf(at)), kind, at, facing, stock)) {
            std::fprintf(stderr, "probe: no floor for a machine at %d,%d\n", at.x, at.y);
            std::abort();
        }
        return w.machines_.back();
    }
    /// h knows the machine and what is in it, as if it had seen it.
    static void knowMachine(Human& h, const Machine& m) {
        h.memory.machines[m.at] = {m.kind, m.stock};
        h.memory.see(m.at, Tile::Vending);
        h.memory.see(m.at + m.along(), Tile::Vending);
    }
    /// Standing in front of a machine of the kind it knows, h starts working it.
    static bool startVending(World& w, Human& h, Vend kind) {
        h.vend_.kind = kind;
        return h.startVending(w);
    }
    /// Given up on a way out, at home on home.
    static void settle(Human& h, Vec2i home) {
        h.escape_.settled = true;
        h.escape_.home = home;
        h.escape_.hope = 0.0f;
    }
};
