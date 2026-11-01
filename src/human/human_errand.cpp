#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>

// Looking after its needs with some foresight (AI/needs_planning_plan.md).
// Out walking for water or a cake, it keeps to the errand: only danger and
// someone new to greet turn it aside, and afterwards it goes back to it.
// Having learnt to keep near water, it drinks at water it passes before it
// is thirsty, and turns back before it has gone too far from the water it
// last drank at; the bold leave it later, the lesson sooner. Just drunk, a
// cake close by is fetched while it is there; just taken a cake, so is a
// drink, by those who top up.

namespace {

/// Turned aside from an errand more than this often, it stops keeping to
/// it: the way there leads past whatever turns it aside.
constexpr int ERRAND_ASIDES = 2;
/// Given up so, an errand it does without pressing need waits this long.
constexpr float ERRAND_REST = 30.0f;
/// Having learnt to keep near water as well as it can, it tops up from this
/// much below THIRSTY, by the lesson; the bold (brave and curious) up to
/// TOP_UP_BOLD of that later, the timid sooner; never below TOP_UP_MIN.
constexpr float TOP_UP_EARLY = 0.35f;
constexpr float TOP_UP_BOLD = 0.5f;
constexpr float TOP_UP_MIN = 0.15f;
/// Water this many cells off or nearer is walked to for topping up.
constexpr int TOP_UP_REACH = 6;
/// Thirst one step of walking costs.
constexpr float STEP_THIRST = THIRST_PER_SECOND[size_t(Human::Action::Walk)] * STEP_TIME;
/// A walk back is this many times the cells as the crow flies, for walls.
constexpr float RANGE_WIND = 1.5f;
/// It turns back once the walk back would take this share of the thirst
/// it has left before parched (bottles counted); the bold up to RANGE_BOLD
/// of it more, the timid less; a full lesson up to RANGE_LESSON of it less.
constexpr float RANGE_REACH = 0.6f;
constexpr float RANGE_BOLD = 0.4f;
constexpr float RANGE_LESSON = 0.4f;
/// Within this many seconds of a drink or a cake, the other within
/// CHAIN_REACH cells is gone for while there: a cake if it has none, or
/// room for one and is CHAIN_HUNGER hungry.
constexpr float CHAIN_TIME = 30.0f;
constexpr int CHAIN_REACH = 15;
constexpr float CHAIN_HUNGER = 0.25f;

} // namespace

bool Human::errandWant(Want w) { return w == Want::Food || w == Want::Drink || w == Want::Fetch || w == Want::Fill; }

bool Human::errandGoal(Goal g) {
    return g == Goal::Drink || g == Goal::FetchCake || g == Goal::FetchKey || g == Goal::Pick || g == Goal::Fill ||
           g == Goal::Vend;
}

void Human::onErrand(Want w) {
    if (!errandWant(w)) return;
    // Went straight to it where it stands, or set off some other way (a
    // fight for a cake): nothing to keep to.
    if (state != State::Walking || !errandGoal(goal)) {
        errand_.want = Want::Count;
        return;
    }
    if (errand_.want != w) {
        errand_.want = w;
        errand_.since = clock_;
        errand_.aside = errand_.backNoted = false;
        errand_.asides = 0;
        return;
    }
    if (!errand_.aside) return;
    errand_.aside = false;
    if (++errand_.asides > ERRAND_ASIDES) {
        errand_.restWant = w;
        errand_.restUntil = clock_ + ERRAND_REST;
        errand_.want = Want::Count;
        return;
    }
    if (errand_.backNoted) return;
    errand_.backNoted = true;
    note(w == Want::Drink ? "WENT BACK FOR THE WATER" : w == Want::Fill ? "WENT BACK TO FILL MY BOTTLE"
                                                                         : "WENT BACK FOR THE CAKE");
    sayLater(talk::Moment::ErrandBack);
}

// Its needs, and someone new to greet, always count; exploring for one of
// them too. Wanting out, company, doors, keys and the rest wait.
bool Human::turnsAsideFor(Want w, bool seeking) const {
    switch (w) {
    case Want::Food:
    case Want::Drink:
    case Want::Relieve:
    case Want::Sleep:
    case Want::Flesh:
    case Want::Hunt:
    case Want::Track:
    case Want::Meet:
        return true;
    case Want::Explore:
        return seeking || w == errand_.want;
    default:
        return w == errand_.want;
    }
}

