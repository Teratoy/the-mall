// Cakes and blows: eating, sharing, fighting, being hit and hurt by traps,
// running away, and going through the dead.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <queue>

// Someone last seen carrying cakes is gone after when nearer than any
// cake lying about. Cakes near someone this human ran from are left alone
// while it is still afraid.
bool Human::seekFood(World& world) {
    auto it = social_.lastSeen.find(fight_.feared);
    bool afraid = fight_.fear > 0.0f && it != social_.lastSeen.end();
    auto p = search(world, [&](Vec2i c) {
        auto k = inReach(c, memory.cakes);
        return k && !(afraid && cheb(*k, it->second) < FLEE_DIST);
    });
    // A body with cakes on it does as well as cakes lying about.
    auto q = lootPath(world, true);
    size_t nearest = std::min(p ? p->size() : SIZE_MAX, q ? q->size() : SIZE_MAX);
    const Human* h = nearestHolder(world);
    if (h && (nearest == SIZE_MAX || cheb(cell, social_.lastSeen[h->id]) < int(nearest))) {
        // Not angry at them, only after their cakes.
        fight_.foe = h->id;
        fight_.grudge = 0.0f;
        if (startFight(world)) return true;
    }
    if (q && (!p || q->size() < p->size())) return startLoot(world, true);
    if (!p) return false;
    startWalking(Goal::FetchCake, *p);
    return true;
}

void Human::startEating() {
    state = State::Eating;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    timer = EAT_TIME;
}

// Snatched away mid-bite, nothing is eaten.
void Human::endEating() {
    // Eating with more cakes still in hand bears out the lesson to keep them.
    if (cakes > 1 && lessons.hoard > 0.0f) learn(lessons.hoard, false);
    if (cakes > 0) {
        cakes--;
        hunger = std::max(0.0f, hunger - CAKE_FOOD);
        takeIn(0.0f, 1.0f);
        note("ATE A CAKE");
    }
    startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
}

const Human* Human::nearestHolder(const World& world) const {
    const Human* best = nullptr;
    int bestDist = 0;
    for (auto [id, n] : social_.seenCakes) {
        if (fight_.fear > 0.0f && id == fight_.feared) continue;
        // A suspect's cakes only tempt the starving, and so does a fight
        // at all, to the timid.
        if ((suspicious(id) || traits.brave < TIMID) && !starving()) continue;
        // A foe's cakes look nearer, a friend's farther.
        int d = cheb(cell, social_.lastSeen.at(id)) + int(trust(id) * TRUST_STEPS);
        // Asleep, they can't fight back.
        if (const Human& o = world.human(id); o.state == State::Sleeping && sight.sees(o.cell)) d -= SLEEPER_NEARER;
        if (!best || d < bestDist) best = &world.human(id), bestDist = d;
    }
    return best;
}

// Moved by their plea, it gives as to a friend, to the merely hungry too,
// and to the weak as well.
bool Human::wouldShare(const Human& other) const {
    bool moved = movedBy(other.id);
    return cakes > (isFriend(other.id) || moved ? 1 : spareFrom()) && !starving() &&
           (other.starving() || (moved && other.hungry())) && other.cakes == 0 &&
           (other.health >= preyHealth() || moved) && !suspicious(other.id) && !isFoe(other.id);
}

bool Human::helpStarving(World& world) {
    if (cakes <= 1 || starving()) return false;
    const Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->cakes > 0 || !sight.sees(o->cell)) continue;
        if (!o->starving() && !(movedBy(o->id) && o->hungry())) continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    if (!best) return false;
    bool moved = movedBy(best->id);
    // A foe gets beaten up however well they look; anyone else only when weak.
    if (!moved && (best->health < preyHealth() || isFoe(best->id))) {
        fight_.foe = best->id;
        fight_.grudge = grudgeFor(best->id);
        return startFight(world);
    }
    if (cakes <= spareFrom() && !isFriend(best->id) && !moved) return false;
    social_.sharee = best->id;
    return startShare(world);
}

