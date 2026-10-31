#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cstdlib>
#include <queue>

// Shut in a slam room with no key, a human cries for help now and then,
// loud enough to carry through the walls. Anyone in earshot hears it, sees
// them or not, and is shaken; one with a key to spare may go and let them
// out, though a key spent on a gate is one fewer for a way out, so few do.
// Whoever is let out trusts their rescuer deeply and keeps with them.
// A hearer remembers the cry a while: coming by a key later, it may go
// then, and it tells whoever it meets. Without a key it goes for one it
// knows of, and may go and call back through the wall. The greedy and the
// erratic don't care either way (AI/shut_in_plan.md).

namespace {

/// The first cry comes this long after the gate shuts, then one every so
/// often; each carries this far, walls or not.
constexpr float HELP_FIRST_MIN = 8.0f, HELP_FIRST_MAX = 15.0f;
constexpr float HELP_EVERY_MIN = 20.0f, HELP_EVERY_MAX = 40.0f;
constexpr int HELP_RADIUS = 12;
/// Walled in: it can reach no more than this many cells.
constexpr int WALLED_IN_CELLS = 120;
/// Hearing a cry for help, the first time from each.
constexpr float DREAD_HELP_HEARD = 0.1f;
/// The chance a cry heard sends a key holder off to let them out, swayed
/// by how generous, brave and sociable it is; more for a friend or someone
/// whose name it knows, or with a key to spare; less the gloomier it is.
constexpr float RESCUE_CHANCE = 0.15f;
constexpr float RESCUE_GENEROUS = 0.6f, RESCUE_BRAVE = 0.3f, RESCUE_SOCIABLE = 0.3f;
constexpr float RESCUE_FRIEND = 3.0f, RESCUE_KNOWN = 1.5f, RESCUE_SPARE = 1.5f;
constexpr float RESCUE_GLOOM = 0.6f;
/// What a rescue does to the one let out, and to the one who did it.
constexpr float TRUST_FREED = 0.6f, TRUST_FREER = 0.15f;
constexpr float HOPE_FREED = 0.25f;
constexpr float EPISODE_FREED = 0.6f, EPISODE_FREED_ME = 0.9f;
/// A cry is remembered this long after it was last heard or told of.
constexpr float CRY_STALE = 600.0f;
/// At least this greedy, it doesn't care who is shut in.
constexpr float UNCARING_GREEDY = 0.67f;
/// A locked door it knows this near where a voice came from is taken for
/// the gate.
constexpr int GATE_NEAR = 8;
/// Going to a voice with no way known to the gate, it makes for the known
/// ground nearest it, each cell farther from the voice costing this much
/// more to end at (in tenths of a cell: more than a step, so nearer always
/// wins).
constexpr int VOICE_COST = 20;
/// With a key, it looks for the gate from the edge of what it knows, no
/// farther than this from it or the voice.
constexpr int LOOK_NEAR = 12;
/// How much more it wants to fetch a key it knows of, keyless and minded
/// to let someone out.
constexpr float RESCUE_FETCH = 0.8f;
/// Calling back through the wall: how long it stands there, and the hope
/// it gives whoever is shut in.
constexpr float CALL_BACK_WAIT_MIN = 2.0f, CALL_BACK_WAIT_MAX = 4.0f;
constexpr float HOPE_CALLED_BACK = 0.1f;

float sway(float trait, float half) { return 1.0f + (trait - 0.5f) * 2.0f * half; }

} // namespace

void Human::shutIn(Vec2i gate) {
    rescue_.slamGate = gate;
    rescue_.helpTimer = rng_.range(HELP_FIRST_MIN, HELP_FIRST_MAX);
}

bool Human::walledIn(const World& world) const {
    std::unordered_set<Vec2i, Vec2iHash> seen{cell};
    std::queue<Vec2i> open;
    open.push(cell);
    while (!open.empty()) {
        Vec2i c = open.front();
        open.pop();
        for (Vec2i d : DIRS4) {
            Vec2i n = c + d;
            if (!passable(world.tile(n)) || !seen.insert(n).second) continue;
            if (int(seen.size()) > WALLED_IN_CELLS) return false;
            open.push(n);
        }
    }
    return true;
}

