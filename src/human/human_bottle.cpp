#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

// Bottles: every human wakes with one, full, in a back pocket. Thirsty with
// water far off, it drinks from it where it stands; beside water it would
// drink, it fills it, first pouring out what it wouldn't drink, or what is
// clearly dirtier than the water before it, in a puddle that dries. The
// bottle is see-through: anyone who sees it sees how much is in it and how
// foul or red, and the water keeps whatever it was filled from. Thirsty with
// nothing to drink, a human asks someone close by it sees with water: they
// pour it some, sell it some for a cake, lie that they have none (seen
// through the glass), or refuse. Bottles are taken off the dead and snatched
// in a fight (human_fight.cpp), like cakes.

namespace {

/// A gulp from the bottle: this long, this much water and as much thirst
/// off, as at the water; it drinks until down to SIP_QUENCHED or dry.
constexpr float SIP_TIME = 1.5f;
constexpr float SIP_GULP = 0.35f;
constexpr float SIP_QUENCHED = 0.05f;
/// Kneeling at the water it fills this much a share, a share every
/// FILL_TIME; a bottle under FILL_TOPUP is worth topping up.
constexpr float FILL_TIME = 0.8f;
constexpr float FILL_SHARE = 0.25f;
constexpr float FILL_TOPUP = 0.9f;
/// Not thirsty, with no bottle fuller than BOTTLE_LOW, it wants to fill up
/// as much again as WANTING's Fill base, up to FILL_DRIVE more when all are
/// dry.
constexpr float BOTTLE_LOW = 0.5f;
constexpr float FILL_DRIVE = 1.0f;
/// Pouring a bottle out takes this long.
constexpr float POUR_TIME = 1.2f;
/// The puddle lands this far ahead of the feet, in pixels.
constexpr float POUR_AHEAD_PX = 2.0f;
/// Asking for a drink: of one in sight within ASK_SEEN, walking up to this
/// close first (or they'd walk off before hearing it), carrying at least
/// ASK_WATER_MIN, the
/// same one again only after ASK_DRINK_AGAIN; this likely for a middling
/// human (the sociable likelier), likelier by ASK_PARCHED when parched and
/// by ASK_FRIEND of a friend;
/// it waits this long for an answer. It asks only with no water it would
/// drink within NEAR_WATER steps.
constexpr size_t NEAR_WATER = 12;
constexpr int ASK_REACH = 3, ASK_SEEN = 12;
constexpr float ASK_WATER_MIN = 0.2f;
constexpr float ASK_DRINK_AGAIN = 45.0f;
/// Not asking this time, it may think again this much sooner.
constexpr float ASK_RETRY = 10.0f;
constexpr float ASK_DRINK_CHANCE = 0.5f, ASK_PARCHED = 0.3f, ASK_FRIEND = 0.3f;
constexpr float ASK_WAIT_MIN = 2.5f, ASK_WAIT_MAX = 3.5f;
/// Asked, it pours this much at most, and never more than half of what it
/// has; it shares freely if this sociable and not this greedy, to anyone
/// but a foe, as to a friend or one it walks with.
constexpr float GIVE_WATER = 0.35f;
constexpr float SHARE_SOCIABLE = 0.55f, SHARE_GREEDY = 0.45f;
/// Asks a cake for it from this greedy; lies that it has none if this
/// greedy and brave, to anyone not a friend.
constexpr float SELL_GREEDY = 0.5f;
constexpr float LIE_GREEDY = 0.5f, LIE_BRAVE = 0.4f;
/// Handing over water takes as long as handing over a cake.
constexpr float POUR_FOR_TIME = 0.8f;
/// What a drink poured does to the trust of whoever got it, and of whoever
/// saw it; a drink bought, both ways; a lie seen through, the asker's.
constexpr float TRUST_GAVE_WATER = 0.3f, TRUST_SAW_WATER = 0.08f;
constexpr float TRUST_SOLD_WATER = 0.05f;
constexpr float TRUST_BOTTLE_LIE = -0.4f;
/// Water this deep in a bottle is plain to see.
constexpr float SEE_WATER = 0.1f;
/// How much it strikes the one lied to.
constexpr float EPISODE_BOTTLE_LIE = 0.5f;

/// The fullest bottle, -1 if none holds any.
int fullest(const std::vector<Bottle>& bottles) {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++)
        if (!bottles[i].empty() && (best < 0 || bottles[i].water > bottles[size_t(best)].water)) best = int(i);
    return best;
}

