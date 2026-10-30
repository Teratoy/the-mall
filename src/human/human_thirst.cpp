#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cstdlib>

// Thirst: grows with everything a human does, like hunger, but only water
// quenches it, and water stays where it is, but for what a bottle holds
// (human_bottle.cpp). A thirsty human goes to the nearest water it knows,
// pool, canal or basin, kneels at its edge and drinks a gulp at a time
// until quenched or something else comes first; with water far off, it
// drinks from its bottle instead.
// Water fouled with feces, piss or blood hurts whoever drinks it, and very foul
// water makes them sick; they go to cleaner water when they can see the
// difference, unless parched. Red water, and water by a body, put them off
// too, but for the parched, those with a taste for flesh (who drink blood
// gladly) and, by a body, the brave. Whoever washes in the water someone
// drinks from, or lives by, is thought the worse of. With no water to be
// had, the parched and the erratic drink from a toilet, as foul as it is
// dirty, and anyone who sees it thinks less of them.

namespace {

/// Each gulp takes this long and this much thirst off; it drinks until
/// down to QUENCHED.
constexpr float DRINK_GULP_TIME = 2.0f;
constexpr float DRINK_GULP = 0.35f;
constexpr float QUENCHED = 0.05f;
/// Water it would drink this many steps off or nearer is walked to, the
/// bottle saved; farther, it drinks from the bottle where it stands.
constexpr size_t BOTTLE_WALK = 12;
/// A bottle this much fouler or redder than the water it kneels at is
/// poured out, to fill it with the cleaner water.
constexpr float POUR_WORSE = 0.1f;
/// Each gulp of foul water costs this much health per foulness, never
/// leaving less than FOUL_SURVIVE; from FOUL_SICK on it makes it sick for
/// FOUL_SICK_TIME (no healing).
constexpr float FOUL_DAMAGE = 0.04f;
constexpr float FOUL_SURVIVE = 0.05f;
constexpr float FOUL_SICK = 0.5f;
constexpr float FOUL_SICK_TIME = 60.0f;
/// Water it sees is foul is as far off as this many tenths of a cell more
/// walking, per foulness, unless parched.
constexpr float FOUL_COST = 400.0f;
/// Won't drink water it sees is this foul at all, unless parched.
constexpr float FOUL_REFUSE = 0.3f;
/// Foul enough to taste: noted in the log.
constexpr float FOUL_NOTED = 0.1f;
/// Water this red is refused, unless parched or craving flesh (from
/// BLOOD_CRAVING); drunk anyway, it frightens this much.
constexpr float RED_WATER = 0.15f;
constexpr float BLOOD_CRAVING = 0.4f;
constexpr float RED_DREAD = 0.15f;
/// A body this close to where it would drink puts it off, but for the
/// brave (from BODY_BRAVE) and the parched.
constexpr int BODY_BY_WATER = 2;
constexpr float BODY_BRAVE = 0.6f;
/// Filth it knows of on and round the spot it would kneel at (FILTH_REACH
/// cells): the disgust of the piss and feces there (Mess::disgust: a fresh
/// pile about 1) and of blood (a cell covered, wet, BLOOD_FILTH; dry, a
/// third of that). From FILTH_BAR × (1.5 − modesty) it goes elsewhere, the
/// parched and erratic never; those craving flesh don't count the blood.
constexpr int FILTH_REACH = 1;
/// Toilet water is this foul per unit of the toilet's dirt; drinking from
/// one stops once no longer parched (the erratic go on). Anyone seeing it
/// thinks this much less of the drinker (half within its group).
constexpr float TOILET_FOUL = 0.5f;
constexpr float TRUST_TOILET_DRINK = -0.1f;
/// Cruel enough to mock it, as for an accident (human_relief.cpp).
constexpr float CRUEL_DRINK_GREEDY = 0.6f, CRUEL_DRINK_SOCIABLE = 0.4f;
constexpr float BLOOD_FILTH = 1.0f;
constexpr float FILTH_BAR = 0.25f;
/// Seeing someone wash in water it drinks from (thirsty) or lives by
/// (settled within OWN_WATER of home), it thinks this much less of them.
constexpr float TRUST_WASHED_IN = -0.1f, TRUST_WASHED_IN_HOME = -0.2f;
constexpr int OWN_WATER = 6;
/// A wash restarts often; it gives offence once in this long.
constexpr float OFFEND_AGAIN = 60.0f;
/// A settler's water is any within HOME_WATER of home. Someone thirsty come
/// for it: the sociable (from WATER_FREE) let drink, and anyone not greedy
/// lets the parched; the greedy (from WATER_PRICE) ask one with a cake for
/// one; the rest refuse. Let drink, it may for WATER_ALLOW seconds; refused,
/// it keeps off that water for WATER_REFUSED unless parched.
constexpr int HOME_WATER = 2;
constexpr float WATER_FREE = 0.45f, WATER_PRICE = 0.5f;
constexpr float WATER_ALLOW = 90.0f, WATER_REFUSED = 300.0f;
/// Asked a cake, the greedy and bold may drink and go without paying, this
/// likely at their greediest.
constexpr float CHEAT_GREEDY = 0.67f, CHEAT_BRAVE = 0.5f, CHEAT_CHANCE = 0.5f;
/// Paid, both think this much better of each other; cheated, the guard this
/// much worse, and goes for the cheat.
constexpr float TRUST_PAID_WATER = 0.1f, TRUST_CHEATED = -0.4f;
/// Telling of water: the stingy (from WATER_STINGY) keep it from all but
/// friends and those they walk with, the grasping (from WATER_SELL) sell it
/// to anyone else with a cake; the cunning (WATER_SELL greedy and
/// WATER_LIE_BRAVE brave) send a thirsty foe over spikes.
constexpr float WATER_STINGY = 0.67f, WATER_SELL = 0.5f, WATER_LIE_BRAVE = 0.4f;
/// Knowing no water it can get to, it asks those it walks with this often.
constexpr float ASK_WATER_EVERY = 30.0f;
/// Leading someone to drink: only those not loners (from LEAD_SOCIABLE),
/// healthy (LEAD_HEALTH), lead friends and those they walk with within
/// LEAD_REACH; seeing one adds LEAD_PARCHED to wanting to escort. Being
/// led there earns what being led to wash does (human_blood.cpp).
constexpr float LEAD_SOCIABLE = 0.3f, LEAD_HEALTH = 0.5f;
constexpr int LEAD_REACH = 6;
constexpr float LEAD_PARCHED = 1.5f;
/// Once there, it leaves them to it: it doesn't lead the same one again
/// for this long.
constexpr float LEAD_AGAIN = 30.0f;
/// Thirsty, new water lifts hope this much, once in HOPE_WATER_AGAIN; each
/// drink takes this much dread off.
constexpr float HOPE_WATER = 0.05f, HOPE_WATER_AGAIN = 60.0f;
constexpr float DRINK_CALM = 0.05f;
/// The near-water lesson: paid off by drinking within WATER_SOON of getting
/// thirsty; from TETHER_FROM of it, strolls are about the water it last
/// drank at; past WATER_TETHER cells from that water, exploring is wanted
/// up to TETHER_EXPLORE less, by the lesson.
constexpr float WATER_SOON = 60.0f;
constexpr float TETHER_FROM = 0.3f;
constexpr int WATER_TETHER = 30;
constexpr float TETHER_EXPLORE = 0.6f;

} // namespace

