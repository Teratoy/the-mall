#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cstdlib>

// Groups: a human that likes someone in sight may keep with them, walking
// after them, waiting near them and sitting when they sit. Anyone but a
// loner keeps with a friend; the sociable keep with acquaintances they
// think at all well of too. Nobody keeps with someone who (however many
// steps round) keeps with it, so each group has one who goes its own way,
// and the rest trail after; that one waits now and then for stragglers.

namespace {

/// Below this sociability it keeps with nobody but a friend, and a loner not
/// even with one. Otherwise it keeps with anyone met it trusts at least
/// TRUST_COMPANY, the most sociable at least TRUST_COMPANY_SOCIABLE (anyone
/// not disliked), and lets go once it trusts them TRUST_COMPANY_KEEP less.
constexpr float LONER_COMPANY = 0.3f;
constexpr float SOCIABLE_COMPANY = 0.4f;
constexpr float TRUST_COMPANY = 0.1f;
constexpr float TRUST_COMPANY_SOCIABLE = 0.0f;
constexpr float TRUST_COMPANY_KEEP = 0.1f;
/// Choosing whom to keep with: someone with keys counts this much more
/// liked, and each cell away this much less.
constexpr float COMPANY_KEYS = 0.1f;
constexpr float COMPANY_NEAR = 0.01f;
/// It keeps this close, waiting this long at a time once there; when they
/// sit, it sits too this often.
constexpr int COMPANY_GAP = 2;
constexpr float COMPANY_WAIT_MIN = 0.3f, COMPANY_WAIT_MAX = 0.8f;
constexpr float COMPANY_SIT = 0.7f;
/// Whoever goes its own way waits for anyone in sight keeping with it that
/// fell this far behind, a moment at a time, up to COMPANY_PATIENCE seconds
/// in a row.
constexpr int COMPANY_LAG = 6;
constexpr float COMPANY_PAUSE_MIN = 0.5f, COMPANY_PAUSE_MAX = 1.0f;
constexpr float COMPANY_PATIENCE = 8.0f;
/// Parched, keeping with one parched too who knows of no water: it stays if
/// sociability, and timidity times STAY_LOST_TIMID, and STAY_LOST_FRIEND for
/// a friend, come to STAY_LOST; else it goes to look alone.
constexpr float STAY_LOST = 0.8f, STAY_LOST_TIMID = 0.3f, STAY_LOST_FRIEND = 0.3f;

} // namespace

float Human::companyTrust() const {
    float s = std::clamp((traits.sociable - SOCIABLE_COMPANY) / (1.0f - SOCIABLE_COMPANY), 0.0f, 1.0f);
    return TRUST_COMPANY + (TRUST_COMPANY_SOCIABLE - TRUST_COMPANY) * s;
}

bool Human::followedBy(const World& world, int id) const {
    // A chain is never longer than there are humans.
    for (size_t n = 0; id >= 0 && n <= world.humans().size(); n++) {
        if (id == this->id) return true;
        id = world.human(id).follows();
    }
    return false;
}

int Human::pickCompany(const World& world) const {
    if (traits.sociable < LONER_COMPANY) return -1;
    int best = -1;
    float bestScore = 0.0f;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || !met.count(o->id) || !sight.sees(o->cell)) continue;
        // Nor someone too filthy for it to greet (human_relief.cpp).
        if (suspicious(o->id) || isFoe(o->id) || repelledBy(*o)) continue;
        float t = trust(o->id);
        if (!isFriend(o->id) && (traits.sociable < SOCIABLE_COMPANY || t < companyTrust())) continue;
        if (followedBy(world, o->id) || leavesLost(*o)) continue;
        float score = t + (o->keys > 0 ? COMPANY_KEYS : 0.0f) - COMPANY_NEAR * float(cheb(cell, o->cell));
        if (best < 0 || score > bestScore) {
            best = o->id;
            bestScore = score;
        }
    }
    return best;
}