/// The emptiest bottle not yet full, -1 if all are.
int emptiest(const std::vector<Bottle>& bottles, float under) {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++)
        if (bottles[i].water < under && (best < 0 || bottles[i].water < bottles[size_t(best)].water)) best = int(i);
    return best;
}

} // namespace

float Human::bottleWater() const {
    float sum = 0.0f;
    for (const Bottle& b : bottles) sum += b.water;
    return sum;
}

bool Human::bottleInHand() const {
    bool holds = state == State::Sipping || state == State::Filling || state == State::Pouring || givingWater();
    return holds && water_.inHand >= 0 && water_.inHand < int(bottles.size());
}

const Bottle* Human::shownBottle() const {
    if (bottleInHand()) return &bottles[size_t(water_.inHand)];
    if (bottles.empty()) return nullptr;
    int i = fullest(bottles);
    return &bottles[size_t(i < 0 ? 0 : i)];
}

int Human::sippable() const {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++)
        if (wouldSip(bottles[i]) && (best < 0 || bottles[i].water > bottles[size_t(best)].water)) best = int(i);
    return best;
}

bool Human::startSipping() {
    int i = sippable();
    if (i < 0) return false;
    if (state != State::Sipping) {
        note("DRANK FROM MY BOTTLE");
        sayLater(talk::Moment::Sip);
    }
    water_.inHand = i;
    state = State::Sipping;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    timer = SIP_TIME;
    return true;
}

// Anything new seen sends it back to choose(), as drinking at the water.
void Human::sip(World& world) {
    int i = water_.inHand;
    if (i < 0 || i >= int(bottles.size()) || !wouldSip(bottles[size_t(i)])) return choose(world);
    Bottle& b = bottles[size_t(i)];
    float foul = b.foul, red = b.red;
    float got = b.take(std::min(SIP_GULP, thirst));
    thirst = std::max(0.0f, thirst - got);
    takeIn(got / SIP_GULP, 0.0f);
    swallow(world, foul, red, std::nullopt, true);
    bool dry = b.empty();
    if (dry) {
        note("MY BOTTLE RAN DRY");
        sayLater(talk::Moment::BottleDry);
    }
    if (observe(world)) rethink_ = true;
    if (dry || rethink_ || thirst <= SIP_QUENCHED || fight_.fear > 0.0f || fight_.foe >= 0) {
        water_.inHand = -1;
        return choose(world);
    }
    timer = SIP_TIME;
}

bool Human::wantsFill(const World& world, Vec2i water) const {
    return emptiest(bottles, FILL_TOPUP) >= 0 || toPour(world, water) >= 0;
}

// What goes into the bottle is the water as it truly is there, seen or
// not; it sees it in the glass soon enough.
bool Human::startFilling(World& world, Vec2i water) {
    if (bottles.empty()) return false;
    bool toilet = world.toilet(water) != nullptr;
    if (toilet ? !parched() && !isErratic() : !wouldDrink(world, cell, water)) return false;
    int pour = toPour(world, water);
    int i = pour >= 0 ? pour : emptiest(bottles, FILL_TOPUP);
    if (i < 0) return false;
    bool fresh = state != State::Filling;
    water_.drinkAt = water;
    water_.inHand = i;
    goal = Goal::Fill;
    if (water.x != cell.x) facingLeft = water.x < cell.x;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    if (pour >= 0) {
        // Poured where it stands, a little ahead; the puddle spreads as it
        // goes, and the bottle is empty once done.
        Bottle& b = bottles[size_t(i)];
        float ahead = facingLeft ? -POUR_AHEAD_PX : POUR_AHEAD_PX;
        world.mess().pour(cell, (cell.x + 0.5f) * CELL_PX + ahead, (cell.y + 0.8f) * CELL_PX, b.water, world.time(),
                          id, uint32_t(rng_.next()), b.foul, b.red);
        note(b.foul >= b.red ? "POURED OUT MY BOTTLE. IT WAS FOUL" : "POURED OUT MY BOTTLE. IT WAS BLOODY");
        sayLater(talk::Moment::PourOut);
        state = State::Pouring;
        timer = POUR_TIME;
        return true;
    }
    if (fresh) {
        note(toilet ? "FILLED MY BOTTLE AT A TOILET" : "FILLED MY BOTTLE");
        sayLater(talk::Moment::FillBottle);
    }
    state = State::Filling;
    timer = FILL_TIME;
    return true;
}

void Human::pourOut(World& world) {
    int i = water_.inHand;
    if (i >= 0 && i < int(bottles.size())) bottles[size_t(i)] = {};
    if (startFilling(world, water_.drinkAt)) return;
    water_.inHand = -1;
    choose(world);
}