std::optional<Vec2i> Human::waterBeside(Vec2i c) const {
    for (Vec2i d : DIRS4)
        if (drinkable(memory.tile(c + d))) return c + d;
    return std::nullopt;
}

// Those with a taste for flesh don't mind it either: much of what fouls
// water is blood.
float Human::seenFoul(const World& world, Vec2i water) const {
    bool minds = !parched() && !isErratic() && mind_.appetite < BLOOD_CRAVING;
    return sight.sees(water) && minds ? world.foul(water) : 0.0f;
}

// Foul water it sees, red water it sees, and bodies it knows of by the
// water put it off.
bool Human::wouldDrink(const World& world, Vec2i from, Vec2i water) const {
    if (seenFoul(world, water) >= FOUL_REFUSE) return false;
    bool craving = mind_.appetite >= BLOOD_CRAVING || isErratic();
    if (sight.sees(water) && !parched() && !craving && world.blood().tint(water) >= RED_WATER) return false;
    if (parched()) return true;
    // A settler who turned it away from this water, not long ago.
    for (const auto& [home, at] : water_.refused)
        if (clock_ - at < WATER_REFUSED && cheb(home, water) <= OWN_WATER) return false;
    if (!isErratic() && filthAround(world, from, craving) >= FILTH_BAR * (1.5f - modesty())) return false;
    if (craving || traits.brave >= BODY_BRAVE) return true;
    for (const auto& [who, b] : deaths_.bodies)
        if (cheb(b.cell, from) <= BODY_BY_WATER) return false;
    return true;
}

