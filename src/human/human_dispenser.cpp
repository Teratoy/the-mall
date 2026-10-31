#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <iterator>

// Water dispensers: an upturned jug of clean water on a stand in every
// server room, three thirst bars of it at most, which never comes back on
// its own (user). The jug is see-through, so a human remembers how full it
// saw it, and tells of it as of any water. Cool and clean, it is drunk from
// before a basin or a pool not much nearer (user), and bottles are filled
// at it, draining it. Only pouring a bottle in fills it again: a settler
// keeps the one by its home full, and one who has learnt water is scarce
// and sleeps among the machines keeps its water there, behind the locked
// doors (user). Only water it would drink itself goes in, so a
// dispenser's water is always clean.

namespace {

/// Under this much it is dry: nothing comes from the tap.
constexpr float DRY = 0.01f;
/// A settler's dispenser is any within this many cells of its home.
constexpr int KEEP_REACH = 6;
/// A bottle with at least this much clean water in it is worth carrying
/// to its dispenser, when there is at least REFILL_ROOM room in it.
constexpr float REFILL_MIN = 0.5f;
constexpr float REFILL_ROOM = 0.5f;
/// Clean enough to pour in: no fouler or redder than this.
constexpr float POUR_CLEAN = 0.05f;
/// Carrying water to its dispenser is wanted this much over WANTING's Fill base.
constexpr float REFILL_DRIVE = 0.5f;
/// Pouring a bottle in takes this long, as pouring one out does.
constexpr float REFILL_TIME = 1.2f;

} // namespace

bool Human::seeDispenser(const Dispenser& d) {
    checkTold(d.at, true);
    bool wasDry = knownDry(d.at);
    rememberLevel(d.at, d.water);
    if (!dispense_.seen && state != State::Waking) {
        dispense_.seen = true;
        note("FOUND A WATER DISPENSER");
        if (!discover({talk::Moment::FirstDispenser, d.at})) sayLater(talk::Moment::FirstDispenser);
    }
    return wasDry && d.water > DRY && thirsty() && goal != Goal::Drink;
}

// Gone dry is news to it only if it came for that water, or keeps it.
void Human::rememberLevel(Vec2i at, float water) {
    bool had = !knownDry(at);
    memory.dispensers[at] = water;
    if (!had || water > DRY || state == State::Waking) return;
    if (!thirsty() && goal != Goal::Drink && goal != Goal::Fill && keptDispenser() != at) return;
    note("FOUND THE WATER DISPENSER DRY");
    sayLater(talk::Moment::DispenserDry);
}

bool Human::knownDry(Vec2i water) const {
    auto it = memory.dispensers.find(water);
    return it != memory.dispensers.end() && it->second <= DRY;
}

float Human::drawFrom(World& world, Vec2i at, float amount) {
    float got = world.drawWater(at, amount);
    if (const Dispenser* d = world.dispenser(at)) rememberLevel(at, d->water);
    return got;
}

bool Human::knowsCooler() const {
    return std::any_of(memory.dispensers.begin(), memory.dispensers.end(),
                       [](const auto& d) { return d.second > DRY; });
}

// The nearest to home, the one higher up and further left of two as near.
std::optional<Vec2i> Human::keptDispenser() const {
    if (!escape_.settled) return dispense_.kept;
    std::optional<Vec2i> best;
    for (const auto& [c, water] : memory.dispensers) {
        if (cheb(c, home()) > KEEP_REACH) continue;
        if (!best || cheb(c, home()) < cheb(*best, home()) ||
            (cheb(c, home()) == cheb(*best, home()) && (c.y != best->y ? c.y < best->y : c.x < best->x)))
            best = c;
    }
    return best;
}

// Only a room it knows, and a dispenser it knows in it.
void Human::keepWaterHere() {
    if (!water_.scarce || escape_.settled) return;
    for (const auto& [inner, r] : memory.serverRooms) {
        if (!r.contains(cell)) continue;
        for (const auto& [c, water] : memory.dispensers) {
            if (!r.contains(c) || dispense_.kept == c) continue;
            dispense_.kept = c;
            note("MEANS TO KEEP WATER HERE, BEHIND THE LOCKED DOORS");
        }
        return;
    }
}

int Human::pourable() const {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++) {
        const Bottle& b = bottles[i];
        if (b.water < REFILL_MIN || b.foul > POUR_CLEAN || b.red > POUR_CLEAN) continue;
        if (best < 0 || b.water > bottles[size_t(best)].water) best = int(i);
    }
    return best;
}

// Thirsty, it drinks first: the water is for later.
float Human::refillDrive() const {
    if (thirsty() || pourable() < 0) return -1.0f;
    auto at = keptDispenser();
    if (!at) return -1.0f;
    auto it = memory.dispensers.find(*at);
    float level = it == memory.dispensers.end() ? DISPENSER_HOLDS : it->second;
    return level <= DISPENSER_HOLDS - REFILL_ROOM ? REFILL_DRIVE : -1.0f;
}

bool Human::seekRefill(World& world) {
    if (refillDrive() < 0.0f) return false;
    Vec2i at = *keptDispenser();
    auto p = search(world, [&](Vec2i c) { return std::any_of(std::begin(DIRS4), std::end(DIRS4), [&](Vec2i d) { return c + d == at; }); },
                    true);
    if (!p) return false;
    if (p->empty()) return startRefill(at);
    startWalking(Goal::Fill, *p);
    return true;
}

bool Human::startRefill(Vec2i at) {
    int i = pourable();
    if (i < 0) return false;
    water_.drinkAt = at;
    water_.inHand = i;
    dispense_.refilling = true;
    goal = Goal::Fill;
    if (at.x != cell.x) facingLeft = at.x < cell.x;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    state = State::Pouring;
    timer = REFILL_TIME;
    return true;
}

void Human::endRefill(World& world) {
    dispense_.refilling = false;
    int i = water_.inHand;
    water_.inHand = -1;
    Vec2i at = water_.drinkAt;
    if (i >= 0 && i < int(bottles.size()) && world.dispenser(at)) {
        float in = world.pourIn(at, bottles[size_t(i)].water);
        bottles[size_t(i)].take(in);
        rememberLevel(at, world.dispenser(at)->water);
        if (in > 0.0f) {
            note("POURED MY BOTTLE INTO THE WATER DISPENSER");
            sayLater(talk::Moment::RefillDispenser);
        }
    }
    choose(world);
}
