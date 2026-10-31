#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <cmath>

// Computers: the terminals and the console desk of the server rooms. A
// curious human tries the first terminal and the first desk it comes upon
// while exploring or wandering: walks up, stands at the keys and types, to
// learn something of this place. Each is locked by a password. Now and then
// it tries another it passes, never one it tried before, each try less
// likely than the last. The first try lifts its hope a little, someone runs
// this place; each lock after that, a little dread. Curious onlookers
// remark on someone at the keys, once (AI/computers_plan.md).

namespace {

/// This curious or more, it tries the computers it comes upon (user).
constexpr float COMPUTER_CURIOUS = 0.5f;
/// The first of each kind it always tries; after that, each computer that
/// turned it away halves the chance it tries the next it sees (user).
constexpr float TRY_AGAIN_FADE = 0.5f;
/// It walks up to one no farther than this many steps away, at middling
/// curiosity; the more curious up to half as far again.
constexpr int COMPUTER_REACH = 10;
constexpr float COMPUTER_REACH_CURIOUS = 0.5f;
/// It types for this long, up to the span more for the most curious (user: 3 s to 8 s).
constexpr float TYPE_TIME_MIN = 3.0f, TYPE_TIME_SPAN = 5.0f;
/// Not got to one it meant to try, it lets computers be this long.
constexpr float COMPUTER_PAUSE = 10.0f;
/// Hope from the first try: someone runs this place (user).
constexpr float HOPE_COMPUTER = 0.05f;
/// Dread from each lock after the first (user).
constexpr float DREAD_LOCKED = 0.05f;
/// Stood a while at it after the password stopped it, its line said.
constexpr float LOCKED_LOOK_MIN = 0.6f, LOCKED_LOOK_MAX = 1.5f;

} // namespace

void Human::seeComputer(const ServerPiece& p) {
    if (state == State::Waking || traits.curious < COMPUTER_CURIOUS) return;
    if (!meansToTry(p) || clock_ < computers_.pauseUntil) return;
    // Kept already, or the one it is walking to.
    auto same = [&](const Find& f) { return f.rig && f.at == p.at; };
    if (inspect_.going && same(*inspect_.going)) return;
    for (const Find& f : inspect_.queue)
        if (same(f)) return;
    bool first = p.kind == Rig::Terminal ? !computers_.terminal : !computers_.desk;
    int reach = int(std::lround(COMPUTER_REACH * sway(traits.curious, COMPUTER_REACH_CURIOUS)));
    if (cheb(cell, p.at) > reach) return;
    using M = talk::Moment;
    Find f(first ? (p.kind == Rig::Terminal ? M::TryTerminal : M::TryDesk) : M::TryAgain, p.at);
    f.rig = p.kind;
    discover(std::move(f));
}

bool Human::meansToTry(const ServerPiece& p) {
    using T = Computers::Try;
    T& t = computers_.seen.emplace(p.at, T::Unweighed).first->second;
    if (t == T::Unweighed) {
        bool first = p.kind == Rig::Terminal ? !computers_.terminal : !computers_.desk;
        t = first ? T::First : rng_.chance(std::pow(TRY_AGAIN_FADE, float(computers_.tries))) ? T::Meant : T::Passed;
    }
    return t == T::First || t == T::Meant;
}

void Human::letComputerBe() { computers_.pauseUntil = clock_ + COMPUTER_PAUSE; }

void Human::watchTyping(const Human& o) {
    if (computers_.watched || state == State::Waking || traits.curious < COMPUTER_CURIOUS) return;
    computers_.watched = true;
    note("WATCHED @ TRY A COMPUTER", o.id);
    sayLater(Saying(talk::Moment::WatchTyping, -1, o.id));
}

void Human::startTyping(const Find& f) {
    ServerPiece p{*f.rig, f.at};
    // Turned away by another on its way here, it may think better of this one.
    if (!meansToTry(p)) return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    if (!p.usedFrom(cell)) {
        letComputerBe();
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    }
    // Tried now, whether or not it sees it through: never again.
    computers_.seen[f.at] = Computers::Try::Tried;
    computers_.at = f.at;
    computers_.kind = p.kind;
    // A terminal stands beside it; at the desk it turns in from its side of the chair.
    facingLeft = p.kind == Rig::Terminal ? f.at.x < cell.x : cell.x > f.at.x + 2;
    note(p.kind == Rig::Terminal ? "STEPPED UP TO A TERMINAL TO TYPE" : "TOOK THE CHAIR AT THE CONSOLE DESK TO TYPE");
    state = State::Typing;
    goal = Goal::Inspect;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    glance_ = SIT_GLANCE;
    timer = TYPE_TIME_MIN + TYPE_TIME_SPAN * traits.curious;
}

void Human::typing(World& world, float dt) {
    glance_ -= dt;
    if (glance_ <= 0.0f) {
        glance_ = SIT_GLANCE;
        if (observe(world)) rethink_ = true;
    }
    if (timer > 0.0f && freeToInspect(false)) return;
    endTyping(world);
}

void Human::endTyping(World& world) {
    // Something pressing came up: it leaves the keys mid-try.
    if (timer > 0.0f) {
        note("LEFT THE COMPUTER, SOMETHING MORE PRESSING");
        state = State::Idle;
        return choose(world);
    }
    using M = talk::Moment;
    bool terminal = computers_.kind == Rig::Terminal;
    bool first = terminal ? !computers_.terminal : !computers_.desk;
    M m = first ? (terminal ? M::TryTerminal : M::TryDesk) : M::TryAgain;
    note(first ? (terminal ? "TRIED A TERMINAL. LOCKED BY A PASSWORD" : "TRIED THE CONSOLE DESK. LOCKED BY A PASSWORD")
               : (terminal ? "TRIED ANOTHER TERMINAL. A PASSWORD AGAIN" : "TRIED ANOTHER CONSOLE DESK. A PASSWORD AGAIN"));
    if (computers_.tries == 0)
        lift(HOPE_COMPUTER);
    else
        scare(DREAD_LOCKED);
    (terminal ? computers_.terminal : computers_.desk) = true;
    computers_.tries++;
    // Those it meant to try only as the first of their kind are weighed again.
    for (auto& [at, t] : computers_.seen)
        if (t == Computers::Try::First) t = Computers::Try::Unweighed;
    // It stands at the keys until its line is said, then a moment more.
    startIdle(LOCKED_LOOK_MIN, LOCKED_LOOK_MAX);
    goal = Goal::Inspect;
    sayHere(Saying(m));
}