float Human::filthAround(const World& world, Vec2i at, bool butBlood) const {
    float sum = 0.0f;
    for (int y = -FILTH_REACH; y <= FILTH_REACH; y++)
        for (int x = -FILTH_REACH; x <= FILTH_REACH; x++) {
            Vec2i c = at + Vec2i{x, y};
            if (const std::vector<int>* here = world.mess().at(c))
                for (int i : *here)
                    if (memory.mess.count(i)) sum += Mess::disgust(world.mess().all()[size_t(i)], world.time());
            if (butBlood) continue;
            if (auto it = memory.blood.find(c); it != memory.blood.end())
                sum += it->second.total() * BLOOD_FILTH * (1.0f + 2.0f * it->second.wet) / 3.0f;
        }
    return sum;
}

// Not where someone else stands: that one may be drinking there.
bool Human::seekWater(World& world) {
    auto foulness = [&](Vec2i c) {
        auto w = waterBeside(c);
        return w ? seenFoul(world, *w) : 0.0f;
    };
    auto p = search(world, [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && wouldDrink(world, c, *w);
    }, true,
                    [&](Vec2i c) { return 1 + int(foulness(c) * FOUL_COST); });
    if (p && p->empty()) {
        startDrinking(*waterBeside(cell));
        return true;
    }
    if (p && p->size() <= BOTTLE_WALK) {
        startWalking(Goal::Drink, *p);
        return true;
    }
    if (startSipping()) return true;
    if (p) {
        startWalking(Goal::Drink, *p);
        return true;
    }
    if (seekToilet(world)) return true;
    // Nothing to drink anywhere: the nearest it saw carrying water, as the
    // hungry go for cakes (human_bottle.cpp).
    if (askForDrink(world)) return true;
    if (bottles.size() >= size_t(BOTTLE_MAX)) return false;
    const Human* h = nearestWaterHolder(world);
    if (!h) return false;
    fight_.foe = h->id;
    fight_.grudge = 0.0f;
    if (!startFight(world)) return false;
    note("WENT FOR @'S WATER", h->id);
    return true;
}

void Human::startDrinking(Vec2i water) {
    if (state != State::Drinking) {
        note("DRANK WATER");
        sayLater(talk::Moment::Drink);
        dread_ = std::max(0.0f, dread_ - DRINK_CALM);
        // Found water soon after getting thirsty: keeping near it paid off.
        if (lessons.nearWater > 0.0f && clock_ - water_.thirstyAt < WATER_SOON) learn(lessons.nearWater, false);
    }
    water_.last = cell;
    water_.drankYet = true;
    goal = Goal::Drink;
    water_.drinkAt = water;
    if (water.x != cell.x) facingLeft = water.x < cell.x;
    state = State::Drinking;
    moving = false;
    path_.clear();
    timer = DRINK_GULP_TIME;
}