bool Human::startShare(World& world) {
    Human& o = world.human(social_.sharee);
    if (!wouldShare(o) || !sight.sees(o.cell)) {
        social_.sharee = -1;
        return false;
    }
    if (cheb(cell, o.cell) <= 1) {
        give(o, Gift::Share);
        // Those who see it think better of the giver.
        witness(world, *this, o, TRUST_SAW_FEED, 0.0f, Episode::What::Gift, EPISODE_SAW_GIFT, int(Gift::Share));
        return true;
    }
    auto p = search(world, [&](Vec2i c) { return c != o.cell && cheb(c, o.cell) <= 1; });
    if (!p) {
        social_.sharee = -1;
        return false;
    }
    startWalking(Goal::Share, *p);
    return true;
}

// The cake changes hands at once; the other eats it when next it thinks.
// Handed over under threat, it ends their fight for it, and earns no thanks.
void Human::give(Human& other, Gift why) {
    state = State::Giving;
    moving = false;
    path_.clear();
    if (other.cell.x != cell.x) facingLeft = other.cell.x < cell.x;
    timer = GIVE_TIME;
    cakes--;
    other.cakes++;
    other.rethink_ = true;
    social_.sharee = -1;
    switch (why) {
    case Gift::Share:
        // Fed by the one it was after, it lets them be.
        if (other.fight_.foe == id) other.fight_.foe = -1;
        note("GAVE @ A CAKE", other.id);
        other.note("@ GAVE ME A CAKE", id);
        remember(Episode::What::Gift, EPISODE_GAVE, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_GIVEN, id, other.id, int(why));
        other.regard(id, TRUST_FED);
        // One plea, one cake.
        voice_.moved.erase(other.id);
        sayLater(talk::Moment::Give, other.id);
        other.sayLater(talk::Moment::Thanks, id);
        break;
    case Gift::Pay:
        note("PAID @ A CAKE FOR NEWS", other.id);
        other.note("@ PAID ME A CAKE FOR NEWS", id);
        remember(Episode::What::Gift, EPISODE_PAID, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_PAID, id, other.id, int(why));
        other.regard(id, TRUST_PAID);
        sayLater(talk::Moment::Pay, other.id);
        break;
    case Gift::Yield:
        note("GAVE @ A CAKE UNDER THREAT", other.id);
        other.note("@ GAVE ME A CAKE UNDER THREAT", id);
        remember(Episode::What::Gift, EPISODE_YIELDED, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_YIELDED, id, other.id, int(why));
        if (other.fight_.foe == id) other.fight_.foe = -1;
        break;
    }
}

// The fight goes on while the foe has hit this human lately, or while it
// is hungry and knows they carry cakes. It is off once they are seen dead
// or running, and a foe only remembered is looked for where they were last seen.
bool Human::startFight(World& world) {
    Human& f = world.human(fight_.foe);
    bool seen = sight.sees(f.cell);
    bool running = seen && f.state == State::Walking && f.goal == Goal::Flee;
    bool forCakes = hungry() && social_.seenCakes.count(fight_.foe) > 0;
    // Hunted, they are chased however they run, as long as they are in
    // sight or were lately.
    bool hunt = hunting() && social_.lastSeen.count(fight_.foe);
    if ((seen && f.dead()) || (!hunt && (running || (fight_.grudge <= 0.0f && !forCakes) || (!seen && !forCakes)))) {
        fight_.foe = -1;
        return false;
    }
    Vec2i at = seen ? f.cell : social_.lastSeen[fight_.foe];
    // Only after their cakes: a word first may be enough.
    if (!hunt && seen && fight_.grudge <= 0.0f && cheb(cell, at) <= THREAT_RANGE && threaten(f)) return true;
    if (cheb(cell, at) <= 1) {
        if (seen) {
            startPunch(f);
            return true;
        }
        // Got where they were; they're gone.
        social_.seenCakes.erase(fight_.foe);
        if (hunting()) mind_.prey = -1;
        fight_.foe = -1;
        return false;
    }
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= 1; });
    if (!p) {
        fight_.foe = -1;
        return false;
    }
    startWalking(Goal::Fight, *p);
    return true;
}

void Human::startPunch(Human& foe) {
    state = State::Punching;
    moving = false;
    path_.clear();
    punchLanded = false;
    if (foe.cell.x != cell.x) facingLeft = foe.cell.x < cell.x;
    timer = PUNCH_WINDUP;
}

