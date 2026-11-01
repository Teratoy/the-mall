#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

// Discoveries: the first key, cake, door, water, toilet, machine, server
// room, footprints, statue, bottle lying, body, pool of blood and hidden
// trap a human sees, it goes up to while exploring or wandering, stands and
// looks at, and remarks on there before going on (user). The curious go
// farther for it and stand longer. A body, blood or a trap only the brave
// and the curious walk up to; the rest stop and look from where they are.
// Anything pressing, and it remarks on them where it stands, as it would
// have walking past.

namespace {

/// It walks up to a find no farther than this many steps away, at middling
/// curiosity; the curious go up to half as far again, the incurious half as far.
constexpr int INSPECT_REACH = 8;
constexpr float INSPECT_REACH_CURIOUS = 0.5f;
/// How long it stands looking once its line is said: a glance, up to the
/// whole span more for the most curious.
constexpr float INSPECT_TIME_MIN = 0.6f, INSPECT_TIME_SPAN = 3.4f;
/// This brave, or this curious, it walks up to a body, blood or a trap.
constexpr float GRIM_BRAVE = 0.6f, GRIM_CURIOUS = 0.6f;
/// Not there this many seconds after setting off, it gives up and remarks
/// on the find where it stands.
constexpr float INSPECT_GIVE_UP = 15.0f;

/// The find as the life log names it.
const char* findWord(talk::Moment m) {
    using M = talk::Moment;
    switch (m) {
    case M::FirstKey: return "A KEY";
    case M::FirstCake: return "A CAKE";
    case M::FirstDoor: return "A DOOR";
    case M::FirstWater: return "THE WATER";
    case M::FirstPrints: return "FOOTPRINTS";
    case M::FirstToilet: return "A TOILET";
    case M::FirstVending: return "A MACHINE";
    case M::FirstServerRoom: return "THE HUMMING MACHINES";
    case M::FirstStatue: return "A STATUE";
    case M::SeeBottle: return "A BOTTLE";
    case M::FoundBody: return "A BODY";
    case M::Pool: return "THE BLOOD";
    case M::SpotTrap: return "A TRAP";
    default: return "SOMETHING";
    }
}

} // namespace

bool Human::freeToInspect(bool grim) const {
    if (state == State::Waking || dead()) return false;
    if (goal != Goal::Explore && goal != Goal::Wander && goal != Goal::Inspect) return false;
    if (hungry() || thirsty() || tired() || needsToGo() || running()) return false;
    if (fight_.foe >= 0 || fight_.fear > 0.0f || escorting_.escortee >= 0 || escorting_.by >= 0) return false;
    return grim || gore_.scaredUntil <= clock_;
}

bool Human::discover(Find f) {
    if (!freeToInspect(f.grim)) return false;
    inspect_.queue.push_back(std::move(f));
    if (goal != Goal::Inspect) rethink_ = true;
    return true;
}

bool Human::sayFind(const Find& f) {
    // A computer not got to is said nothing of: it may come back to it.
    if (f.rig) {
        letComputerBe();
        return false;
    }
    Saying s(f.moment, -1, f.about);
    s.topic = f.topic;
    return sayLater(std::move(s));
}

bool Human::gone(const Find& f) const {
    return (f.moment == talk::Moment::FirstKey && !memory.keys.count(f.at)) ||
           (f.moment == talk::Moment::FirstCake && !memory.cakes.count(f.at));
}

bool Human::inspectStale() const {
    return goal == Goal::Inspect && inspect_.going &&
           (!freeToInspect(inspect_.going->grim) || clock_ - inspect_.since > INSPECT_GIVE_UP);
}

void Human::settleFinds() {
    // Turned aside on its way: the find goes back to be weighed again, unless
    // it has been too long getting there.
    if (inspect_.going) {
        Find f = std::move(*inspect_.going);
        inspect_.going.reset();
        if (clock_ - inspect_.since > INSPECT_GIVE_UP)
            sayFind(f);
        else
            inspect_.queue.insert(inspect_.queue.begin(), std::move(f));
    }
    for (size_t i = 0; i < inspect_.queue.size();) {
        if (freeToInspect(inspect_.queue[i].grim)) {
            i++;
            continue;
        }
        sayFind(inspect_.queue[i]);
        inspect_.queue.erase(inspect_.queue.begin() + long(i));
    }
}

bool Human::inspectNext(World& world, bool grimOnly) {
    int reach = int(std::lround(INSPECT_REACH * sway(traits.curious, INSPECT_REACH_CURIOUS)));
    while (true) {
        auto it = inspect_.queue.end();
        for (auto f = inspect_.queue.begin(); f != inspect_.queue.end(); ++f)
            if ((!grimOnly || f->grim) && (it == inspect_.queue.end() || cheb(cell, f->at) < cheb(cell, it->at))) it = f;
        if (it == inspect_.queue.end()) return false;
        Find f = std::move(*it);
        inspect_.queue.erase(it);
        if (gone(f)) {
            sayFind(f);
            continue;
        }
        if (f.grim && traits.brave < GRIM_BRAVE && traits.curious < GRIM_CURIOUS) {
            lookAt(f, false);
            return true;
        }
        if (cheb(cell, f.at) > reach) {
            sayFind(f);
            continue;
        }
        auto p = search(world, [&](Vec2i c) {
            bool there = f.rig ? ServerPiece{*f.rig, f.at}.usedFrom(c) : c != f.at && cheb(c, f.at) <= f.near;
            return there && !avoids(c) && (c == cell || !world.occupant(c));
        });
        if (!p || int(p->size()) > reach) {
            sayFind(f);
            continue;
        }
        if (p->empty()) {
            lookAt(f, false);
            return true;
        }
        startWalking(Goal::Inspect, *p);
        inspect_.going = std::move(f);
        inspect_.since = clock_;
        return true;
    }
}

void Human::lookAt(const Find& f, bool walked) {
    if (f.rig) return startTyping(f);
    if (f.at.x != cell.x) facingLeft = f.at.x < cell.x;
    note(std::string(walked ? "WENT CLOSER TO LOOK AT " : "STOPPED TO LOOK AT ") + findWord(f.moment));
    startIdle(INSPECT_TIME_MIN, INSPECT_TIME_MIN + INSPECT_TIME_SPAN * traits.curious);
    goal = Goal::Inspect;
    Saying s(f.moment, -1, f.about);
    s.topic = f.topic;
    sayHere(std::move(s));
}

// Its line comes next, ahead of anything else it meant to say but what it
// had to say on coming to, and it stands there until it is said, then a while more.
void Human::sayHere(Saying s) {
    s.awaited = true;
    auto at = std::find_if(voice_.sayings.begin(), voice_.sayings.end(),
                           [](const Saying& q) { return !talk::isUrgent(q.moment) && !talk::isWaking(q.moment); });
    voice_.sayings.insert(at, std::move(s));
    awaiting_ = true;
    voice_.answerWait = timer;
}
