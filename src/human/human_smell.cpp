#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <set>

namespace {

/// A human sniffs this often, in seconds.
constexpr float SNIFF_EVERY = 1.0f;
/// It forgets a smell it hasn't smelt for this long.
constexpr float SMELL_FORGET = 120.0f;
/// Dirty this long it smells (grime, of GRIME_MAX 3): about five minutes
/// after turning dirty (the user). From faint, it gets stronger up to GRIME_MAX.
constexpr float SMELL_GRIME = 1.5f, SMELL_GRIME_TOP = 3.0f, SMELL_HUMAN_MIN = 0.3f;
/// A body smells only once it has begun to rot (Human::spoiled), from
/// this strongly, more as it goes, bloody or not (the user).
constexpr float BODY_ROT_MIN = 0.5f;
/// Walking on or next to the strongest smell it knows costs this many
/// tenths of a cell more, at middling modesty.
constexpr float SMELL_STEP_COST = 60.0f;
/// It walks at most this far to eat away from a smell, and not again for
/// this many seconds after.
constexpr int PUT_OFF_RANGE = 8;
constexpr float PUT_OFF_AGAIN = 30.0f;
/// It tells the same one they smell again only after this many seconds.
constexpr float COMPLAIN_AGAIN = 180.0f;

} // namespace

float Human::smellStrength() const {
    if (!dead()) {
        if (!grime_.smelly) return 0.0f;
        float past = (grime_.level - SMELL_GRIME) / (SMELL_GRIME_TOP - SMELL_GRIME);
        return std::clamp(SMELL_HUMAN_MIN + (1.0f - SMELL_HUMAN_MIN) * past, SMELL_HUMAN_MIN, 1.0f);
    }
    if (bones() || !spoiled()) return 0.0f;
    return std::min(1.0f, BODY_ROT_MIN + (1.0f - BODY_ROT_MIN) * decay());
}

void Human::sniff(const World& world, float dt) {
    // Turning smelly, from going dirty too long.
    if (grime_.dirty && !grime_.smelly && grime_.level >= SMELL_GRIME) {
        grime_.smelly = true;
        note("STARTED TO SMELL");
        sayLater(talk::Moment::Stink);
        if (wantsScrub()) rethink_ = true;
    }
    if (!grime_.dirty) grime_.smelly = false;
    if ((nose_.sniffIn -= dt) > 0.0f) return;
    nose_.sniffIn += SNIFF_EVERY;
    if (state == State::Sleeping) return;
    std::unordered_set<Vec2i, Vec2iHash> smelt;
    for (const Smell& s : world.smells()) {
        if (s.who == id || cheb(s.cell, cell) > smellReach(s.strength)) continue;
        smelt.insert(s.cell);
        Memory::Whiff& w = memory.smells[s.cell];
        // The strongest of what is on the cell is what it smells there.
        if (w.at < clock_ || s.strength > w.strength) w = {s.kind, s.strength, s.who, clock_};
        // A body it has not seen, smelt: something has died near.
        if (s.kind == SmellKind::Body && !deaths_.bodies.count(s.who) && !sight.sees(s.cell) &&
            nose_.smeltDead.insert(s.who).second) {
            note("SMELT DEATH");
            sayLater(talk::Moment::SmellDeath);
        }
    }
    // Where it would smell it and doesn't, the smell has gone; and long
    // unsmelt, it forgets.
    std::erase_if(memory.smells, [&](const auto& kv) {
        const auto& [c, w] = kv;
        if (smelt.count(c)) return false;
        return clock_ - w.at > SMELL_FORGET || cheb(c, cell) <= smellReach(w.strength);
    });
    memory.rebuildReek();
}

int Human::smellCost(Vec2i c) const {
    if (isErratic() || memory.reek.empty()) return 0;
    auto it = memory.reek.find(c);
    return it == memory.reek.end() ? 0 : int(it->second * SMELL_STEP_COST * (0.5f + modesty()));
}