// Lands only if the foe is still within reach, and snatches one of their
// cakes when there is room for it.
void Human::landPunch(World& world) {
    punchLanded = true;
    timer = PUNCH_FOLLOW;
    if (fight_.foe < 0) return;
    Human& f = world.human(fight_.foe);
    // Aimed where they are seen, which is still the cell left until halfway through a step.
    Vec2i at = f.stepProgress() < 0.5f ? f.prev : f.cell;
    if (at.x != cell.x) facingLeft = at.x < cell.x;
    if (f.dead() || cheb(cell, at) > PUNCH_REACH) return;
    bool took = f.cakes > 0 && cakes < carryMax();
    if (took) {
        f.cakes--;
        cakes++;
    }
    note(took ? "HIT @ AND TOOK A CAKE" : "HIT @", f.id);
    f.note(took ? "@ HIT ME AND TOOK A CAKE" : "@ HIT ME", id);
    remember(Episode::What::Fight, EPISODE_HIT, id, f.id);
    f.remember(Episode::What::Fight, took ? EPISODE_ROBBED : EPISODE_GOT_HIT, id, f.id);
    sayLater(talk::Moment::Hit, f.id);
    // Wet blood on them comes off on the fist.
    if (f.stains.wetAny()) stain({BodyPart::FrontArm}, 1, 120, true);
    bool asleep = f.state == State::Sleeping;
    if (asleep) note("HIT @ IN THEIR SLEEP", f.id);
    f.takeHit(world, *this, rng_.range(PUNCH_DAMAGE_MIN, PUNCH_DAMAGE_MAX));
    // Set on asleep, it never stood a chance: seen, that is as bad as a killing.
    if (asleep)
        witness(world, *this, f, TRUST_SAW_SLEEPER_HIT, DREAD_SAW_SLEEPER_HIT, Episode::What::Fight, EPISODE_SAW_KILL);
    else
        witness(world, *this, f, TRUST_SAW_HIT, DREAD_SAW_HIT, Episode::What::Fight, EPISODE_SAW_FIGHT);
    observe(world);
}

void Human::creditKill(World& world, Human& by, bool witnesses) {
    deaths_.killer = by.id;
    if (witnesses)
        for (const auto& h : world.humans())
            if (h.get() != this && h.get() != &by && !h->dead() && h->sight.sees(cell))
                h->rememberAt(cell, Episode::What::Kill, EPISODE_SAW_KILL, by.id, id);
    // Bled out out of its sight, it only learns of it at the body.
    deaths_.killerSaw = witnesses || (!by.dead() && by.sight.sees(cell));
    if (!deaths_.killerSaw) return;
    by.note(by.hunting() ? "KILLED @ TO EAT" : "KILLED @", id);
    by.remember(Episode::What::Kill, EPISODE_KILLED, by.id, id);
    by.sayLater(talk::Moment::Killed, -1, id);
}

void Human::takeHit(World& world, Human& from, float damage) {
    if (dead()) return;
    bool asleep = state == State::Sleeping;
    health -= damage;
    hurtTime = HURT_TIME;
    regard(from.id, TRUST_HIT);
    scare(DREAD_HIT);
    if (health <= 0.0f) {
        note("KILLED BY @", from.id);
        creditKill(world, from, true);
        return die(world);
    }
    // Hit in its sleep: wakes with a start, slow to answer at first.
    if (asleep) {
        wakeUp(world, true, from.id, "WOKE UP TO @ HITTING ME");
        scare(DREAD_WOKEN);
        lift(HOPE_WOKEN_HIT);
        sleep_.stunned = WOKEN_STUN;
        escape_.daze = std::max(escape_.daze, WOKEN_DAZE);
        blameWatcher(world);
    }
    sayLater(talk::Moment::Ouch, from.id);
    // The blow that tips a beating over opens a wound as deep as the whole
    // of it; each after only deepens it.
    bool open = fight_.beating > BEATING_BLEED;
    fight_.beating += damage;
    if (fight_.beating > BEATING_BLEED) {
        wound(WoundKind::Blunt, BodyPart::Head, open ? damage : fight_.beating, true, from.id);
        gore_.sprays.push_back({float(cell.x - from.cell.x), float(cell.y - from.cell.y), damage, BodyPart::Head});
    }
    // Already trading blows with them: keep at it.
    if (state == State::Punching && fight_.foe == from.id && !weak(*this)) {
        fight_.grudge = grudgeFor(from.id);
        return;
    }
    fight_.provoked = true;
    fight_.attacker = from.id;
}

