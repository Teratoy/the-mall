#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>

// Erratic and stuck, shut in with no key or without hope of a way out, a
// human comes in time to want it over. Now and then it goes to a wall and
// beats its head against it: a few blows, each a thud, a bleeding head and
// blood on the wall, then it slumps down dazed, until the next time. It
// seldom lives through more than a few bouts. Anyone beyond the wall hears
// the thuds, taken for banging when it is shut in; anyone who sees it is
// shaken (AI/headbang_plan.md).

namespace {

/// Stuck this long before it first weighs a bout; then it weighs one this
/// often: a chance, more the gloomier it is, more shut in.
constexpr float HEADBANG_AFTER = 120.0f;
constexpr float HEADBANG_EVERY = 15.0f;
constexpr float HEADBANG_CHANCE = 0.08f, HEADBANG_GLOOM = 0.25f, HEADBANG_SHUT = 0.07f;
/// Once it has started, the chance is at least this: it goes on with it.
constexpr float HEADBANG_GOING_ON = 0.5f;
/// After a bout, no other for this long.
constexpr float HEADBANG_AGAIN_MIN = 25.0f, HEADBANG_AGAIN_MAX = 50.0f;
/// A wish to go at a wall left this long untaken is let go.
constexpr float HEADBANG_STALE = 10.0f;
/// The wall it goes to is beside floor no farther than this.
constexpr int HEADBANG_FARTHEST = 25;
/// A bout: this many blows, one every so often, the first after drawing
/// the head back; the head stays in the wall this long of each, and after
/// the last.
constexpr int HEADBUTT_BLOWS_MIN = 3, HEADBUTT_BLOWS_MAX = 5;
constexpr float HEADBUTT_EVERY = 0.8f, HEADBUTT_WINDUP = 0.5f, HEADBUTT_LANDED = 0.25f, HEADBUTT_HOLD = 0.4f;
/// Health each blow takes at once, and how much deeper than that it opens
/// the wound on its head: deep enough to bleed on and keep it from healing
/// until the next bout.
constexpr float HEADBUTT_DAMAGE_MIN = 0.06f, HEADBUTT_DAMAGE_MAX = 0.10f;
constexpr float HEADBUTT_WOUND = 1.5f;
/// Each blow leaves a splat on the wall's face where the head struck, this
/// far up its cell from the bottom and this far into it, as wide as this
/// and more per damage.
constexpr float SPLAT_UP_PX = 4.5f, SPLAT_IN_PX = 1.0f;
constexpr float SPLAT_RADIUS = 0.8f, SPLAT_RADIUS_PER_DAMAGE = 8.0f;
/// Dazed after a bout, as on waking.
constexpr float HEADBANG_DAZE = 0.6f;
/// The thuds are heard this far, walls or not, by anyone who can't see it.
constexpr int THUD_HEARD = 16;
/// Hearing someone's thuds, and seeing them at it, the first time each.
constexpr float DREAD_THUD = 0.08f, DREAD_SAW_HEADBANG = 0.2f;
/// The chance it says anything on first hearing the thuds.
constexpr float HEAR_THUD_SAY = 0.5f;

/// A wall beside c, east or west (the blow is drawn side on), as it knows.
std::optional<Vec2i> sideWall(const Memory& memory, Vec2i c) {
    for (Vec2i d : {Vec2i{1, 0}, Vec2i{-1, 0}})
        if (memory.tile(c + d) == Tile::Wall) return c + d;
    return std::nullopt;
}

} // namespace

bool Human::stuckForGood() const {
    return isErratic() && ((rescue_.slamGate && keys == 0) || rescue_.callDoor || gloom() > 0.0f);
}

void Human::weighHeadbang(World&, float) {
    if (!stuckForGood()) {
        headbang_.stuckSince = -1.0f;
        return;
    }
    if (headbang_.stuckSince < 0.0f) {
        headbang_.stuckSince = clock_;
        headbang_.nextRoll = clock_ + HEADBANG_AFTER;
        return;
    }
    if (clock_ < headbang_.nextRoll || headbang_.want) return;
    headbang_.nextRoll = clock_ + HEADBANG_EVERY;
    // Only when nothing else has hold of it.
    bool free = (state == State::Idle || state == State::Walking || state == State::Sitting) && fight_.foe < 0 &&
                !fight_.provoked && goal != Goal::Flee && goal != Goal::Fight;
    if (!free) return;
    bool shut = rescue_.slamGate || rescue_.callDoor;
    float chance = HEADBANG_CHANCE + HEADBANG_GLOOM * gloom() + (shut ? HEADBANG_SHUT : 0.0f);
    if (headbang_.bouts > 0) chance = std::max(chance, HEADBANG_GOING_ON);
    if (!rng_.chance(chance)) return;
    headbang_.want = true;
    headbang_.wantWhen = clock_;
    rethink_ = true;
}

