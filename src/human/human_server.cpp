#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"

// Server rooms: racks and screens humming behind two locked doors. Seeing
// one, a human remembers it, and the first it ever sees it remarks on
// (user). Tired, the warm hum and the locked doors make it the best place
// it knows to sleep: with a key for each door it makes for one, opens its
// way in, and lies down among the machines (user). Getting out is free.

namespace {

/// It makes for a server room to sleep in no farther than this many cells away.
constexpr int HUM_REACH = 60;
/// A cell inside a known server room costs this share of what it would
/// elsewhere, when choosing where to sleep.
constexpr float HUM_SLEEP_SHARE = 0.3f;

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