// Flying chest high, a dart takes the chest or, from the side, the arm on
// the side it came from.
void Human::hitByDart(float damage, Vec2i dir) {
    if (dead()) return;
    if (health > DART_SURVIVE_HEALTH) health = std::max(DART_SURVIVE_HEALTH, health - damage);
    hurtTime = HURT_TIME;
    note("A DART HIT ME");
    remember(Episode::What::Trap, EPISODE_DART, id, -1, int(TrapKind::Darts));
    scare(DREAD_TRAP_HIT);
    sayLater(talk::Moment::Darted);
    BodyPart part = BodyPart::Torso;
    if (dir.x != 0 && rng_.chance(0.5f)) part = (dir.x > 0) == facingLeft ? BodyPart::FrontArm : BodyPart::BackArm;
    wound(WoundKind::Pierce, part, damage, false);
    gore_.sprays.push_back({float(dir.x), float(dir.y), damage, part});
    rethink_ = true;
}

void Human::impale(World&) {
    if (dead()) return;
    health = std::min(health, SPIKE_LEFT);
    hurtTime = HURT_TIME;
    note("STEPPED ON SPIKES");
    scare(DREAD_TRAP_HIT);
    sayLater(talk::Moment::Spiked);
    BodyPart part = rng_.chance(0.5f) ? BodyPart::Feet : BodyPart::Legs;
    float damage = rng_.range(SPIKE_WOUND_MIN, SPIKE_WOUND_MAX);
    wound(WoundKind::Pierce, part, damage).bleed *= SPIKE_BLEED_MUL;
    gore_.sprays.push_back({0.0f, 0.0f, damage, part});
    rethink_ = true;
}

void Human::learnTrap(Vec2i cell, TrapKind kind, bool mine) {
    bool fresh = memory.traps.emplace(cell, kind).second;
    if (fresh) rethink_ = true;
    rememberAt(cell, Episode::What::Trap,
               !mine ? EPISODE_SAW_TRAP : kind == TrapKind::Spikes ? EPISODE_SPIKES : EPISODE_OWN_TRAP, mine ? id : -1,
               -1, int(kind));
    // Spikes underfoot are in the log as they go in.
    if (!mine)
        note(std::string("SAW ") + trapWord(kind) + " GO OFF");
    else if (kind == TrapKind::Darts)
        note("SET OFF DARTS");
    else if (kind == TrapKind::Slam) {
        note("SHUT IN A SLAM ROOM");
        sayLater(talk::Moment::Slammed);
    }
    // Seen going off, told of or not: a scare.
    scare(DREAD_TRAP_SEEN);
    // A trap it was told of, seen: true. A trap where it was told the way
    // was clear: whoever told it lied.
    if (auto it = memory.toldBy.find(cell); it != memory.toldBy.end()) {
        if (fresh) {
            note("@ LIED ABOUT THE WAY", it->second);
            remember(Episode::What::Lied, EPISODE_LIED, it->second, id);
            regard(it->second, TRUST_LIED);
        }
        memory.toldBy.erase(it);
    }
}

// Hit by a starving human it would feed anyway, it feeds them. Otherwise,
// not hungry or too hurt, it runs; hungry, or with nowhere to run, it
// fights back.
void Human::react(World& world) {
    fight_.provoked = false;
    awaiting_ = false;
    moving = false;
    observe(world);
    Human& a = world.human(fight_.attacker);
    if (!a.dead() && wouldShare(a)) {
        if (fight_.foe == a.id) fight_.foe = -1;
        social_.sharee = a.id;
        if (startShare(world)) return;
    }
    if (!a.dead()) {
        // The brave stand their ground, unless too hurt to.
        if (weak(*this) || (!hungry() && !rng_.chance(traits.brave))) {
            fight_.feared = a.id;
            fight_.fear = fearTime();
            scare(DREAD_HIT);
            if (fight_.foe == a.id) fight_.foe = -1;
            mind_.prey = -1;
            if (startFlee(world)) {
                note("RAN FROM @", a.id);
                sayLater(talk::Moment::Flee, a.id);
                return;
            }
        }
        fight_.foe = a.id;
        fight_.grudge = grudgeFor(a.id);
    }
    choose(world);
}

