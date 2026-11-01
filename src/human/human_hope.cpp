#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

// Hope: whether a human still believes there is a way out of here. It
// wakes fairly sure there is one. Each new chunk it sets foot in lifts it,
// the first few a lot, later ones less and less until more of the same
// only wears it down; opened doors and new friends lift it too. Dead ends,
// bodies, starving and long stretches finding nothing new wear it down.
// Without hope it settles: makes a home of a place by water, where it
// keeps sleeping safely, or by a table, keeps to it, and, if it is the
// kind to, drives off whoever comes in.

namespace {

constexpr float HOPE_START = 0.7f;
/// How much losing or finding hope, a new place, and settling strike it as
/// episodes to talk about.
constexpr float EPISODE_HOPE = 0.5f;
constexpr float EPISODE_NEW_PLACE = 0.3f;
constexpr float EPISODE_SETTLED = 0.6f;
/// The first few new chunks lift hope this much each; after HOPE_FRESH of
/// them each lifts HOPE_SAMEISH less than the last, down to HOPE_SAME_FLOOR
/// (wearing it down: more of the same).
constexpr float HOPE_NEW_CHUNK = 0.15f;
constexpr int HOPE_FRESH = 3;
constexpr float HOPE_SAMEISH = 0.05f;
constexpr float HOPE_SAME_FLOOR = -0.08f;
/// Nothing new seen for this long wears hope down this much, again and
/// again.
constexpr float HOPE_STALE_TIME = 300.0f;
constexpr float HOPE_STALE = -0.08f;
/// Below HOPE_LOW it is gloomy (gloom()); below the settling bar it gives
/// up and settles, the erratic sooner; above HOPE_UNSETTLE a settler sets
/// off again. Hope lifts wanting out at most this much over waking hope.
constexpr float HOPE_LOW = 0.35f;
constexpr float HOPE_SETTLE = 0.15f;
constexpr float HOPE_SETTLE_ERRATIC = 0.3f;
constexpr float HOPE_UNSETTLE = 0.4f;
/// Alone, silent and finding nothing new for QUIET_AFTER, it remarks on the
/// quiet: in its first QUIET_EARLY of life no oftener than QUIET_AGAIN, and
/// later only after QUIET_LONELY alone, no oftener than QUIET_LONELY_AGAIN.
constexpr float QUIET_AFTER = 15.0f;
constexpr float QUIET_EARLY = 180.0f;
constexpr float QUIET_AGAIN = 60.0f;
constexpr float QUIET_LONELY = 300.0f;
constexpr float QUIET_LONELY_AGAIN = 240.0f;
constexpr float HOPE_DRIVE_MAX = 1.3f;
/// A home is a known floor cell beside water or a table, no farther than
/// this walk; failing that, where it stands.
constexpr size_t HOME_FARTHEST = 40;
/// Slept safely in the same place this many times in a row, that place
/// will do for a home when there is no water near.
constexpr int HOME_SLEEPS = 2;
/// A settler keeps its place this far round home: anyone it doesn't like
/// (trust under TRUST_WELCOME) found inside while it is there is warned,
/// thought worse of, and gone for if still inside after GUARD_GRACE.
constexpr int HOME_GUARD = 6;
constexpr float GUARD_GRACE = 10.0f;
constexpr float TRUST_INTRUDER = -0.1f;
constexpr float TRUST_WELCOME = 0.15f;
/// Keeping its place: brave and grasping for it, sociable against.
constexpr float GUARD_BRAVE = 0.5f, GUARD_GREEDY = 0.5f, GUARD_SOCIABLE = 0.4f;
constexpr float GUARD_BAR = 0.3f;

/// The timid lose hope half as fast again, the fearless half as fast.
float sway(float t) { return 0.5f + t; }

} // namespace

float Human::gloom() const { return std::max(0.0f, (HOPE_LOW - escape_.hope) / HOPE_LOW); }

float Human::outDrive() const { return escape_.settled ? 0.0f : escape_.out * std::min(HOPE_DRIVE_MAX, escape_.hope / HOPE_START); }

void Human::lift(float how) {
    bool was = gloom() > 0.0f;
    escape_.hope = std::clamp(escape_.hope + (how < 0.0f ? how * sway(1.0f - traits.brave) : how), 0.0f, 1.0f);
    bool now = gloom() > 0.0f;
    if (now == was) return;
    note(now ? "STARTING TO THINK THERE IS NO WAY OUT" : "MAYBE THERE IS A WAY OUT AFTER ALL");
    remember(Episode::What::Despair, EPISODE_HOPE, id, -1, now ? 1 : 0);
    sayLater(now ? talk::Moment::Despair : talk::Moment::Hope);
}

// The chunk it woke in counts as seen, and lifts nothing.
void Human::enterChunk(const World& world) {
    int c = world.chunkIdAt(cell);
    if (c < 0 || !escape_.chunksIn.insert(c).second || escape_.chunksIn.size() == 1) return;
    int later = int(escape_.chunksIn.size()) - 1 - HOPE_FRESH;
    float lifts = std::max(HOPE_SAME_FLOOR, HOPE_NEW_CHUNK - HOPE_SAMEISH * float(std::max(0, later)));
    lift(lifts);
    // Out of where it woke, there is more of it; and the first new place that
    // lifts it no more, the sense that there is no end.
    if (escape_.chunksIn.size() == 2) {
        note("LEFT THE PLACE I WOKE IN. THERE'S MORE OF IT");
        sayLater(talk::Moment::ItGoesOn);
    } else if (later == int(std::lround(HOPE_NEW_CHUNK / HOPE_SAMEISH))) {
        note("ANOTHER PLACE LIKE THE LAST. IT NEVER ENDS");
        sayLater(talk::Moment::NeverEnds);
    }
    // The first few new places stand out; later ones blur together.
    remember(Episode::What::NewPlace, later < 0 ? EPISODE_NEW_PLACE : EPISODE_NEW_PLACE * 0.5f, -1, -1, c);
}