// Anything new seen sends it back to choose(), which has it go on drinking
// while still thirsty and nothing more pressing turns up.
void Human::drink(World& world) {
    const Toilet* toilet = world.toilet(water_.drinkAt);
    // Turned away, or the water went bad, since it knelt; at a toilet, no
    // longer desperate.
    if (toilet ? !parched() && !isErratic() : !wouldDrink(world, cell, water_.drinkAt)) return choose(world);
    thirst = std::max(0.0f, thirst - DRINK_GULP);
    takeIn(1.0f, 0.0f);
    if (toilet) {
        float foul = toilet->dirt * TOILET_FOUL;
        if (health > FOUL_SURVIVE) health = std::max(FOUL_SURVIVE, health - FOUL_DAMAGE * foul);
        if (foul >= FOUL_SICK && mind_.sick <= 0.0f) {
            mind_.sick = FOUL_SICK_TIME;
            note("THE TOILET WATER MADE ME SICK");
        }
        if (observe(world)) rethink_ = true;
        if (rethink_ || thirst <= QUENCHED || fight_.fear > 0.0f || fight_.foe >= 0) return choose(world);
        timer = DRINK_GULP_TIME;
        return;
    }
    swallow(world, world.foul(water_.drinkAt), world.blood().tint(water_.drinkAt), water_.drinkAt, false);
    if (observe(world)) rethink_ = true;
    if (rethink_ || thirst <= QUENCHED || fight_.fear > 0.0f || fight_.foe >= 0) {
        if (thirst <= QUENCHED || !thirsty()) payForWater(world);
        return choose(world);
    }
    timer = DRINK_GULP_TIME;
}

// The same for a gulp from a pool or from a bottle filled at one: red
// water frightens all but those craving flesh, foul water hurts, and very
// foul water sickens.
void Human::swallow(World&, float foul, float red, std::optional<Vec2i> at, bool fromBottle) {
    if (red >= RED_WATER) {
        bool glad = mind_.appetite >= BLOOD_CRAVING;
        if (fromBottle)
            note(glad ? "DRANK BLOODY WATER FROM MY BOTTLE, GLADLY" : "DRANK BLOODY WATER FROM MY BOTTLE");
        else
            note(glad ? "DRANK BLOODY WATER, GLADLY" : "DRANK BLOODY WATER");
        if (!glad) scare(RED_DREAD);
        sayLater(talk::Moment::BloodyWater);
    }
    if (foul <= 0.0f) return;
    if (foul >= FOUL_NOTED) {
        note(fromBottle ? "DRANK FOUL WATER FROM MY BOTTLE" : "DRANK FOUL WATER");
        sayLater(talk::Moment::FoulWater);
        // Strikes it more the fouler it is.
        if (at)
            rememberAt(*at, Episode::What::FoulWater, std::min(1.0f, foul));
        else
            remember(Episode::What::FoulWater, std::min(1.0f, foul));
    }
    if (health > FOUL_SURVIVE) health = std::max(FOUL_SURVIVE, health - FOUL_DAMAGE * foul);
    if (foul >= FOUL_SICK && mind_.sick <= 0.0f) {
        mind_.sick = FOUL_SICK_TIME;
        note("THE FOUL WATER MADE ME SICK");
    }
}

// What it sees through the glass, it minds as it would at the water.
bool Human::wouldSip(const Bottle& b) const {
    if (b.empty()) return false;
    if (parched() || isErratic() || mind_.appetite >= BLOOD_CRAVING) return true;
    return b.foul < FOUL_REFUSE && b.red < RED_WATER;
}

void Human::waterHere(const World& world, Vec2i water, float& foul, float& red) const {
    const Toilet* t = world.toilet(water);
    foul = t ? t->dirt * TOILET_FOUL : world.foul(water);
    red = t ? 0.0f : world.blood().tint(water);
}

