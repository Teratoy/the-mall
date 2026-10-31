#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"

// Server rooms: racks and screens humming behind two locked doors. Seeing
// one, a human remembers it, and the first it ever sees it remarks on
// (user). Tired, the warm hum and the locked doors make it the best place
// it knows to sleep: with a key for each door it makes for one, opens its
// way in, and lies down among the machines (user). Getting out is free.
// Twice through a door to find another locked behind it, it learns the
// double doors eat keys and keeps its keys from them, unless curious; and
// having felt the hum, drowsy inside, it sleeps before leaving
// (AI/server_room_keys_plan.md).

namespace {

/// It makes for a server room to sleep in no farther than this many cells away.
constexpr int HUM_REACH = 60;
/// A cell inside a known server room costs this share of what it would
/// elsewhere, when choosing where to sleep.
constexpr float HUM_SLEEP_SHARE = 0.3f;
/// The inner door stands no more than this many cells in from the outer.
constexpr int AIRLOCK_DEPTH = 3;
/// Found another locked door behind the one it opened this many times, it
/// has learnt (user: the second time).
constexpr int AIRLOCK_LEARN = 2;
/// Key-wise, only this curious or more still spends keys on the airlock
/// to see what's behind (user).
constexpr float AIRLOCK_DARE_CURIOUS = 0.65f;

} // namespace

void Human::seeServerRoom(const World& world, Vec2i c) {
    const ServerRoom* r = world.serverRoom(c);
    if (!r) return;
    if (!server_.seen && state != State::Waking) {
        server_.seen = true;
        note("FOUND A ROOM OF HUMMING MACHINES");
        sayLater(talk::Moment::FirstServerRoom);
    }
    memory.serverRooms[r->inner] = *r;
    if (!server_.lulled && state != State::Waking && r->contains(cell)) {
        server_.lulled = true;
        note("THE HUM IN HERE IS MADE FOR SLEEP");
        sayLater(talk::Moment::HumLesson);
    }
}

int Human::keysFor(Vec2i door) const {
    auto it = memory.airlocks.find(door);
    if (it == memory.airlocks.end()) return 1;
    for (int k = 2; k <= AIRLOCK_DEPTH; k++) {
        auto out = memory.airlocks.find(door - it->second * k);
        if (out != memory.airlocks.end() && out->second == it->second) return 1;
    }
    return 2;
}

void Human::meetAirlock(Vec2i door) {
    auto it = memory.airlocks.find(door);
    if (server_.keyWise || it == memory.airlocks.end()) return;
    bool behind = false;
    for (int k = 2; k <= AIRLOCK_DEPTH; k++) {
        Vec2i c = door + it->second * k;
        auto in = memory.airlocks.find(c);
        behind |= in != memory.airlocks.end() && in->second == it->second && memory.tile(c) == Tile::DoorLocked;
    }
    if (!behind) return;
    if (++server_.airlocksMet < AIRLOCK_LEARN) {
        note("ANOTHER LOCKED DOOR BEHIND THE FIRST");
        sayLater(talk::Moment::AirlockSurprise);
        return;
    }
    server_.keyWise = true;
    note("LEARNT THE DOUBLE DOORS EAT KEYS");
    sayLater(talk::Moment::AirlockLesson);
}

bool Human::sparesKeys(Vec2i door) const {
    if (!server_.keyWise || !memory.airlocks.count(door)) return false;
    return traits.curious < AIRLOCK_DARE_CURIOUS || keys < keysFor(door);
}

std::optional<Vec2i> Human::doorWorthKey(Vec2i c, std::optional<Vec2i>& spared) const {
    auto d = lockedDoorBeside(c);
    if (!d || !sparesKeys(*d)) return d;
    if (!spared) spared = d;
    return std::nullopt;
}

// Said once an airlock, kept by its outer door as far as it knows it.
void Human::spareKeys(std::optional<Vec2i> spared) {
    if (!spared) return;
    Vec2i outer = *spared, in = memory.airlocks.at(outer);
    for (int k = 2; k <= AIRLOCK_DEPTH; k++) {
        auto out = memory.airlocks.find(*spared - in * k);
        if (out != memory.airlocks.end() && out->second == in) outer = out->first;
    }
    if (!server_.passedUp.insert(outer).second) return;
    note("LET THE DOUBLE DOORS BE, KEYS KEPT");
    sayLater(talk::Moment::SpareKeys);
}

float Human::humShare(Vec2i c) const {
    for (const auto& [inner, r] : memory.serverRooms)
        if (r.contains(c)) return HUM_SLEEP_SHARE;
    return 1.0f;
}

// A door it knows the push side of, stood beside on the other side, is a
// way in for a key. Two keys are wanted to set off for an outer door, one
// for an inner door, as it stands in the lobby.
bool Human::seekHum(World& world) {
    if (memory.serverRooms.empty() || humShare(cell) < 1.0f) return false;
    auto wayIn = [&](Vec2i c) -> std::optional<Vec2i> {
        for (Vec2i d : DIRS4) {
            auto it = memory.airlocks.find(c + d);
            if (it == memory.airlocks.end() || it->second != d || memory.tile(c + d) != Tile::DoorLocked) continue;
            int needs = memory.serverRooms.count(c + d) ? 1 : 2;
            if (keys >= needs) return c + d;
        }
        return std::nullopt;
    };
    if (auto door = wayIn(cell)) {
        if (!world.unlock(*door)) return false;
        keys--;
        note("UNLOCKED A DOOR TO THE HUMMING MACHINES");
        startPassDoor(*door);
        timer = UNLOCK_TIME;
        return true;
    }
    if (keys < 1) return false;
    auto p = search(world, [&](Vec2i c) { return cheb(c, cell) <= HUM_REACH && wayIn(c).has_value(); }, true);
    if (!p) return false;
    if (!server_.seeking) {
        server_.seeking = true;
        note("WENT TO SLEEP AMONG THE HUMMING MACHINES");
        sayLater(talk::Moment::SeekHum);
    }
    startWalking(Goal::Sleep, *p);
    return true;
}
