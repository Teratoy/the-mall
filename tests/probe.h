#pragma once
// What the tests may see and set of humans and the world that the game
// keeps to itself. SoakProbe is a friend of Human and World.
#include <optional>
#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <tuple>
#include <utility>

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
    static void setLungs(Human& h, float seconds) { h.run_.lungs = seconds; }
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
    /// How many steps to the nearest water it would drink, -1 none known (tools/trips.cpp).
    static int waterSteps(const World& w, const Human& h) {
        Rng saved = h.rng_;
        auto p = h.search(w, [&](Vec2i c) {
            auto water = h.waterBeside(c);
            return water && h.wouldDrink(w, c, *water);
        });
        h.rng_ = saved;
        return p ? int(p->size()) : -1;
    }
    static bool besideWater(const Human& h) { return h.waterBeside(h.cell).has_value(); }
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
    /// A toilet on the cell, used from front, already this dirty and this
    /// much of it piss.
    static void placeToilet(World& w, Vec2i at, Vec2i front, float dirt = 0.0f, float piss = 0.0f) {
        w.setTile(at, Tile::Toilet);
        w.toilets_[at] = {front, dirt, piss, 7};
    }
    /// A statue of this kind about the cell, kept clear of the human; false if none fits.
    static bool placeStatue(World& w, Vec2i cell, const Human& h, StatueKind kind) {
        Chunk& c = *w.chunkAt(World::slotOf(cell));
        const World::Glowing before = w.glowing();
        bool placed = w.placeStatue(c, cell - c.origin(), h.cell - c.origin(), false, kind);
        w.lightPlaced(before);
        return placed;
    }
    /// Which remarks on the room through the fog it has made, a bit each
    /// from talk::Moment::FirstStatue.
    static unsigned fogSights(const Human& h) { return h.escape_.fogSights; }
    /// As if it had seen one of everything worth a look before: nothing
    /// pulls it aside to look (human_inspect.cpp).
    static void seenItAll(Human& h) {
        h.firstSights_ = ~0u;
        h.escape_.fogSights |= 1u;
        h.vend_.seen = h.server_.seen = true;
        h.inspect_.body = h.inspect_.pool = h.inspect_.trap = h.inspect_.bottle = true;
        h.computers_.terminal = h.computers_.desk = true;
        h.computers_.tries = 64;
    }
    /// A terminal or console desk stood with its cell (the desk's top-left) at at.
    /// Every light in the cell's chunk put out, as in a dark chunk.
    static void darkenChunk(World& w, Vec2i cell) {
        const Chunk& c = *w.chunkAt(World::slotOf(cell));
        for (LightSource& l : w.lights_)
            if (w.chunkAt(World::slotOf(l.cell)) == &c) l.on = false;
        w.relightAround(c.origin(), c.origin() + Vec2i{c.w - 1, c.h - 1});
    }
    /// A ceiling fitting hung over the cell, lit, and the light round it worked out again.
    static void hangLight(World& w, Vec2i cell) {
        w.addLight(cell, LightKind::Fitting);
        w.relightAround(cell, cell);
    }
    static void standComputer(World& w, Rig kind, Vec2i at) {
        const World::Glowing before = w.glowing();
        w.standPiece(*w.chunkAt(World::slotOf(at)), kind, at, 1);
        w.lightPlaced(before);
    }
    /// h tried this computer earlier, and was turned away.
    static void triedComputer(Human& h, Vec2i at, Rig kind) {
        h.computers_.seen[at] = Human::Computers::Try::Tried;
        (kind == Rig::Terminal ? h.computers_.terminal : h.computers_.desk) = true;
        h.computers_.tries++;
    }
    /// h has already made up its mind to try this computer when it next sees it.
    static void meanToTry(Human& h, Vec2i at) { h.computers_.seen[at] = Human::Computers::Try::Meant; }
    /// What it has kept to go and look at.
    static size_t findsKept(const Human& h) { return h.inspect_.queue.size() + (h.inspect_.going ? 1 : 0); }
    /// As if it had already set foot in n other places (ids no chunk has).
    /// Drops asleep where it is, worn out, asking nobody to keep watch.
    static void collapse(World& w, Human& h) {
        h.tiredness = 0.95f;
        h.startSleeping(w, true);
    }
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
    /// Last drank at this cell (AI/needs_planning_plan.md).
    static void setLastWater(Human& h, Vec2i at) {
        h.water_.drankYet = true;
        h.water_.last = at;
    }
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
        const World::Glowing before = w.glowing();
        if (!w.standMachine(*w.chunkAt(World::slotOf(at)), kind, at, facing, stock)) {
            std::fprintf(stderr, "probe: no floor for a machine at %d,%d\n", at.x, at.y);
            std::abort();
        }
        w.lightPlaced(before);
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
    /// A server room of rw x rh floor with its top-left at at, walled, its
    /// airlock below it in the middle (inner door, two cells of lobby,
    /// outer door) and grown inside as generation grows them. The cell
    /// below the outer door must be floor.
    static const ServerRoom& buildServerRoom(World& w, Vec2i at, int rw, int rh) {
        const int cx = at.x + rw / 2;
        for (int y = -1; y <= rh + 3; y++)
            for (int x = -1; x <= rw; x++) w.setTile(at + Vec2i{x, y}, Tile::Wall);
        for (int y = 0; y < rh; y++)
            for (int x = 0; x < rw; x++) w.setTile(at + Vec2i{x, y}, Tile::Floor);
        const Vec2i inner{cx, at.y + rh}, outer{cx, at.y + rh + 3}, in{0, -1};
        for (Vec2i c : {inner - in, inner - in * 2}) w.setTile(c, Tile::Floor);
        for (Vec2i door : {inner, outer}) {
            w.setTile(door, Tile::DoorLocked);
            w.airlocks_[door] = {in};
        }
        w.serverRooms_.push_back({at.x, at.y, rw, rh, outer, inner});
        Rng dice(1);
        const ServerRoom r = w.serverRooms_.back();
        const World::Glowing before = w.glowing();
        w.furnishServerRoom(*w.chunkAt(World::slotOf(at)), dice, r, inner + in);
        // Only its machines light it, as generation has it.
        for (LightSource& l : w.lights_)
            if ((l.kind == LightKind::Fitting || l.kind == LightKind::NightLight) && r.encloses(l.cell)) l.on = false;
        w.relightAround(at - Vec2i{1, 1}, at + Vec2i{rw, rh + 3});
        w.lightPlaced(before);
        return w.serverRooms_.back();
    }
    /// h knows the server room and its doors, as if it had seen them.
    static void knowServerRoom(Human& h, const ServerRoom& r) {
        h.memory.serverRooms[r.inner] = r;
        for (Vec2i door : {r.inner, r.outer}) {
            h.memory.airlocks[door] = {0, -1};
            h.memory.see(door, Tile::DoorLocked);
        }
        h.server_.seen = true;
    }
    /// h has met the airlock this many times, and so may know what the
    /// double doors cost (AI/server_room_keys_plan.md).
    static void metAirlock(Human& h, int times) {
        h.server_.airlocksMet = times;
        h.server_.keyWise = times >= 2;
    }
    static bool keyWise(const Human& h) { return h.server_.keyWise; }
    /// The water dispenser grown in the server room, set to hold this much
    /// water. Aborts if the room grew none.
    static const Dispenser& fillDispenser(World& w, const ServerRoom& r, float water) {
        for (Dispenser& d : w.dispensers_)
            if (r.contains(d.at)) {
                d.water = water;
                return d;
            }
        std::fprintf(stderr, "probe: no dispenser in the server room at %d,%d\n", r.x, r.y);
        std::abort();
    }
    /// h knows the dispenser and how full it is, as if it had seen it.
    static void knowDispenser(Human& h, const Dispenser& d) {
        h.memory.dispensers[d.at] = d.water;
        h.memory.see(d.at, Tile::Dispenser);
        h.dispense_.seen = true;
    }
    /// h has run a bottle dry once and learnt that water is scarce.
    static void sawScarce(Human& h) { h.water_.scarce = true; }
    /// What keeps happening (AI/repeat_lines_plan.md): how many times each
    /// kind has, set as if it had happened that often already.
    using Repeat = Human::Repeat;
    static int repeats(const Human& h, Repeat r) { return h.repeat_.count[size_t(r)]; }
    static void setRepeats(Human& h, Repeat r, int n) { h.repeat_.count[size_t(r)] = n; }
    static float dread(const Human& h) { return h.dread_; }
    static void setDread(Human& h, float d) { h.dread_ = d; }
    static size_t bodiesSeen(const Human& h) { return h.deaths_.seenDead.size(); }
    /// Given up on a way out, at home on home.
    static void settle(Human& h, Vec2i home) {
        h.escape_.settled = true;
        h.escape_.home = home;
        h.escape_.hope = 0.0f;
    }
};

