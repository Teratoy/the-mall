#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace {

/// Hungry and starving from here on, and too timid below this much
/// bravery, as in human_tuning.h: the timid would rather starve than eat flesh.
constexpr float HUNGRY = 0.5f;
constexpr float STARVING = 0.8f;
constexpr float TIMID = 0.3f;
/// A bar no hunger reaches.
constexpr float NEVER = 2.0f;
/// Erratic from here on; from ERRATIC_MAD not even friends are safe.
constexpr float ERRATIC = 0.5f;
constexpr float ERRATIC_MAD = 0.8f;
/// What pushes the mind toward erratic: each second starving, each second
/// of dread above DREAD_UNHINGES (times how far above), a half-eaten body
/// seen for the first time, someone seen eating a body.
constexpr float ERRATIC_STARVE = 1.0f / 120.0f;
constexpr float DREAD_UNHINGES = 0.6f;
constexpr float ERRATIC_DREAD = 0.02f;
constexpr float ERRATIC_GNAWED = 0.1f;
constexpr float ERRATIC_SAW_EAT = 0.05f;
/// Parched pushes it too, half as hard as starving, and harder the more it
/// craves flesh (up to twice as hard again); a meal of flesh takes this
/// much thirst off.
constexpr float ERRATIC_PARCHED = 1.0f / 240.0f;
constexpr float FLESH_DRINK = 0.1f;
/// Fed and calmer than this, erratic fades this much a second, though
/// never below this share of its appetite.
constexpr float CALM = 0.3f;
/// Gloom unhinges too, this much per second at its deepest.
constexpr float ERRATIC_DESPAIR = 0.004f;
constexpr float ERRATIC_FADE = 1.0f / 600.0f;
constexpr float ERRATIC_HELD = 0.8f;
/// Brave enough to eat the dead when starving, and to hunt the weak when
/// starving. Anyone short of timid eats the dead at death's door: starved
/// through and this hurt.
constexpr float BRAVE_FLESH = 0.6f;
constexpr float BRAVE_HUNT = 0.7f;
constexpr float DESPERATE = 0.15f;
/// What guilt adds to how hungry it must be before eating flesh again, and
/// what appetite takes off; it always takes some hunger.
constexpr float GUILT_BAR = 0.3f;
constexpr float APPETITE_BAR = 0.6f;
constexpr float BAR_MIN = 0.1f;
/// The sane brave hunt only those this weak, plus this much more with appetite.
constexpr float PREY_HEALTH = 0.35f;
constexpr float PREY_APPETITE = 0.2f;
/// Rotten flesh is eaten only with this much appetite.
constexpr float SPOILED_APPETITE = 0.4f;
/// Kneeling to eat; what a meal takes off hunger, and the share a rotten
/// one gives; what rotten flesh costs in health (never all of it), and how
/// long it keeps it from healing.
constexpr float FEED_TIME = 4.0f;
constexpr float FLESH_FOOD = 0.4f;
constexpr float SPOILED_FOOD = 0.5f;
constexpr float SICK_DAMAGE = 0.1f;
constexpr float SICK_LEAVES = 0.05f;
constexpr float SICK_TIME = 60.0f;
/// Fresh for this long after death, then spoiled; bones once this long dead.
constexpr float FRESH_TIME = 300.0f;
constexpr float BONES_TIME = 1800.0f;
/// What one meal leaves in the mind, all guilt for a steady mind, all
/// appetite for an erratic one, and how much heavier it weighs when it
/// killed them, had met them, or counted them a friend. Guilt fades over
/// half an hour, appetite over an hour.
constexpr float GUILT_PER_MEAL = 0.3f;
constexpr float APPETITE_PER_MEAL = 0.25f;
constexpr float WEIGHT_KILLED = 2.0f;
constexpr float WEIGHT_MET = 1.5f;
constexpr float WEIGHT_FRIEND = 2.0f;
constexpr float GUILT_FADE = 1.0f / 1800.0f;
constexpr float APPETITE_FADE = 1.0f / 3600.0f;
/// Scares: guilt per meal, the first half-eaten body, the first sight of
/// someone eating one, and the body it ate from seen again (times guilt).
constexpr float DREAD_GUILT = 0.3f;
constexpr float DREAD_GNAWED = 0.4f;
constexpr float DREAD_SAW_EAT = 0.5f;
constexpr float DREAD_HAUNT = 0.15f;
/// Guilty from here on, the body it ate from haunts it again at most this
/// often; craving from here on, it says so at most this often.
constexpr float HAUNT_GUILT = 0.2f;
constexpr float HAUNT_EVERY = 60.0f;
constexpr float CRAVING = 0.3f;
constexpr float CRAVE_EVERY = 30.0f;
/// Seen eating a body, the eater is trusted no more than this. A friend of
/// the eaten this brave goes for the eater.
constexpr float TRUST_EATER = -0.8f;
constexpr float BRAVE_AVENGE = 0.6f;
/// Fresh flesh bleeds as it is eaten: this many drops, this wide.
constexpr int FEED_DROPS = 3;
constexpr float FEED_DROP_MIN = 0.6f, FEED_DROP_MAX = 1.6f;
constexpr float BODY_HALF_PX = 8.0f;
/// Bodies this close to whoever it ran from are left alone while afraid,
/// as FLEE_DIST in human_tuning.h.
constexpr int FEARED_DIST = 10;