// Only what it minds: the parched, the erratic and those craving flesh
// keep whatever they have. What it would fill with is the water as it truly
// is there, which it will see in the glass; only what is clearly worse than
// that is poured.
int Human::toPour(const World& world, Vec2i water) const {
    if (parched() || isErratic() || mind_.appetite >= BLOOD_CRAVING) return -1;
    float foul, red;
    waterHere(world, water, foul, red);
    int worst = -1;
    float most = 0.0f;
    for (size_t i = 0; i < bottles.size(); i++) {
        const Bottle& b = bottles[i];
        if (b.empty()) continue;
        float worse = std::max(b.foul - foul, b.red - red);
        if (worse >= POUR_WORSE && worse > most) worst = int(i), most = worse;
    }
    return worst;
}

// Only those who see it, and not the erratic: the thirsty, and anyone who
// lives by that water.
void Human::offendDrinkers(World& world, Vec2i water) {
    if (clock_ - water_.offendedAt < OFFEND_AGAIN) return;
    water_.offendedAt = clock_;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->isErratic() || !o->sight.sees(cell)) continue;
        bool home = o->settled() && cheb(o->home(), water) <= OWN_WATER;
        if (!home && !o->thirsty()) continue;
        o->regard(id, home ? TRUST_WASHED_IN_HOME : TRUST_WASHED_IN);
        o->note(home ? "@ WASHED IN MY WATER" : "@ WASHED IN THE WATER I DRINK", id);
        o->sayLater(talk::Moment::Fouled, id);
    }
}

bool Human::othersWater(const World& world, Vec2i water) const {
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && o->settled() && sight.sees(o->cell) &&
            cheb(o->home(), water) <= OWN_WATER)
            return true;
    return false;
}

bool Human::mayDrinkHere(int who) const {
    auto it = water_.letDrink.find(who);
    return it != water_.letDrink.end() && clock_ < it->second;
}

// Only for water by its home, and only someone thirsty; the rest of
// guarding is guardHome's (human_hope.cpp).
bool Human::askedToDrink(Human& o, bool& refused) {
    refused = false;
    if (!o.thirsty() || isErratic()) return false;
    bool water = false;
    for (int y = -HOME_WATER; y <= HOME_WATER && !water; y++)
        for (int x = -HOME_WATER; x <= HOME_WATER && !water; x++) water = drinkable(memory.tile(escape_.home + Vec2i{x, y}));
    if (!water) return false;
    bool foe = isFoe(o.id);
    if (!foe && (traits.sociable >= WATER_FREE || (o.parched() && traits.greedy < WATER_PRICE))) {
        water_.letDrink[o.id] = clock_ + WATER_ALLOW;
        note("LET @ DRINK MY WATER", o.id);
        sayLater(talk::Moment::WaterFree, o.id);
        return true;
    }
    if (!foe && traits.greedy >= WATER_PRICE && o.cakes > 0) {
        water_.letDrink[o.id] = clock_ + WATER_ALLOW;
        o.water_.owes = id;
        note("ASKED @ A CAKE FOR MY WATER", o.id);
        sayLater(talk::Moment::WaterPrice, o.id);
        return true;
    }
    o.water_.refused.push_back({escape_.home, o.clock_});
    note("TURNED @ AWAY FROM MY WATER", o.id);
    sayLater(talk::Moment::MyWater, o.id);
    refused = true;
    return false;
}

// The greedy and bold may walk off instead; a guard who sees it goes for
// them.
void Human::payForWater(World& world) {
    if (water_.owes < 0) return;
    Human& guard = world.human(water_.owes);
    water_.owes = -1;
    if (guard.dead()) return;
    bool cheat = cakes == 0 ||
                 (traits.greedy >= CHEAT_GREEDY && traits.brave >= CHEAT_BRAVE && rng_.chance(CHEAT_CHANCE));
    if (!cheat) {
        cakes--;
        guard.cakes++;
        regard(guard.id, TRUST_PAID_WATER);
        guard.regard(id, TRUST_PAID_WATER);
        note("PAID @ A CAKE FOR WATER", guard.id);
        guard.note("@ PAID A CAKE FOR MY WATER", id);
        return;
    }
    note("DRANK @'S WATER AND DIDN'T PAY", guard.id);
    if (!guard.sight.sees(cell)) return;
    guard.regard(id, TRUST_CHEATED);
    guard.note("@ DRANK MY WATER AND DIDN'T PAY", id);
    guard.sayLater(talk::Moment::WaterCheat, id);
    guard.fight_.foe = id;
    guard.fight_.grudge = guard.grudgeFor(id);
    guard.rethink_ = true;
}