void Human::callForHelp(World& world, float dt) {
    callThroughDoor(world, dt);
    if (!rescue_.slamGate) return;
    // Opened, by itself or by someone else: out of it.
    if (world.tile(*rescue_.slamGate) != Tile::DoorLocked) {
        rescue_.slamGate.reset();
        return;
    }
    if ((rescue_.helpTimer -= dt) > 0.0f) return;
    rescue_.helpTimer = rng_.range(HELP_EVERY_MIN, HELP_EVERY_MAX);
    // Stepped out before the gate shut, it was never shut in; with a key it
    // lets itself out.
    if (!walledIn(world)) {
        rescue_.slamGate.reset();
        return;
    }
    if (keys > 0) return;
    if (!sayLater(talk::Moment::Help)) return;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && cheb(o->cell, cell) <= HELP_RADIUS) o->hearHelp(*this, *rescue_.slamGate);
}

// Heard as a cry from a slam room is: anyone near enough may come with a
// key, or call back.
void Human::callThroughDoor(World& world, float dt) {
    if (!rescue_.callDoor) return;
    Vec2i door = *rescue_.callDoor;
    // Opened, no longer parched, or gone from it: done calling.
    if (world.tile(door) != Tile::DoorLocked || !parched() || cheb(cell, door) > 1 || dead()) {
        rescue_.callDoor.reset();
        return;
    }
    if ((rescue_.callTimer -= dt) > 0.0f) return;
    rescue_.callTimer = rng_.range(HELP_EVERY_MIN, HELP_EVERY_MAX);
    if (state == State::Sleeping || !sayLater(talk::Moment::CallDoor)) return;
    // Those in there with it just hear it said; it is for whoever is beyond.
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && cheb(o->cell, cell) <= HELP_RADIUS && !o->sight.sees(cell))
            o->hearHelp(*this, door);
}

void Human::hearHelp(const Human& caller, Vec2i gate) {
    if (dead() || state == State::Sleeping) return;
    // Behind a wall, it knows the voice only of a friend.
    bool known = isFriend(caller.id);
    if (rescue_.heardHelp.insert(caller.id).second) {
        if (known)
            note("HEARD @ CALLING FOR HELP", caller.id);
        else
            note("HEARD SOMEONE CALLING FOR HELP");
        scare(DREAD_HELP_HEARD);
    }
    // The gate only if it has seen that tile; else just where the voice came from.
    bool knowsGate = memory.tile(gate) != Tile::Void;
    hearOf(caller.id, knowsGate ? gate : caller.cell, knowsGate, -1);
    weighRescue(caller.id);
}

bool Human::caresForShutIn() const { return !isErratic() && traits.greedy < UNCARING_GREEDY; }

// Heard again or told again, it is fresh again; a gate known is never
// traded for a voice.
void Human::hearOf(int caller, Vec2i at, bool gate, int teller) {
    rescue_.out.erase(caller);
    auto [it, fresh] = rescue_.cries.try_emplace(caller);
    Rescue::Cry& c = it->second;
    if (fresh) c.teller = teller;
    if (gate || !c.gate) {
        c.at = at;
        c.gate = gate;
    }
    c.when = clock_;
    if (!gate) return;
    if (teller < 0)
        memory.see(at, Tile::DoorLocked);
    else
        memory.hear(at, Tile::DoorLocked);
}