void Human::fill(World& world) {
    int i = water_.inHand;
    Vec2i w = water_.drinkAt;
    bool toilet = world.toilet(w) != nullptr;
    // Turned away, or the water went bad, since it knelt.
    if (i < 0 || i >= int(bottles.size()) || (toilet ? !parched() && !isErratic() : !wouldDrink(world, cell, w))) {
        water_.inHand = -1;
        return choose(world);
    }
    float foul, red;
    waterHere(world, w, foul, red);
    bottles[size_t(i)].add(FILL_SHARE, foul, red);
    if (observe(world)) rethink_ = true;
    if (rethink_ || fight_.fear > 0.0f || fight_.foe >= 0) {
        water_.inHand = -1;
        return choose(world);
    }
    if (!bottles[size_t(i)].full()) {
        timer = FILL_TIME;
        return;
    }
    // Full: the next, if any wants it.
    if (startFilling(world, w)) return;
    water_.inHand = -1;
    choose(world);
}

// Only water it would drink, and only when not thirsty: thirsty, it drinks
// first (seekWater), and fills after.
bool Human::seekFill(World& world) {
    if (thirsty() || bottles.empty()) return false;
    auto p = search(world, [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && wouldDrink(world, c, *w) && wantsFill(world, *w);
    }, true);
    if (!p) return false;
    if (p->empty()) return startFilling(world, *waterBeside(cell));
    startWalking(Goal::Fill, *p);
    return true;
}

float Human::fillDrive() const {
    if (bottles.empty() || thirsty()) return -1.0f;
    float most = 0.0f;
    for (const Bottle& b : bottles) most = std::max(most, b.water);
    if (most >= BOTTLE_LOW) return -1.0f;
    return FILL_DRIVE * (BOTTLE_LOW - most) / BOTTLE_LOW;
}

// As for cakes (nearestHolder): never whoever it ran from, and a suspect's
// water, or a fight at all for the timid, only tempts the parched.
const Human* Human::nearestWaterHolder(const World& world) const {
    const Human* best = nullptr;
    int bestDist = 0;
    for (auto [id, water] : social_.seenWater) {
        if (fight_.fear > 0.0f && id == fight_.feared) continue;
        if ((suspicious(id) || traits.brave < TIMID) && !parched()) continue;
        if (world.human(id).dead()) continue;
        // A foe's water looks nearer, a friend's farther.
        int d = cheb(cell, social_.lastSeen.at(id)) + int(trust(id) * TRUST_STEPS);
        if (!best || d < bestDist) best = &world.human(id), bestDist = d;
    }
    return best;
}

// Only when nothing it would drink is near; never a foe, or whoever it is
// afraid of. The answer comes when the asking is said (human_speech.cpp).
bool Human::askForDrink(World& world) {
    if (!thirsty() || sippable() >= 0) {
        water_.askee = -1;
        return false;
    }
    Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->state == State::Sleeping || o->bottleWater() < ASK_WATER_MIN ||
            !sight.sees(o->cell) || cheb(cell, o->cell) > ASK_SEEN)
            continue;
        if (isFoe(o->id) || (fight_.fear > 0.0f && o->id == fight_.feared)) continue;
        if (auto it = water_.askedDrink.find(o->id); it != water_.askedDrink.end() && clock_ - it->second < ASK_DRINK_AGAIN)
            continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    if (!best) {
        water_.askee = -1;
        return false;
    }
    // Made up its mind already: only the walking up is left.
    if (water_.askee != best->id) {
        water_.askee = -1;
        auto near = search(world, [&](Vec2i c) {
            auto w = waterBeside(c);
            return w && wouldDrink(world, c, *w);
        });
        if (near && near->size() <= NEAR_WATER) return false;
        float p = ASK_DRINK_CHANCE * (0.5f + traits.sociable) + (parched() ? ASK_PARCHED : 0.0f) +
                  (isFriend(best->id) ? ASK_FRIEND : 0.0f);
        if (!rng_.chance(p)) {
            water_.askedDrink[best->id] = clock_ - ASK_DRINK_AGAIN + ASK_RETRY;
            return false;
        }
        water_.askee = best->id;
    }
    if (cheb(cell, best->cell) > ASK_REACH) {
        Vec2i at = best->cell;
        auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) < ASK_REACH; });
        if (!p || p->empty()) {
            water_.askee = -1;
            water_.askedDrink[best->id] = clock_;
            return false;
        }
        startWalking(Goal::Ask, *p);
        return true;
    }
    water_.askee = -1;
    water_.askedDrink[best->id] = clock_;
    Saying s{talk::Moment::AskDrink, best->id};
    if (!sayLater(s)) return false;
    state = State::Idle;
    moving = false;
    path_.clear();
    timer = rng_.range(ASK_WAIT_MIN, ASK_WAIT_MAX);
    awaitLine();
    if (best->cell.x != cell.x) facingLeft = best->cell.x < cell.x;
    return true;
}