std::optional<Human::Tell> Human::waterRoute(const World& world, const Human& o, bool lie) const {
    auto unseen = [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && o.memory.tile(*w) == Tile::Void;
    };
    if (!lie) {
        auto p = search(world, unseen);
        if (!p) return std::nullopt;
        Tell t{Tell::Kind::Water, *waterBeside(p->empty() ? cell : p->back())};
        t.route.push_back(cell);
        t.route.insert(t.route.end(), p->begin(), p->end());
        return t;
    }
    // To beside a trap o doesn't know, over it, and on to water.
    for (const auto& [plate, kind] : memory.traps) {
        if (!bait(plate) || o.memory.traps.count(plate)) continue;
        auto there = search(world, [&](Vec2i c) { return c != plate && cheb(c, plate) == 1; });
        if (!there) continue;
        auto on = searchFrom(world, plate, [&](Vec2i c) { return waterBeside(c).has_value(); }, false);
        if (!on || on->empty()) continue;
        Tell t{Tell::Kind::Water, *waterBeside(on->back()), kind};
        t.lie = true;
        t.route.push_back(cell);
        t.route.insert(t.route.end(), there->begin(), there->end());
        t.route.push_back(plate);
        t.route.insert(t.route.end(), on->begin(), on->end());
        return t;
    }
    return std::nullopt;
}

std::optional<Human::Tell> Human::waterFor(const World& world, const Human& o, bool& forPay) const {
    forPay = false;
    if (!o.thirsty()) return std::nullopt;
    if (isFoe(o.id)) {
        if (traits.greedy < WATER_SELL || traits.brave < WATER_LIE_BRAVE) return std::nullopt;
        return waterRoute(world, o, true);
    }
    bool close = isFriend(o.id) || inGroupWith(o);
    if (!close && traits.greedy >= WATER_STINGY) return std::nullopt;
    bool selling = !close && traits.greedy >= WATER_SELL;
    if (selling && o.cakes == 0) return std::nullopt;
    auto t = waterRoute(world, o, false);
    if (t) forPay = selling;
    return t;
}

void Human::askGroupForWater(World& world) {
    if (clock_ - water_.askedAt < ASK_WATER_EVERY) return;
    water_.askedAt = clock_;
    if (search(world, [&](Vec2i c) {
            auto w = waterBeside(c);
            return w && wouldDrink(world, c, *w);
        }))
        return;
    // Asked once, of the nearest; whoever knows answers.
    Human* nearest = nullptr;
    Human* knows = nullptr;
    std::optional<Tell> way;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || !sight.sees(o->cell) || !inGroupWith(*o)) continue;
        if (!nearest || cheb(cell, o->cell) < cheb(cell, nearest->cell)) nearest = o.get();
        if (!knows)
            if ((way = o->waterRoute(world, *this, false))) knows = o.get();
    }
    if (!nearest) return;
    sayLater(talk::Moment::AskWater, nearest->id);
    if (knows) knows->tell(*this, *way);
}

bool Human::wouldLeadToDrink(const Human& o) const {
    if (&o == this || o.dead() || traits.sociable < LEAD_SOCIABLE || health < LEAD_HEALTH || hungry() || fight_.foe >= 0 ||
        fight_.fear > 0.0f || escorting_.escortee >= 0 || escorting_.by >= 0)
        return false;
    if (!isFriend(o.id) && !inGroupWith(o)) return false;
    if (auto it = escorting_.ledToDrink.find(o.id); it != escorting_.ledToDrink.end() && clock_ - it->second < LEAD_AGAIN) return false;
    if (isFoe(o.id) || o.isFoe(id) || o.escorting_.by >= 0 || o.escorting_.escortee >= 0 || o.fight_.foe >= 0 || o.fight_.fear > 0.0f) return false;
    // Parched, or thirsty and not already on the way to water, with nothing
    // to drink in a bottle.
    bool needs = (o.parched() || (o.thirsty() && o.goal != Goal::Drink)) && o.sippable() < 0;
    if (!needs || o.state == State::Drinking) return false;
    return sight.sees(o.cell) && cheb(cell, o.cell) <= LEAD_REACH;
}