bool Human::startFlee(World& world) {
    Vec2i from = world.human(fight_.feared).cell;
    auto p = search(world, [&](Vec2i c) { return cheb(c, from) >= FLEE_DIST; }, true);
    if (!p || p->empty()) return false;
    startWalking(Goal::Flee, *p);
    return true;
}

bool Human::worthLooting(int id, bool cakesOnly) const {
    auto it = deaths_.bodies.find(id);
    if (it == deaths_.bodies.end()) return false;
    bool forCakes = it->second.cakes > 0 && cakes < carryMax();
    return forCakes || (!cakesOnly && !it->second.searched);
}

// Bodies near someone this human ran from are left alone while it is
// still afraid, as cakes are.
std::optional<std::vector<Vec2i>> Human::lootPath(const World& world, bool cakesOnly, int* whose) const {
    auto it = social_.lastSeen.find(fight_.feared);
    bool afraid = fight_.fear > 0.0f && it != social_.lastSeen.end();
    std::vector<std::pair<int, Vec2i>> worth;
    for (const auto& [who, b] : deaths_.bodies)
        if (worthLooting(who, cakesOnly) && !(afraid && cheb(b.cell, it->second) < FLEE_DIST)) worth.push_back({who, b.cell});
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

bool Human::startLoot(World& world, bool cakesOnly) {
    sleep_.robbee = -1;
    int whose = -1;
    auto p = lootPath(world, cakesOnly, &whose);
    if (!p) {
        deaths_.corpse = -1;
        return false;
    }
    deaths_.corpse = whose;
    if (!p->empty()) {
        startWalking(Goal::Loot, *p);
        return true;
    }
    // Beside it: kneels and goes through it.
    const Human& b = world.human(deaths_.corpse);
    state = State::Looting;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    if (b.cell.x != cell.x) facingLeft = b.cell.x < cell.x;
    timer = LOOT_TIME;
    return true;
}

// Everything it holds of keys, and the cakes there is room for.
void Human::loot(World& world) {
    if (sleep_.robbee >= 0) return rob(world);
    Human& b = world.human(deaths_.corpse);
    deaths_.corpse = -1;
    int k = b.keys, c = std::clamp(carryMax() - cakes, 0, b.cakes);
    b.keys = 0;
    b.cakes -= c;
    keys += k;
    cakes += c;
    if (k > 0 && social_.leader >= 0) {
        sayLater(talk::Moment::PartOwnKey, social_.leader);
        social_.leader = -1;
    }
    auto count = [](int n, const char* one, const char* many) {
        return n == 1 ? std::string(one) : std::to_string(n) + many;
    };
    std::string took = k > 0 && c > 0 ? count(k, "A KEY", " KEYS") + " AND " + count(c, "A CAKE", " CAKES")
                       : k > 0        ? count(k, "A KEY", " KEYS")
                                      : count(c, "A CAKE", " CAKES");
    note(k + c > 0 ? "TOOK " + took + " OFF @'S BODY" : "WENT THROUGH @'S BODY, FOUND NOTHING", b.id);
    remember(Episode::What::Loot, EPISODE_LOOTED, id, b.id);
    // Gone through in plain sight: picked over, and a poor thing to do.
    for (const auto& o : world.humans())
        if (!o->dead() && (o.get() == this || o->sight.sees(b.cell))) o->deaths_.bodies[b.id] = {b.cell, b.cakes, true, b.meals(), b.spoiled()};
    witness(world, *this, b, TRUST_SAW_LOOT, 0.0f, Episode::What::Loot, EPISODE_SAW_LOOT);
    // Said aloud, and the nearest who saw it may say what they think of it.
    sayLater(talk::Moment::Loot, -1, b.id);
    Human* critic = nullptr;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && o->sight.sees(cell) && cheb(o->cell, cell) <= HEAR_RADIUS &&
            !o->isFoe(b.id) && (!critic || cheb(o->cell, cell) < cheb(critic->cell, cell)))
            critic = o.get();
    if (critic) critic->sayLater(talk::Moment::Shame, id, b.id);
    startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    if (c > 0 && hungry()) timer = 0.0f;
}
