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

float sway(float trait, float half) { return 1.0f + (trait - 0.5f) * 2.0f * half; }
int cheb(Vec2i a, Vec2i b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

} // namespace

void Human::shutIn(Vec2i gate) {
    slamGate_ = gate;
    helpTimer_ = rng_.range(HELP_FIRST_MIN, HELP_FIRST_MAX);
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
    if (!slamGate_) return;
    // Opened, by itself or by someone else: out of it.
    if (world.tile(*slamGate_) != Tile::DoorLocked) {
        slamGate_.reset();
        return;
    }
    if ((helpTimer_ -= dt) > 0.0f) return;
    helpTimer_ = rng_.range(HELP_EVERY_MIN, HELP_EVERY_MAX);
    // Stepped out before the gate shut, it was never shut in; with a key it
    // lets itself out.
    if (!walledIn(world)) {
        slamGate_.reset();
        return;
    }
    if (keys > 0) return;
    if (!sayLater(talk::Moment::Help)) return;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && cheb(o->cell, cell) <= HELP_RADIUS) o->hearHelp(*this, *slamGate_);
}

void Human::hearHelp(const Human& caller, Vec2i gate) {
    if (dead() || state == State::Sleeping) return;
    // Behind a wall, it knows the voice only of a friend.
    bool known = isFriend(caller.id);
    if (heardHelp_.insert(caller.id).second) {
        if (known)
            note("HEARD @ CALLING FOR HELP", caller.id);
        else
            note("HEARD SOMEONE CALLING FOR HELP");
        scare(DREAD_HELP_HEARD);
    }
    // Only with a key, knowing where the gate is, and minded to.
    if (rescuee_ >= 0 || keys == 0 || memory.tile(gate) == Tile::Void || isErratic() || hunting() || starving() ||
        parched() || isFoe(caller.id) || suspicious(caller.id))
        return;
    float will = RESCUE_CHANCE * sway(1.0f - traits.greedy, RESCUE_GENEROUS) * sway(traits.brave, RESCUE_BRAVE) *
                 sway(traits.sociable, RESCUE_SOCIABLE) * (1.0f - RESCUE_GLOOM * gloom());
    if (known)
        will *= RESCUE_FRIEND;
    else if (knownName(caller.id))
        will *= RESCUE_KNOWN;
    if (keys > 1) will *= RESCUE_SPARE;
    if (!rng_.chance(will)) return;
    rescuee_ = caller.id;
    rescueGate_ = gate;
    memory.see(gate, Tile::DoorLocked);
    rethink_ = true;
    if (known)
        note("GOING TO LET @ OUT", caller.id);
    else
        note("GOING TO LET WHOEVER IT IS OUT");
    sayLater(talk::Moment::HelpComing, -1, caller.id);
}

bool Human::startRescue(World& world) {
    const Human& o = world.human(rescuee_);
    auto beside = [&](Vec2i c) { return lockedDoorBeside(c) == std::optional<Vec2i>(rescueGate_); };
    std::optional<std::vector<Vec2i>> p;
    if (keys > 0 && !o.dead() && memory.tile(rescueGate_) == Tile::DoorLocked) p = search(world, beside);
    if (!p) {
        rescuee_ = -1;
        return false;
    }
    startWalking(Goal::OpenDoor, *p);
    return true;
}

bool Human::letOut(World& world, Vec2i gate) {
    if (rescueGate_ == gate) rescuee_ = -1;
    bool any = false;
    for (const auto& h : world.humans()) {
        Human& o = *h;
        if (&o == this || o.dead() || o.slamGate_ != gate) continue;
        any = true;
        note("LET @ OUT OF A SLAM ROOM", o.id);
        o.note("@ LET ME OUT", id);
        rememberAt(gate, Episode::What::Freed, EPISODE_FREED, id, o.id);
        o.rememberAt(gate, Episode::What::Freed, EPISODE_FREED_ME, id, o.id);
        o.slamGate_.reset();
        regard(o.id, TRUST_FREER);
        o.regard(id, TRUST_FREED);
        o.lift(HOPE_FREED);
        // With no key of its own, it keeps with whoever let it out.
        if (o.leader_ < 0 && !o.settled_ && o.keys == 0 && keys > 0) {
            o.leader_ = id;
            o.note("FOLLOWING @, WHO LET ME OUT", id);
        }
        o.rethink_ = true;
        o.sayLater(talk::Moment::Freed, id);
    }
    return any;
}