float Human::parchedToLead(const World& world) const {
    for (const auto& o : world.humans())
        if (wouldLeadToDrink(*o)) return LEAD_PARCHED;
    return 0.0f;
}

// To water it would drink itself, nearest to it; the leading itself is
// leadEscort's (human_blood.cpp), as for washing.
bool Human::offerDrinkEscort(World& world) {
    Human* o = nullptr;
    for (const auto& h : world.humans())
        if (wouldLeadToDrink(*h) && (!o || cheb(cell, h->cell) < cheb(cell, o->cell))) o = h.get();
    if (!o) return false;
    auto p = search(world, [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && wouldDrink(world, c, *w);
    });
    if (!p) return false;
    escorting_.to = p->empty() ? cell : p->back();
    escorting_.escortee = o->id;
    escorting_.arrived = false;
    escorting_.drink = true;
    o->escorting_.by = id;
    o->rethink_ = true;
    if (!leadEscort(world)) return false;
    escorting_.ledToDrink[o->id] = clock_;
    note("TOOK @ TO WATER TO DRINK", o->id);
    o->note("@ TOOK ME TO WATER TO DRINK", id);
    sayLater(talk::Moment::LeadDrink, o->id);
    o->sayLater(talk::Moment::LedDrink, id);
    return true;
}

void Human::gotThirsty() {
    water_.thirstyAt = clock_;
    sayLater(talk::Moment::Thirsty);
}

void Human::gotParched(const World& world) {
    bool reachable = search(world, [&](Vec2i c) {
                         auto w = waterBeside(c);
                         return w && wouldDrink(world, c, *w);
                     }).has_value();
    if (!reachable) learn(lessons.nearWater, true);
}

void Human::foundWater() {
    if (clock_ - water_.hopeAt < HOPE_WATER_AGAIN) return;
    water_.hopeAt = clock_;
    lift(HOPE_WATER);
}

Vec2i Human::strollFrom() {
    if (escape_.settled) return escape_.home;
    // Drowsy, it strolls about where it means to sleep (human_sleep.cpp).
    if (auto base = sleepBase(); base && rng_.chance(drowsy())) return *base;
    if (water_.drankYet && lessons.nearWater >= TETHER_FROM && rng_.chance(lessons.nearWater)) return water_.last;
    return cell;
}

float Human::waterTether() const {
    if (!water_.drankYet || cheb(cell, water_.last) <= WATER_TETHER) return 0.0f;
    return TETHER_EXPLORE * lessons.nearWater;
}

// Only for the parched and the erratic, and only once there is no water;
// from the floor in front, as for going.
bool Human::seekToilet(World& world) {
    if (!parched() && !isErratic()) return false;
    auto p = search(world, [&](Vec2i c) { return toiletBefore(world, c).has_value(); }, true);
    if (!p) return false;
    if (!p->empty()) {
        startWalking(Goal::Drink, *p);
        return true;
    }
    Vec2i toilet = *toiletBefore(world, cell);
    startDrinking(toilet);
    note("DRANK FROM A TOILET");
    sayLater(talk::Moment::ToiletDrink);
    // Seen at it, it is thought the less of; the cruel say so.
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->isErratic() || !o->sight.sees(cell)) continue;
        bool group = o->inGroupWith(*this);
        o->regard(id, TRUST_TOILET_DRINK * (group ? 0.5f : 1.0f));
        o->note("SAW @ DRINK FROM A TOILET", id);
        if (o->traits.greedy >= CRUEL_DRINK_GREEDY && o->traits.sociable <= CRUEL_DRINK_SOCIABLE)
            o->sayLater(talk::Moment::ToiletMock, id);
    }
    return true;
}