/// How much eating flesh, seeing it eaten, finding a gnawed body and the
/// mind snapping strike it, as episodes to talk about.
constexpr float EPISODE_ATE = 0.9f, EPISODE_SAW_EAT = 0.9f, EPISODE_GNAWED = 0.7f, EPISODE_SNAPPED = 0.8f;

} // namespace

bool Human::isErratic() const { return mind_.erratic >= ERRATIC; }

float Human::decay() const { return std::min(1.0f, deadFor() / BONES_TIME); }

bool Human::spoiled() const { return deadFor() >= FRESH_TIME; }

int Human::meals() const { return decay() >= 1.0f ? 0 : flesh; }

void Human::mind(float dt) {
    bool was = isErratic();
    if (starving()) mind_.erratic += ERRATIC_STARVE * dt;
    if (parched()) mind_.erratic += ERRATIC_PARCHED * (1.0f + 2.0f * mind_.appetite) * dt;
    if (dread_ > DREAD_UNHINGES) mind_.erratic += (dread_ - DREAD_UNHINGES) * ERRATIC_DREAD * dt;
    mind_.erratic += gloom() * ERRATIC_DESPAIR * dt;
    if (!hungry() && dread_ < CALM) mind_.erratic -= ERRATIC_FADE * dt;
    mind_.erratic = std::clamp(mind_.erratic, mind_.appetite * ERRATIC_HELD, 1.0f);
    mind_.guilt = std::max(0.0f, mind_.guilt - GUILT_FADE * dt);
    mind_.appetite = std::max(0.0f, mind_.appetite - APPETITE_FADE * dt);
    mind_.sick = std::max(0.0f, mind_.sick - dt);
    if (isErratic() == was) return;
    note(isErratic() ? "SOMETHING IN ME SNAPPED" : "FEEL MORE LIKE MYSELF");
    remember(Episode::What::Snapped, EPISODE_SNAPPED, id, -1, isErratic() ? 1 : 0);
    if (isErratic()) sayLater(talk::Moment::Snap);
}

// The erratic eat the dead when hungry, and never sooner however they crave
// it; the brave when starving, anyone else only at death's door; the timid
// never.
float Human::fleshBar() const {
    if (traits.brave < TIMID) return NEVER;
    float bar = isErratic()                 ? HUNGRY
                : traits.brave >= BRAVE_FLESH ? STARVING
                : health < DESPERATE          ? 1.0f
                                              : NEVER;
    if (bar >= NEVER) return NEVER;
    return std::max(isErratic() ? HUNGRY : BAR_MIN, bar + GUILT_BAR * mind_.guilt - APPETITE_BAR * mind_.appetite);
}

// The erratic hunt when hungry (never sooner), the bravest when starving;
// nobody too hurt to fight.
float Human::huntBar() const {
    if (traits.brave < TIMID || health < fleeHealth()) return NEVER;
    float bar = isErratic() ? HUNGRY : traits.brave >= BRAVE_HUNT ? STARVING : NEVER;
    if (bar >= NEVER) return NEVER;
    return std::max(isErratic() ? HUNGRY : BAR_MIN, bar + GUILT_BAR * mind_.guilt - APPETITE_BAR * mind_.appetite);
}

// Bodies near whoever it fears are left alone, as for looting; a friend
// only once mad.
bool Human::wouldEat(int id) const {
    auto it = deaths_.bodies.find(id);
    if (it == deaths_.bodies.end()) return false;
    const Body& b = it->second;
    if (b.meals <= 0 || (b.spoiled && mind_.appetite < SPOILED_APPETITE)) return false;
    // A body that smells puts off all but the erratic and the starving.
    if (!isErratic() && !starving() && smellsOfDeath(b.cell)) return false;
    if (isFriend(id) && mind_.erratic < ERRATIC_MAD) return false;
    auto f = social_.lastSeen.find(fight_.feared);
    return !(fight_.fear > 0.0f && f != social_.lastSeen.end() && cheb(b.cell, f->second) < FEARED_DIST);
}