bool Human::stinksHere() const { return !isErratic() && memory.reek.count(cell); }

bool Human::leaveStench(World& world) {
    if (clock_ - nose_.putOffAt < PUT_OFF_AGAIN) return false;
    auto p = search(world, [&](Vec2i c) { return cheb(c, cell) <= PUT_OFF_RANGE && !memory.reek.count(c); });
    if (!p || p->empty()) return false;
    nose_.putOffAt = clock_;
    startWalking(Goal::Avoid, *p);
    note("MOVED AWAY FROM A SMELL TO EAT");
    sayLater(talk::Moment::PutOff);
    // Whoever smells, seeing it go, knows why.
    std::set<int> stinkers;
    for (const auto& [c, w] : memory.smells)
        if (w.kind == SmellKind::Human && cheb(c, cell) <= 1) stinkers.insert(w.who);
    for (int who : stinkers) {
        Human& o = world.human(who);
        if (!o.dead() && o.state != State::Sleeping && o.sight.sees(cell)) o.noticeShunned(*this);
    }
    return true;
}

// In order of who, so it is the same whichever way the smells are kept.
void Human::complainOfSmell(World& world) {
    if (dead() || state == State::Sleeping || isErratic() || fight_.fear > 0.0f || fight_.foe >= 0) return;
    std::set<int> smelt;
    for (const auto& [c, w] : memory.smells)
        if (w.kind == SmellKind::Human && w.at == clock_) smelt.insert(w.who);
    for (int who : smelt) {
        Human& o = world.human(who);
        if (o.dead() || o.state == State::Sleeping) continue;
        if (trust(o.id) < TRUST_CONFIDE || o.trust(id) < TRUST_CONFIDE) continue;
        auto it = nose_.complainedAt.find(o.id);
        if (it != nose_.complainedAt.end() && clock_ - it->second < COMPLAIN_AGAIN) continue;
        nose_.complainedAt[o.id] = clock_;
        note("TOLD @ THEY SMELL", o.id);
        sayLater(talk::Moment::SmellComplain, o.id);
        o.toldSmelly(*this);
    }
}

void Human::toldSmelly(const Human& o) {
    note("@ SAID I SMELL", o.id);
    sayLater(talk::Moment::SmellAnswer, o.id);
    if (isErratic()) return;
    grime_.shame = 1.0f;
    if (wantsScrub()) rethink_ = true;
}

void Human::noticeShunned(const Human& o) {
    if (!grime_.smelly || !washesForLooks()) return;
    note("@ KEPT AWAY FROM MY SMELL", o.id);
    sayLater(talk::Moment::Shunned);
    grime_.shame = 1.0f;
    if (wantsScrub()) rethink_ = true;
}

bool Human::smellsOfDeath(Vec2i c) const {
    auto it = memory.smells.find(c);
    return it != memory.smells.end() && it->second.kind == SmellKind::Body;
}

// The strongest, and of those the first by its cell, so it is the same
// whichever way the smells are kept.
bool Human::followStench(World& world) {
    const Memory::Whiff* best = nullptr;
    Vec2i at;
    for (const auto& [c, w] : memory.smells) {
        if (w.kind != SmellKind::Body || deaths_.bodies.count(w.who)) continue;
        bool first = !best || w.strength > best->strength ||
                     (w.strength == best->strength && (c.y != at.y ? c.y < at.y : c.x < at.x));
        if (first) best = &w, at = c;
    }
    if (!best) return false;
    auto p = search(world, [&](Vec2i c) { return cheb(c, at) <= 1; });
    if (!p || p->empty()) return false;
    if (nose_.followed.insert(best->who).second) {
        note("FOLLOWED THE SMELL OF DEATH");
        sayLater(talk::Moment::FollowStench);
    }
    startWalking(Goal::Explore, *p);
    return true;
}