/// What tools/genview.cpp asks of the world: one chunk of the kind and
/// footprint it is told, grown as the game would grow it, past the spawn.
struct GenViewProbe {
    /// Whether the game ever grows this kind on this footprint.
    static bool grows(ChunkKind kind, const Footprint& parts) { return World::grows(kind, parts); }
    /// Grows a chunk of this kind on the footprint past the spawn's door
    /// (World::growPastSpawn).
    static const Chunk* grow(World& w, ChunkKind kind, const Footprint& parts) { return w.growPastSpawn(kind, parts); }
    /// A human waking at a cell as every human does, a bottle on the floor
    /// a step or two away.
    static Human& wake(World& w, const HumanLook& look, Vec2i cell, uint64_t seed) {
        Human& h = w.addHuman(look, cell, seed);
        w.layBottle(h);
        return h;
    }
    /// Where keys and the chunk's human may start: floor deep enough into
    /// the chunk from where it was entered. World cells.
    static std::vector<Vec2i> deepSpots(World& w, const Chunk& c, Vec2i entry) {
        return w.deepSpots(c, entry - c.origin());
    }
    /// The chunk the game would grow through the spawn's door, its kind and
    /// footprint rolled.
    static const Chunk* rolled(World& w) {
        w.ensureGenerated(w.spawnEntry());
        return w.chunks_.back().get();
    }
};