std::optional<std::vector<Vec2i>> Human::fleshPath(const World& world, int* whose) const {
    std::vector<std::pair<int, Vec2i>> worth;
    for (const auto& [who, b] : deaths_.bodies)
        if (wouldEat(who)) worth.push_back({who, b.cell});
    if (worth.empty()) return std::nullopt;
    return search(world, [&](Vec2i c) {
        for (auto [who, at] : worth)
            if (cheb(c, at) <= 1) {
                if (whose) *whose = who;
                return true;
            }
        return false;
    });
}

bool Human::startFeed(World& world, int body) {
    std::optional<std::vector<Vec2i>> p;
    int whose = body;
    if (body >= 0) {
        // Its own kill it eats, however it stood with them.
        bool own = body == mind_.prey && deaths_.bodies.count(body) && deaths_.bodies[body].meals > 0;
        if (own || wouldEat(body)) {
            Vec2i at = deaths_.bodies[body].cell;
            p = search(world, [&](Vec2i c) { return cheb(c, at) <= 1; });
        }
    } else {
        p = fleshPath(world, &whose);
    }
    if (!p) {
        mind_.meal = -1;
        // None it knows of: one it has smelt, maybe.
        return body < 0 && followStench(world);
    }
    mind_.meal = whose;
    if (!p->empty()) {
        startWalking(Goal::Feed, *p);
        return true;
    }
    // Beside it: kneels and eats.
    const Human& b = world.human(mind_.meal);
    state = State::Feeding;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    if (b.cell.x != cell.x) facingLeft = b.cell.x < cell.x;
    timer = FEED_TIME;
    return true;
}

void Human::feed(World& world) {
    Human& b = world.human(mind_.meal);
    if (b.meals() <= 0 || cheb(cell, b.cell) > 1) {
        mind_.meal = -1;
        return startIdle(0.0f, 0.0f);
    }
    if (mind_.prey == b.id) mind_.prey = -1;
    bool rotten = b.spoiled();
    b.flesh--;
    hunger = std::max(0.0f, hunger - FLESH_FOOD * (rotten ? SPOILED_FOOD : 1.0f));
    takeIn(0.0f, 1.0f);
    thirst = std::max(0.0f, thirst - FLESH_DRINK);
    // Blood round the mouth and on the hands; fresh flesh bleeds under it.
    stain({BodyPart::Head}, 3, 150);
    stain({BodyPart::FrontArm}, 3, 150);
    if (!rotten) {
        float fx = b.cell.x * CELL_PX + CELL_PX / 2.0f, fy = float((b.cell.y + 1) * CELL_PX);
        for (int i = 0; i < FEED_DROPS; i++)
            world.blood().drop(world, fx + rng_.range(-BODY_HALF_PX, BODY_HALF_PX), fy - rng_.range(0.0f, 6.0f),
                               rng_.range(FEED_DROP_MIN, FEED_DROP_MAX), b.id, rng_);
    }

    // A steady mind takes it as guilt, an erratic one as a taste for more;
    // the more it knew of them, the heavier.
    float w = 1.0f;
    if (b.deaths_.killer == id) w *= WEIGHT_KILLED;
    if (met.count(b.id)) w *= WEIGHT_MET;
    if (isFriend(b.id)) w *= WEIGHT_FRIEND;
    float e = mind_.erratic;
    bool first = mind_.ateFrom.insert(b.id).second;
    // It haunts it once it has walked away, not at the meal.
    mind_.hauntedAt = clock_;
    mind_.guilt += w * GUILT_PER_MEAL * (1.0f - e);
    mind_.appetite = std::min(1.0f, mind_.appetite + w * APPETITE_PER_MEAL * e);
    scare(DREAD_GUILT * w * (1.0f - e));
    note(rotten ? "ATE FROM @'S ROTTEN BODY" : "ATE FROM @'S BODY", b.id);
    remember(Episode::What::Flesh, EPISODE_ATE, id, b.id);
    if (first) sayLater(talk::Moment::Feed, -1, b.id);
    if (rotten) {
        health = std::max(std::min(health, SICK_LEAVES), health - SICK_DAMAGE);
        mind_.sick = SICK_TIME;
        note("THE ROTTEN FLESH MADE ME SICK");
        sayLater(talk::Moment::Sick);
        repeated(Repeat::Low, b.id);
    }
    if (mind_.guilt > 1.0f) {
        mind_.erratic = std::min(1.0f, mind_.erratic + mind_.guilt - 1.0f);
        mind_.guilt = 1.0f;
        note("THE GUILT IS TOO MUCH");
    }

    // Seen at it, it is what it is to everyone who saw.
    for (const auto& o : world.humans()) {
        if (o->dead() || !(o.get() == this || o->sight.sees(cell))) continue;
        Body& seen = o->deaths_.bodies[b.id];
        seen.cell = b.cell;
        seen.cakes = b.cakes;
        seen.meals = b.meals();
        seen.spoiled = b.spoiled();
        if (o.get() != this) o->sawEat(*this, b);
    }

    // More while still hungry for it and there is more.
    if (hunger >= fleshBar() && b.meals() > 0 && wouldEat(b.id)) {
        state = State::Feeding;
        animTime = 0.0f;
        timer = FEED_TIME;
        return;
    }
    mind_.meal = -1;
    startIdle(0.5f, 1.0f);
}