void Human::weighRescue(int caller) {
    auto it = rescue_.cries.find(caller);
    if (it == rescue_.cries.end()) return;
    const Rescue::Cry& c = it->second;
    // Only minded to, and not with nothing to add: a key, or a call not yet made.
    if (rescue_.rescuee >= 0 || !caresForShutIn() || hunting() || starving() || parched() || isFoe(caller) ||
        suspicious(caller) || (keys == 0 && c.calledBack))
        return;
    bool known = isFriend(caller);
    float will = RESCUE_CHANCE * sway(1.0f - traits.greedy, RESCUE_GENEROUS) * sway(traits.brave, RESCUE_BRAVE) *
                 sway(traits.sociable, RESCUE_SOCIABLE) * (1.0f - RESCUE_GLOOM * gloom());
    if (known)
        will *= RESCUE_FRIEND;
    else if (knownName(caller))
        will *= RESCUE_KNOWN;
    if (keys > 1) will *= RESCUE_SPARE;
    if (!rng_.chance(will)) return;
    rescue_.rescuee = caller;
    if (c.gate) rescue_.gate = c.at;
    rethink_ = true;
    // Told of them, it was told who.
    known |= c.teller >= 0;
    if (keys == 0) {
        if (known)
            note("GOING TO CALL BACK TO @", caller);
        else
            note("GOING TO CALL BACK TO WHOEVER IT IS");
        return;
    }
    if (known)
        note("GOING TO LET @ OUT", caller);
    else
        note("GOING TO LET WHOEVER IT IS OUT");
    sayLater(talk::Moment::HelpComing, -1, caller);
}

// Over: the gate seen open, the caller seen dead or met out and about, or
// heard of too long ago.
void Human::recallCries() {
    for (auto it = rescue_.cries.begin(); it != rescue_.cries.end();) {
        int who = it->first;
        const Rescue::Cry& c = it->second;
        auto met = social_.greetedAt.find(who);
        bool freed = (c.gate && memory.tile(c.at) != Tile::DoorLocked) ||
                     (met != social_.greetedAt.end() && met->second > c.when);
        bool over = freed || clock_ - c.when > CRY_STALE || deaths_.seenDead.count(who);
        if (!over) {
            ++it;
            continue;
        }
        if (freed) rescue_.out.insert(who);
        if (rescue_.rescuee == who) rescue_.rescuee = -1;
        it = rescue_.cries.erase(it);
    }
    if (keys > rescue_.keysWeighed) {
        rescue_.keyNoted = false;
        for (const auto& [who, c] : rescue_.cries) weighRescue(who);
    }
    rescue_.keysWeighed = keys;
}

std::optional<Vec2i> Human::gateNear(Vec2i voice) const {
    std::optional<Vec2i> best;
    for (int y = -GATE_NEAR; y <= GATE_NEAR; y++)
        for (int x = -GATE_NEAR; x <= GATE_NEAR; x++) {
            Vec2i c = voice + Vec2i{x, y};
            if (memory.tile(c) == Tile::DoorLocked && (!best || cheb(c, voice) < cheb(*best, voice))) best = c;
        }
    return best;
}

float Human::keyForCries() const {
    return keys == 0 && caresForShutIn() && !rescue_.cries.empty() ? RESCUE_FETCH : 0.0f;
}

void Human::heardBack(int from) {
    if (dead() || (!rescue_.slamGate && !rescue_.callDoor)) return;
    if (isFriend(from))
        note("@ CALLED BACK THROUGH THE WALL", from);
    else
        note("SOMEONE OUTSIDE CALLED BACK");
    lift(HOPE_CALLED_BACK);
    sayLater(talk::Moment::HeardBack);
}

// To beside the gate if it knows one, else as near the voice as it knows
// ground: from where it stands, the start included.
std::vector<Vec2i> Human::wayToShutIn(const World& world, const Tell& t) const {
    std::optional<std::vector<Vec2i>> p;
    if (t.gate) p = search(world, [&](Vec2i x) { return lockedDoorBeside(x) == std::optional<Vec2i>(t.cell); });
    if (!p) p = search(world, [](Vec2i) { return true; }, false, [&](Vec2i x) { return VOICE_COST * cheb(x, t.cell); });
    std::vector<Vec2i> route{cell};
    if (p) route.insert(route.end(), p->begin(), p->end());
    return route;
}