// Answered as it is asked, out loud; thirsty itself, it keeps its water.
void Human::answerDrink(World& world, Human& asker) {
    asker.note("ASKED @ FOR WATER", id);
    if (dead() || state == State::Sleeping || asker.dead()) return;
    bool busy = fight_.foe >= 0 || state == State::Punching || (state == State::Walking && goal == Goal::Flee);
    if (busy || bottleWater() < SEE_WATER) return;
    bool foe = isFoe(asker.id);
    bool close = isFriend(asker.id) || inGroupWith(asker);
    bool kind = close || (traits.sociable >= SHARE_SOCIABLE && traits.greedy <= SHARE_GREEDY);
    if (!foe && !thirsty() && kind && giveWater(world, asker, false)) return;
    if (!foe && !thirsty() && traits.greedy >= SELL_GREEDY && asker.cakes > 0 && giveWater(world, asker, true)) return;
    if (!isFriend(asker.id) && traits.greedy >= LIE_GREEDY && traits.brave >= LIE_BRAVE) {
        note("TOLD @ MY BOTTLE WAS EMPTY", asker.id);
        sayLater(talk::Moment::BottleLie, asker.id);
        // The bottle is see-through: the asker sees the water in it.
        if (!asker.sight.sees(cell) || bottleWater() < SEE_WATER) return;
        asker.note("@ SAID THEIR BOTTLE WAS EMPTY. IT WASN'T", id);
        asker.regard(id, TRUST_BOTTLE_LIE);
        asker.remember(Episode::What::Lied, EPISODE_BOTTLE_LIE, id, asker.id);
        asker.sayLater(talk::Moment::CaughtBottleLie, id);
        return;
    }
    note("WOULDN'T GIVE @ WATER", asker.id);
    asker.note("@ WOULDN'T GIVE ME WATER", id);
    sayLater(talk::Moment::RefuseDrink, asker.id);
}

// Poured from its fullest into the asker's emptiest; to one with no
// bottle, a spare one is handed over, with whatever is in it.
bool Human::giveWater(World& world, Human& o, bool sold) {
    int from = fullest(bottles);
    if (from < 0) return false;
    std::string what;
    if (o.bottles.empty()) {
        if (bottles.size() < 2) return false;
        int spare = from == 0 ? 1 : 0;
        // Something in it to drink, from the other if need be.
        if (bottles[size_t(spare)].water < GIVE_WATER) bottles[size_t(from)].pourInto(bottles[size_t(spare)], GIVE_WATER);
        o.bottles.push_back(bottles[size_t(spare)]);
        bottles.erase(bottles.begin() + spare);
        from = fullest(bottles);
        what = "A BOTTLE";
    } else {
        int to = emptiest(o.bottles, 1.0f);
        if (to < 0) return false;
        Bottle& b = bottles[size_t(from)];
        if (b.pourInto(o.bottles[size_t(to)], std::min(GIVE_WATER, b.water / 2.0f)) <= 0.0f) return false;
        what = "SOME WATER";
    }
    state = State::Giving;
    water_.givingWater = true;
    water_.inHand = from;
    social_.sharee = o.id;
    moving = false;
    path_.clear();
    if (o.cell.x != cell.x) facingLeft = o.cell.x < cell.x;
    timer = POUR_FOR_TIME;
    o.rethink_ = true;
    if (sold) {
        o.cakes--;
        cakes++;
        regard(o.id, TRUST_SOLD_WATER);
        o.regard(id, TRUST_SOLD_WATER);
        note("SOLD @ " + what + " FOR A CAKE", o.id);
        o.note("BOUGHT " + what + " FROM @ FOR A CAKE", id);
        sayLater(talk::Moment::SellDrink, o.id);
        return true;
    }
    note("GAVE @ " + what, o.id);
    o.note("@ GAVE ME " + what, id);
    o.regard(id, TRUST_GAVE_WATER);
    sayLater(talk::Moment::GiveDrink, o.id);
    o.sayLater(talk::Moment::Thanks, id);
    // Those who see it think better of the giver.
    for (const auto& h : world.humans())
        if (h.get() != this && h.get() != &o && !h->dead() && h->sight.sees(cell) && !h->isFoe(o.id))
            h->regard(id, TRUST_SAW_WATER);
    return true;
}