bool Human::goAtWall(World& world) {
    if (!headbang_.want) return false;
    // Let go when stale, or no longer stuck; kept while it knows no wall yet.
    if (clock_ - headbang_.wantWhen > HEADBANG_STALE || !stuckForGood()) {
        headbang_.want = false;
        return false;
    }
    if (auto w = sideWall(memory, cell)) {
        headbang_.want = false;
        startHeadbanging(world, *w);
        return true;
    }
    auto p = searchNear(world, [&](Vec2i c) { return sideWall(memory, c).has_value(); }, HEADBANG_FARTHEST);
    if (!p || p->empty()) return false;
    headbang_.want = false;
    headbang_.wall = *sideWall(memory, p->back());
    startWalking(Goal::Headbang, *p);
    return true;
}

void Human::startHeadbanging(World& world, Vec2i wall) {
    headbang_.wall = wall;
    headbang_.blows = rng_.range(HEADBUTT_BLOWS_MIN, HEADBUTT_BLOWS_MAX);
    headbang_.blow = HEADBUTT_WINDUP;
    headbang_.bouts++;
    facingLeft = wall.x < cell.x;
    note("BEAT MY HEAD AGAINST THE WALL");
    sayLater(talk::Moment::Headbang);
    state = State::Headbanging;
    goal = Goal::Headbang;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    glance_ = SIT_GLANCE;
    // Whoever is there sees it; anyone beyond the walls hears thuds: banging,
    // when it is shut in.
    std::optional<Vec2i> gate = rescue_.slamGate ? rescue_.slamGate : rescue_.callDoor;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || cheb(o->cell, cell) > THUD_HEARD) continue;
        if (o->sight.sees(cell))
            o->sawHeadbang(*this);
        else if (gate)
            o->hearHelp(*this, *gate, true);
        else
            o->hearThud(*this);
    }
}

void Human::headbanging(World& world, float dt) {
    glance_ -= dt;
    if (glance_ <= 0.0f) {
        glance_ = SIT_GLANCE;
        observe(world);
    }
    // Let out, or hope come back: it stops.
    if (!stuckForGood()) headbang_.blows = 0;
    if ((headbang_.blow -= dt) > 0.0f) return;
    if (headbang_.blows > 0) {
        headbutt(world);
        if (dead()) return;
        headbang_.blow = --headbang_.blows > 0 ? HEADBUTT_EVERY : HEADBUTT_HOLD;
        return;
    }
    // Done: it slumps down dazed, and won't go again for a while.
    escape_.daze = std::max(escape_.daze, HEADBANG_DAZE);
    headbang_.nextRoll = clock_ + rng_.range(HEADBANG_AGAIN_MIN, HEADBANG_AGAIN_MAX);
    rethink_ = false;
    sitDown();
}

void Human::headbutt(World& world) {
    world.noise(NoiseKind::Thud, headbang_.wall, id);
    float damage = rng_.range(HEADBUTT_DAMAGE_MIN, HEADBUTT_DAMAGE_MAX);
    health -= damage;
    hurtTime = HURT_TIME;
    // Its own wound: nobody is credited with what bleeds it to death.
    wound(WoundKind::Blunt, BodyPart::Head, damage * HEADBUTT_WOUND, true, id);
    gore_.sprays.push_back({float(headbang_.wall.x - cell.x), 0.0f, damage, BodyPart::Head});
    float faceX = headbang_.wall.x < cell.x ? float(cell.x * CELL_PX) - SPLAT_IN_PX : float((cell.x + 1) * CELL_PX) + SPLAT_IN_PX;
    world.blood().drop(world, faceX, float((cell.y + 1) * CELL_PX) - SPLAT_UP_PX,
                       SPLAT_RADIUS + SPLAT_RADIUS_PER_DAMAGE * damage, id, rng_);
    if (health > 0.0f) return;
    note("BEAT MY HEAD ON THE WALL ONE LAST TIME");
    die(world);
}

bool Human::headbuttLanded() const {
    return state == State::Headbanging &&
           (headbang_.blows == 0 || headbang_.blow > HEADBUTT_EVERY - HEADBUTT_LANDED);
}

void Human::hearThud(const Human& who) {
    if (dead() || state == State::Sleeping || !headbang_.heard.insert(who.id).second) return;
    note("HEARD SOMETHING POUNDING ON A WALL");
    scare(DREAD_THUD);
    if (rng_.chance(HEAR_THUD_SAY)) sayLater(talk::Moment::HearThud);
}

void Human::sawHeadbang(const Human& who) {
    if (dead() || state == State::Sleeping || !headbang_.saw.insert(who.id).second) return;
    note("SAW @ BEATING THEIR HEAD AGAINST THE WALL", who.id);
    scare(DREAD_SAW_HEADBANG);
    sayLater(talk::Moment::SawHeadbang, -1, who.id);
}