// The steady brave go only for the weak, and only unseen; the erratic for
// anyone but a friend, until mad.
Human* Human::preyInSight(const World& world) const {
    bool sane = !isErratic();
    float weakest = PREY_HEALTH + PREY_APPETITE * mind_.appetite;
    Human* best = nullptr;
    int others = 0;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || !sight.sees(o->cell)) continue;
        others++;
        if (fight_.fear > 0.0f && o->id == fight_.feared) continue;
        if (isFriend(o->id) && (sane || mind_.erratic < ERRATIC_MAD)) continue;
        if (sane && o->health >= weakest) continue;
        if (!best || o->health < best->health ||
            (o->health == best->health && cheb(cell, o->cell) < cheb(cell, best->cell)))
            best = o.get();
    }
    if (sane && others > 1) return nullptr;
    return best;
}

bool Human::startHunt(World& world) {
    Human* p = preyInSight(world);
    if (!p) return false;
    bool fresh = mind_.prey != p->id;
    mind_.prey = fight_.foe = p->id;
    fight_.grudge = 0.0f;
    if (!startFight(world)) {
        mind_.prey = -1;
        return false;
    }
    if (fresh) {
        note("WENT FOR @ TO EAT THEM", p->id);
        sayLater(talk::Moment::Hunt, p->id);
    }
    return true;
}

// The first time it sees someone eat, they are a horror to it: feared,
// distrusted, and the timid keep away; a brave friend of the dead goes
// for them. The erratic take it more lightly.
void Human::sawEat(Human& eater, const Human& body) {
    mind_.gnawedSeen.insert(body.id);
    if (!mind_.eaters.insert(eater.id).second) return;
    note("SAW @ EATING #", eater.id, body.id);
    rememberAt(body.cell, Episode::What::Flesh, EPISODE_SAW_EAT, eater.id, body.id);
    bool mad = isErratic();
    scare(DREAD_SAW_EAT * (mad ? 0.5f : 1.0f));
    mind_.erratic = std::min(1.0f, mind_.erratic + ERRATIC_SAW_EAT);
    if (float t = trust(eater.id); t > TRUST_EATER) regard(eater.id, TRUST_EATER - t);
    sayLater(talk::Moment::SawEat, eater.id, body.id);
    rethink_ = true;
    if (mad) return;
    if (traits.brave < TIMID) {
        fight_.feared = eater.id;
        fight_.fear = fearTime();
    } else if (isFriend(body.id) && traits.brave >= BRAVE_AVENGE) {
        fight_.foe = eater.id;
        fight_.grudge = grudgeFor(eater.id);
        note("WENT FOR @ FOR WHAT THEY DID", eater.id);
    }
}

void Human::seeRemains(const Human& body) {
    if (mind_.ateFrom.count(body.id)) {
        if (mind_.guilt < HAUNT_GUILT || clock_ - mind_.hauntedAt < HAUNT_EVERY) return;
        mind_.hauntedAt = clock_;
        scare(DREAD_HAUNT * mind_.guilt);
        note("CAN'T STOP THINKING ABOUT @", body.id);
        sayLater(talk::Moment::Haunted, -1, body.id);
        return;
    }
    // Bare bones keep no marks of teeth: nobody can tell they were eaten from.
    if (!body.bones() && body.flesh < FLESH_MEALS && mind_.gnawedSeen.insert(body.id).second) {
        scare(DREAD_GNAWED);
        mind_.erratic = std::min(1.0f, mind_.erratic + ERRATIC_GNAWED);
        note("FOUND @'S BODY HALF EATEN", body.id);
        rememberAt(body.cell, Episode::What::Gnawed, EPISODE_GNAWED, -1, body.id);
        sayLater(talk::Moment::Gnawed, -1, body.id);
        return;
    }
    if (mind_.appetite >= CRAVING && hungry() && body.meals() > 0 && clock_ - mind_.cravedAt >= CRAVE_EVERY) {
        mind_.cravedAt = clock_;
        sayLater(talk::Moment::Craving, -1, body.id);
    }
}
