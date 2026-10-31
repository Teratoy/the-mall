#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>

// Calling out: early on, coming alone to its first door, open or locked, a
// human calls into the mall beyond, loud enough to carry through the walls. Whoever
// hears it from out of sight wonders who it is; the timid hush and are
// shaken, the sociable call back, and the caller hears the answer. A voice
// is someone else here, who might know the way out: the curious and the
// sociable go towards where it came from (AI/waking_talk_plan.md step 5).

namespace {

/// It calls out at its first door only this early in its life.
constexpr float CALL_EARLY = 300.0f;
/// A call carries this far, walls or not.
constexpr int CALL_RADIUS = 24;
/// A voice is worth going to this long after it was heard.
constexpr float CALL_STALE = 90.0f;
/// This sociable or more, it calls back.
constexpr float CALL_ANSWER_SOCIABLE = 0.55f;
/// Hearing a voice from out of sight shakes the timid this much.
constexpr float DREAD_CALL_TIMID = 0.1f;
/// Going to a voice, it makes for the edge of what it knows within this
/// many cells of it, else the known ground nearest it; each cell farther
/// from the voice costs this much more to end at (tenths of a cell: more
/// than a step, so nearer always wins).
constexpr int VOICE_NEAR = 10;
constexpr int VOICE_COST = 20;

} // namespace

void Human::callOut(const World& world, bool alone) {
    if (call_.called || clock_ >= CALL_EARLY || !alone || state == State::Waking || escape_.comingTo) return;
    if (world.tile(cell) != Tile::DoorOpen && !lockedDoorBeside(cell) && !openDoorBeside(cell)) return;
    call_.called = true;
    if (traits.brave < TIMID) return;
    note("CALLED OUT INTO THE MALL");
    sayLater(talk::Moment::CallOut);
}

void Human::shout(World& world, bool answer) {
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && cheb(o->cell, cell) <= CALL_RADIUS && !o->sight.sees(cell))
            o->hearCall(*this, answer);
}

void Human::hearCall(const Human& caller, bool answer) {
    if (dead() || state == State::Sleeping || state == State::Waking) return;
    call_.from = caller.id;
    call_.at = caller.cell;
    call_.heardAt = clock_;
    // Its own call answered.
    if (answer && call_.called && !call_.answered) {
        call_.answered = true;
        note("MY CALL WAS ANSWERED");
        sayLater(talk::Moment::CallAnswered, -1, caller.id);
        return;
    }
    note(answer ? "HEARD SOMEONE CALL BACK" : "HEARD SOMEONE CALL OUT");
    if (traits.brave < TIMID) {
        scare(DREAD_CALL_TIMID);
        sayLater(talk::Moment::HeardCall, -1, caller.id);
        return;
    }
    if (!answer && !call_.answered && traits.sociable >= CALL_ANSWER_SOCIABLE && !hungry() && !thirsty() &&
        fight_.fear <= 0.0f) {
        call_.answered = true;
        note("CALLED BACK");
        sayLater(talk::Moment::CallAnswer, -1, caller.id);
        return;
    }
    sayLater(talk::Moment::HeardCall, -1, caller.id);
}

bool Human::voiceToGo() const {
    return call_.from >= 0 && clock_ - call_.heardAt < CALL_STALE && traits.brave >= TIMID && !met.count(call_.from) &&
           std::find(inSight_.begin(), inSight_.end(), call_.from) == inSight_.end();
}

bool Human::startVoice(World& world) {
    if (!voiceToGo()) {
        call_.from = -1;
        return false;
    }
    Vec2i at = call_.at;
    auto nearer = [&](Vec2i x) { return VOICE_COST * cheb(x, at); };
    auto p = search(world, [&](Vec2i x) { return isFrontier(x) && cheb(x, at) <= VOICE_NEAR; }, false, nearer);
    if (!p) p = search(world, [](Vec2i) { return true; }, false, nearer);
    if (!p || p->empty()) {
        note("GOT AS NEAR AS I COULD TO THE VOICE");
        call_.from = -1;
        return false;
    }
    if (goal != Goal::Voice) note("WENT TOWARD THE VOICE");
    startWalking(Goal::Voice, *p);
    return true;
}