void Human::gotFor(Want w) {
    errand_.got = w;
    errand_.gotAt = clock_;
    errand_.lookedNear = false;
    if (w == Want::Drink) errand_.turnBackNoted = false;
}

float Human::topUpLine() const {
    if (lessons.nearWater <= 0.0f) return 2.0f;
    float bold = (traits.brave + traits.curious) * 0.5f;
    return std::max(TOP_UP_MIN, THIRSTY - TOP_UP_EARLY * lessons.nearWater * sway(1.0f - bold, TOP_UP_BOLD));
}

bool Human::topUp(World& world) {
    if (thirsty() || thirst < topUpLine()) return false;
    auto drinkable = [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && wouldDrink(world, c, *w);
    };
    std::optional<std::vector<Vec2i>> p;
    if (!drinkable(cell)) {
        p = searchNear(world, drinkable, TOP_UP_REACH);
        if (!p || p->empty()) return false;
    }
    note("DRANK WHILE I COULD");
    sayLater(talk::Moment::TopUp);
    if (!p) {
        startDrinking(*waterBeside(cell));
        return true;
    }
    startWalking(Goal::Drink, *p);
    onErrand(Want::Drink);
    return true;
}

float Human::rangeUsed() const {
    if (lessons.nearWater <= 0.0f || !water_.drankYet) return 0.0f;
    float left = (PARCHED - thirst + bottleWater()) / STEP_THIRST;
    if (left <= 0.0f) return 10.0f;
    float bold = (traits.brave + traits.curious) * 0.5f;
    float reach = RANGE_REACH * sway(bold, RANGE_BOLD) * (1.0f - RANGE_LESSON * lessons.nearWater);
    return float(cheb(cell, water_.last)) * RANGE_WIND / (left * reach);
}

bool Human::turnBack(World& world) {
    auto p = search(world, [&](Vec2i c) {
        auto w = waterBeside(c);
        return w && wouldDrink(world, c, *w);
    });
    if (!p) return false;
    if (p->empty()) {
        startDrinking(*waterBeside(cell));
        return true;
    }
    if (!errand_.turnBackNoted) {
        errand_.turnBackNoted = true;
        note("TOO FAR FROM WATER. TURNING BACK");
        sayLater(talk::Moment::TurnBack);
    }
    startWalking(Goal::Drink, *p);
    return true;
}

void Human::noteSeeking() {
    if (thirsty() && !errand_.seekWaterNoted) {
        errand_.seekWaterNoted = true;
        note("LOOKING FOR WATER");
        sayLater(talk::Moment::SeekWater);
    } else if (hungry() && !thirsty() && !errand_.seekFoodNoted) {
        errand_.seekFoodNoted = true;
        note("LOOKING FOR FOOD");
        sayLater(talk::Moment::SeekFood);
    }
}

bool Human::whileHere(World& world) {
    if (errand_.lookedNear) return false;
    errand_.lookedNear = true;
    if (clock_ - errand_.gotAt > CHAIN_TIME) return false;
    bool room = cakes == 0 || (cakes < carryMax() && hunger >= CHAIN_HUNGER);
    if (errand_.got == Want::Drink && room && !memory.cakes.empty()) {
        auto p = searchNear(world, [&](Vec2i c) { return cakeInReach(c).has_value(); }, CHAIN_REACH);
        if (!p || p->empty()) return false;
        note("FETCHING A CAKE WHILE I'M HERE");
        sayLater(talk::Moment::WhileHere);
        startWalking(Goal::FetchCake, *p);
        onErrand(Want::Fetch);
        return true;
    }
    if (errand_.got == Want::Fetch && !thirsty() && thirst >= topUpLine()) {
        auto p = searchNear(world, [&](Vec2i c) {
            auto w = waterBeside(c);
            return w && wouldDrink(world, c, *w);
        }, CHAIN_REACH);
        if (!p || p->empty()) return false;
        note("DRINKING WHILE I'M HERE");
        sayLater(talk::Moment::WhileHere);
        startWalking(Goal::Drink, *p);
        onErrand(Want::Drink);
        return true;
    }
    return false;
}

std::optional<std::vector<Vec2i>> Human::searchNear(const World& world, const std::function<bool(Vec2i)>& done,
                                                    int reach) const {
    int was = searchReach_;
    searchReach_ = reach;
    auto p = search(world, done);
    searchReach_ = was;
    return p;
}