bool Human::stillCompany(const World& world) const {
    if (social_.companion < 0) return false;
    const Human& c = world.human(social_.companion);
    if (sight.sees(c.cell) && c.dead()) return false;
    if (suspicious(c.id) || isFoe(c.id) || (!isFriend(c.id) && trust(c.id) < companyTrust() - TRUST_COMPANY_KEEP))
        return false;
    // Gone filthy: kept away from until washed.
    if (sight.sees(c.cell) && repelledBy(c)) return false;
    if (leavesLost(c)) return false;
    return !followedBy(world, c.id);
}

bool Human::leavesLost(const Human& o) const {
    if (!parched() || !o.parched() || !water_.knewNone.count(o.id)) return false;
    float stay = traits.sociable + STAY_LOST_TIMID * (1.0f - traits.brave) + (isFriend(o.id) ? STAY_LOST_FRIEND : 0.0f);
    return stay < STAY_LOST;
}

bool Human::keepCompany(World& world) {
    if (!stillCompany(world)) {
        if (social_.companion >= 0) partCompany(world);
        social_.companion = pickCompany(world);
        if (social_.companion < 0) return false;
        note("KEEPING WITH @", social_.companion);
        sayLater(talk::Moment::Join, social_.companion);
    }
    Human& c = world.human(social_.companion);
    bool seen = sight.sees(c.cell);
    Vec2i at = seen ? c.cell : social_.lastSeen[c.id];
    if (cheb(cell, at) <= COMPANY_GAP) {
        if (!seen) {
            note("LOST @", social_.companion);
            sayLater(talk::Moment::PartLost, -1, social_.companion);
            social_.companion = -1;
            return false;
        }
        // Beside one asleep, it sits down to wait.
        if (c.state == State::Sleeping ||
            ((c.state == State::Sitting || c.state == State::Waking) && rng_.chance(COMPANY_SIT))) {
            sitDown();
        } else {
            startIdle(COMPANY_WAIT_MIN, COMPANY_WAIT_MAX);
            goal = Goal::Company;
        }
        if (c.cell.x != cell.x) facingLeft = c.cell.x < cell.x;
        return true;
    }
    auto p = search(world, [&](Vec2i x) { return x != at && cheb(x, at) <= COMPANY_GAP; });
    if (!p) {
        social_.companion = -1;
        return false;
    }
    startWalking(Goal::Company, *p);
    return true;
}

// Says why it is leaving social_.companion, if it can tell them: nothing to the
// dead, nor to one who now follows it.
void Human::partCompany(const World& world) {
    const Human& c = world.human(social_.companion);
    if (c.dead() || followedBy(world, c.id)) return;
    if (suspicious(c.id) || isFoe(c.id) || (!isFriend(c.id) && trust(c.id) < companyTrust() - TRUST_COMPANY_KEEP)) {
        note("SPLIT FROM @", c.id);
        sayLater(talk::Moment::PartTurned, c.id);
    } else if (sight.sees(c.cell) && repelledBy(c)) {
        note("KEEPING AWAY FROM @ UNTIL THEY WASH", c.id);
        sayLater(talk::Moment::PartFilthy, c.id);
    } else if (leavesLost(c)) {
        note("LEFT @. THEY KNOW NO MORE THAN I DO", c.id);
        sayLater(talk::Moment::PartDry, c.id);
    }
}

bool Human::waitForCompany(const World& world) {
    if (follows() >= 0) return false;
    bool lagging = false;
    for (const auto& o : world.humans())
        if (o->social_.companion == id && o->social_.leader < 0 && !o->dead() && sight.sees(o->cell) &&
            cheb(cell, o->cell) > COMPANY_LAG)
            lagging = true;
    if (!lagging) {
        social_.waitedFor = 0.0f;
        return false;
    }
    if (social_.waitedFor >= COMPANY_PATIENCE) return false;
    float t = rng_.range(COMPANY_PAUSE_MIN, COMPANY_PAUSE_MAX);
    social_.waitedFor += t;
    startIdle(t, t);
    return true;
}