void Human::brood(const World& world) {
    if (clock_ - escape_.lastFound >= HOPE_STALE_TIME) {
        escape_.lastFound = clock_;
        lift(HOPE_STALE);
    }
    if (escape_.settled) {
        if (escape_.hope < HOPE_UNSETTLE) return;
        escape_.settled = false;
        escape_.intruder = -1;
        note("LEFT MY HOME TO LOOK FOR THE WAY OUT");
        rethink_ = true;
        return;
    }
    if (escape_.hope >= (isErratic() ? HOPE_SETTLE_ERRATIC : HOPE_SETTLE) || state == State::Waking) return;
    escape_.settled = true;
    escape_.home = cell;
    auto by = [&](Vec2i c, Tile t) {
        for (Vec2i d : DIRS4)
            if (memory.tile(c + d) == t) return true;
        return false;
    };
    // By water if it knows any near enough, else where it has slept safely
    // again and again (human_sleep.cpp), else by a table.
    bool byWater = false, bySleep = false;
    if (auto p = search(world, [&](Vec2i c) { return by(c, Tile::Water) || by(c, Tile::Basin) || by(c, Tile::Dispenser); });
        p && p->size() <= HOME_FARTHEST && !p->empty()) {
        escape_.home = p->back();
        byWater = true;
    } else if (auto r = sleep_.sleptYet && sleep_.safeSleeps >= HOME_SLEEPS ? search(world, [&](Vec2i c) { return c == sleep_.sleptAt; })
                                                                 : std::nullopt;
               r && r->size() <= HOME_FARTHEST) {
        escape_.home = sleep_.sleptAt;
        bySleep = true;
    } else if (auto q = search(world, [&](Vec2i c) { return by(c, Tile::Table); });
               q && q->size() <= HOME_FARTHEST && !q->empty()) {
        escape_.home = q->back();
    }
    note(byWater   ? "GAVE UP ON A WAY OUT. MADE A HOME BY THE WATER"
         : bySleep ? "GAVE UP ON A WAY OUT. MADE A HOME WHERE I SLEEP"
                   : "GAVE UP ON A WAY OUT. MADE A HOME");
    rememberAt(escape_.home, Episode::What::Settled, EPISODE_SETTLED, id);
    sayLater(talk::Moment::Settle);
    rethink_ = true;
}

bool Human::territorial() const {
    return isErratic() ||
           GUARD_BRAVE * traits.brave + GUARD_GREEDY * traits.greedy - GUARD_SOCIABLE * traits.sociable >= GUARD_BAR;
}

// Warned first, then gone for: anyone who walks off in time is left be.
bool Human::guardHome(World& world) {
    if (!escape_.settled || !territorial() || cheb(cell, escape_.home) > HOME_GUARD * 2) return false;
    Human* in = nullptr;
    // Those it walks with are never intruders, nor anyone it has let drink
    // (human_thirst.cpp).
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && sight.sees(o->cell) && cheb(o->cell, escape_.home) <= HOME_GUARD &&
            trust(o->id) < TRUST_WELCOME && !inGroupWith(*o) && !mayDrinkHere(o->id) && (!in || o->id == escape_.intruder))
            in = o.get();
    if (!in) {
        escape_.intruder = -1;
        return false;
    }
    if (in->id != escape_.intruder) {
        // Come for its water: let drink, for a cake or not, or told off.
        bool refused;
        if (askedToDrink(*in, refused)) return false;
        escape_.intruder = in->id;
        escape_.intrudedAt = clock_;
        regard(in->id, TRUST_INTRUDER);
        note("@ CAME INTO MY PLACE", in->id);
        if (!refused) sayLater(talk::Moment::Guard, in->id);
        return false;
    }
    if (clock_ - escape_.intrudedAt < GUARD_GRACE || fight_.foe == in->id) return false;
    fight_.foe = in->id;
    fight_.grudge = grudgeFor(in->id);
    note("WENT FOR @ TO DRIVE THEM OFF", in->id);
    return startFight(world);
}

void Human::hearQuiet(bool alone) {
    if (!alone) {
        escape_.aloneSince = clock_;
        return;
    }
    if (state == State::Waking || state == State::Sleeping || escape_.comingTo || talking()) return;
    bool early = clock_ < QUIET_EARLY;
    if (!early && clock_ - escape_.aloneSince < QUIET_LONELY) return;
    if (clock_ - escape_.quietAt < (early ? QUIET_AGAIN : QUIET_LONELY_AGAIN)) return;
    if (clock_ - escape_.lastFound < QUIET_AFTER || clock_ - escape_.aloneSince < QUIET_AFTER) return;
    escape_.quietAt = clock_;
    note("ALONE IN THE QUIET");
    sayLater(talk::Moment::Quiet);
    repeated(Repeat::Quiet);
}