// With a key and the gate known, to the gate; else as near the voice as
// it can get, where it looks for the gate, or calls back through the wall.
bool Human::startRescue(World& world) {
    Human& o = world.human(rescue_.rescuee);
    auto it = rescue_.cries.find(rescue_.rescuee);
    auto giveUp = [&] {
        rescue_.rescuee = -1;
        return false;
    };
    if (it == rescue_.cries.end() || o.dead()) return giveUp();
    Rescue::Cry& c = it->second;
    if (keys > 0 && !c.gate)
        if (auto g = gateNear(c.at)) {
            c.at = *g;
            c.gate = true;
            note("FOUND THE GATE @ IS SHUT BEHIND", o.id);
        }
    if (keys > 0 && c.gate) {
        rescue_.gate = c.at;
        auto beside = [&](Vec2i x) { return lockedDoorBeside(x) == std::optional<Vec2i>(rescue_.gate); };
        if (auto p = search(world, beside)) {
            startWalking(Goal::OpenDoor, *p);
            return true;
        }
        // Not knowing the way to it, it gets as near as it can, and looks.
    }
    // With a key, it looks round the edge of what it knows nearest them:
    // the gate, or the way to it, may be just out of sight. Without one, it
    // only has to get near enough to call.
    auto nearer = [&](Vec2i x) { return VOICE_COST * cheb(x, c.at); };
    std::optional<std::vector<Vec2i>> p;
    if (keys > 0)
        p = search(world, [&](Vec2i x) { return isFrontier(x) && cheb(x, c.at) <= LOOK_NEAR; }, false, nearer);
    else
        p = search(world, [](Vec2i) { return true; }, false, nearer);
    if (!p) return giveUp();
    if (!p->empty()) {
        startWalking(Goal::Rescue, *p);
        return true;
    }
    // As near as it can get.
    rescue_.rescuee = -1;
    if (keys > 0) {
        note("COULDN'T FIND WHERE @ IS SHUT IN", o.id);
        sayLater(talk::Moment::LostShutIn, -1, o.id);
        rescue_.cries.erase(it);
        return false;
    }
    c.calledBack = true;
    note("CALLED BACK TO @ THROUGH THE WALL", o.id);
    sayLater(talk::Moment::CallBack, -1, o.id);
    if (cheb(o.cell, cell) <= HELP_RADIUS) o.heardBack(id);
    startIdle(CALL_BACK_WAIT_MIN, CALL_BACK_WAIT_MAX);
    return true;
}

bool Human::letOut(World& world, Vec2i gate) {
    if (rescue_.gate == gate) rescue_.rescuee = -1;
    bool any = false;
    for (const auto& h : world.humans()) {
        Human& o = *h;
        bool slam = o.rescue_.slamGate == gate;
        // A door runs through both walls: whichever cell of it was unlocked.
        bool called = o.rescue_.callDoor && world.tile(*o.rescue_.callDoor) != Tile::DoorLocked;
        if (&o == this || o.dead() || (!slam && !called)) continue;
        any = true;
        rescue_.cries.erase(o.id);
        rescue_.out.insert(o.id);
        note(slam ? "LET @ OUT OF A SLAM ROOM" : "OPENED THE DOOR @ WAS CALLING THROUGH", o.id);
        o.note("@ LET ME OUT", id);
        rememberAt(gate, Episode::What::Freed, EPISODE_FREED, id, o.id);
        o.rememberAt(gate, Episode::What::Freed, EPISODE_FREED_ME, id, o.id);
        o.rescue_.slamGate.reset();
        o.rescue_.callDoor.reset();
        regard(o.id, TRUST_FREER);
        o.regard(id, TRUST_FREED);
        o.lift(HOPE_FREED);
        // With no key of its own, it keeps with whoever let it out.
        if (o.social_.leader < 0 && !o.escape_.settled && o.keys == 0 && keys > 0) {
            o.social_.leader = id;
            o.note("FOLLOWING @, WHO LET ME OUT", id);
        }
        o.rethink_ = true;
        o.sayLater(talk::Moment::Freed, id);
    }
    return any;
}
