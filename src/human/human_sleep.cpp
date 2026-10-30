#include "human/human.h"
#include "human/human_look.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <vector>

// Sleep: everything a human does tires it, hard work and fights most,
// bleeding and fear on top, and only sleep rests it. A tired human walks
// to somewhere out of the way, a dead end or a corner, never a corridor
// it would block nor a doorway, away from those it fears or distrusts,
// bodies and blood, by friends unless a loner, best of all where it slept
// safely before or at home, and never where a stranger it doesn't trust
// would watch it, if timid or modest. There it lies down: nodding off
// seated, then curled up on its side. Getting drowsy, it already keeps
// near where it means to sleep. Asleep it sees and hears nothing; it wakes
// rested, at a blow, or once well rested enough to mind starving, thirst
// or nearly bursting. Worn right out, it drops asleep wherever it stands.

namespace {

/// Tiredness gained per second by each action, the same rule as hunger
/// and thirst: a new action needs its own cost here too. Sleeping rests
/// instead (SLEEP_RECOVERY).
constexpr float TIRED_PER_SECOND[] = {
    1.0f / 3600.0f, // Rest
    1.0f / 5400.0f, // Sit
    1.0f / 1800.0f, // Walk
    1.0f / 450.0f, // Run
    1.0f / 2700.0f, // Greet
    1.0f / 3600.0f, // Eat
    1.0f / 3600.0f, // Drink
    1.0f / 3600.0f, // Relieve
    1.0f / 300.0f, // Punch
    1.0f / 2700.0f, // Give
    1.0f / 1800.0f, // Wash
    1.0f / 1800.0f, // Loot
    1.0f / 2700.0f, // Feed
    0.0f, // Sleep
    1.0f / 1200.0f, // Watch: staying awake on purpose
    1.0f / 3600.0f, // Fill
    1.0f / 3600.0f, // Pour
    1.0f / 3600.0f, // Stash
};
static_assert(std::size(TIRED_PER_SECOND) == size_t(Human::Action::Count));
/// Bleeding tires it this much more per unit of blood lost a second, and
/// dread up to this much more a second at its worst.
constexpr float TIRED_BLEED = 0.3f;
constexpr float TIRED_DREAD = 1.0f / 1800.0f;
/// Tired from here on: it wants sleep. Exhausted from EXHAUSTED: slower.
constexpr float TIRED = 0.5f;
constexpr float EXHAUSTED = 0.8f;
/// Tiredness slept off a second; it wakes once down to RESTED.
constexpr float SLEEP_RECOVERY = 1.0f / 170.0f;
constexpr float RESTED = 0.05f;
/// Starving, parched or nearly bursting wakes it early, but only once
/// rested this far, so it doesn't drop off again at once; and it holds on
/// asleep until this full; full right up, it wakes whatever, so it never
/// has an accident asleep.
constexpr float WAKE_NEED_BELOW = 0.3f;
constexpr float WAKE_NEED_FULL = 0.9f;
/// Farthest it walks to sleep when just tired, and when exhausted.
constexpr int SLEEP_REACH = 12;
constexpr int SLEEP_REACH_EXHAUSTED = 3;
/// Each open cell round a spot costs this many cells more to sleep there.
constexpr int SLEEP_SHY = 3;
/// A walk costs this much a cell (see search()); so do the costs below.
constexpr float CELL_COST = 10.0f;
/// Drowsy from here on: it starts keeping near where it means to sleep.
constexpr float DROWSY = 0.35f;
/// Drowsy and more than SLEEP_TETHER cells from where it means to sleep,
/// it wants to explore and open doors up to TETHER_SLEEP less, and wants
/// up to HEAD_BACK to walk back there.
constexpr int SLEEP_TETHER = 12;
constexpr float TETHER_SLEEP = 0.6f;
constexpr float HEAD_BACK = 1.5f;
/// Within BASE_NEAR of where it means to sleep counts as there; it walks
/// up to SLEEP_BASE_REACH to get there unless exhausted, and anywhere
/// else costs SLEEP_UNFAMILIAR cells more.
constexpr int BASE_NEAR = 2;
constexpr int SLEEP_BASE_REACH = 30;
constexpr int SLEEP_UNFAMILIAR = 25;
/// Each cell nearer than THREAT_REACH to where it last saw someone it
/// fears or distrusts costs THREAT_COST cells more.
constexpr int THREAT_REACH = 10;
constexpr float THREAT_COST = 3.0f;
/// A body it knows within BODY_REACH costs up to BODY_COST more (the
/// brave mind less); blood it knows within BLOOD_REACH, BLOOD_COST a
/// cell's worth fresh.
constexpr int BODY_REACH = 4;
constexpr float BODY_COST = 15.0f;
constexpr int BLOOD_REACH = 2;
constexpr float BLOOD_COST = 20.0f;
/// Friends and those it walks with, in sight: sleeping beyond
/// COMPANY_NEAR of all of them costs up to SLEEP_COMPANY more, by how
/// sociable it is. Loners (under SLEEP_LONER) pay LONER_CROWD to sleep
/// within COMPANY_NEAR of anyone instead.
constexpr int COMPANY_NEAR = 3;
constexpr float SLEEP_COMPANY = 10.0f;
constexpr float SLEEP_LONER = 0.3f;
constexpr float LONER_CROWD = 8.0f;
/// The timid (under SLEEP_BOLD) and modest (from SLEEP_MODEST) won't sleep
/// where someone trusted less than SLEEP_TRUST, not a friend nor one it
/// walks with, would see; unless erratic, or exhausted.
constexpr float SLEEP_BOLD = 0.4f;
constexpr float SLEEP_MODEST = 0.55f;
constexpr float SLEEP_TRUST = 0.1f;
/// Whom it blames for a cake gone while it slept: someone within
/// BLAME_REACH, thought this much the worse of.
constexpr int BLAME_REACH = 6;
constexpr float TRUST_BLAMED = -0.3f;
/// Sleeping no lighter than HEAVY_SLEEP it sleeps through anything but a
/// blow. Lighter, a fight, a cry of pain or a death within NOISE_REACH
/// (a death for DEATH_NOISE seconds) wakes it at up to NOISE_WAKE a
/// second, someone walking right past at STEP_WAKE, both by how much
/// lighter; and it wakes a little frightened.
constexpr float HEAVY_SLEEP = 0.3f;
constexpr int NOISE_REACH = 5;
constexpr float DEATH_NOISE = 1.0f;
constexpr float NOISE_WAKE = 1.5f;
constexpr float STEP_WAKE = 0.3f;
constexpr float NOISE_DREAD = 0.05f;
/// Robbing a sleeper: only the greedy from ROB_GREEDY and bold from
/// ROB_BRAVE, of someone it trusts under ROB_TRUST, not a friend nor one
/// it walks with. It kneels ROB_TIME, and they wake with chance ROB_WAKE ×
/// (0.5 + how lightly they sleep). Woken, they think TRUST_ROBBED the worse
/// of it; anyone who sees it, TRUST_SAW_ROB. After, it looks round a moment.
constexpr float ROB_GREEDY = 0.5f;
constexpr float ROB_BRAVE = 0.35f;
constexpr float ROB_TRUST = 0.2f;
constexpr float ROB_TIME = 1.5f;
constexpr float ROB_WAKE = 0.3f;
constexpr float TRUST_ROBBED = -0.5f;
constexpr float TRUST_SAW_ROB = -0.3f;
constexpr float ROB_LOOK_MIN = 0.5f, ROB_LOOK_MAX = 1.2f;
/// Drowsy, it lies down by a friend or one of its group asleep within
/// GROUP_SLEEP_NEAR.
constexpr int GROUP_SLEEP_NEAR = 6;
/// Keeping watch: over one of its group or a friend asleep within
/// WATCH_REACH, sitting within WATCH_GAP of them. It takes the watch of
/// itself only if no loner (WATCH_LONER), rested (under WATCH_FRESH), and
/// sociable and brave enough between them (WATCH_WILLING). A foe, suspect
/// or robber within WATCH_ALARM of the sleeper and it wakes them all.
constexpr int WATCH_REACH = 8;
constexpr int WATCH_GAP = 2;
constexpr float WATCH_LONER = 0.3f;
constexpr float WATCH_FRESH = 0.4f;
constexpr float WATCH_WILLING = 0.45f;
constexpr int WATCH_ALARM = 6;
/// Tired on watch, it wakes the most rested sleeper under HANDOVER_FRESH
/// to take a turn.
constexpr float HANDOVER_FRESH = 0.25f;
/// Lying down sleeping this light or lighter, it asks someone within
/// ASK_REACH to watch: a friend, or the sociable (ASK_SOCIABLE), agree;
/// the greedy (ASK_GREEDY) for a cake.
constexpr float ASK_LIGHT = 0.45f;
constexpr int ASK_REACH = 6;
constexpr float ASK_SOCIABLE = 0.5f;
constexpr float ASK_GREEDY = 0.6f;
/// On watch over someone it isn't fond of (trust under BETRAY_TRUST), the
/// greedy (BETRAY_GREEDY) and bold may rob them instead, at BETRAY_CHANCE
/// each time it looks again. Caught, the sleeper thinks TRUST_BETRAYED the
/// worse of it and leaves its company; harmed while its watcher slept or
/// was away, TRUST_FAILED_WATCH.
constexpr float BETRAY_TRUST = 0.3f;
constexpr float BETRAY_GREEDY = 0.67f;
constexpr float BETRAY_CHANCE = 0.15f;
constexpr float TRUST_BETRAYED = -0.8f;
constexpr float TRUST_FAILED_WATCH = -0.3f;
/// Slept right through: hope lifts HOPE_SLEPT and dread falls SLEEP_CALM.
/// Woken robbed or hit, hope falls HOPE_WOKEN_HARM; a cake found gone,
/// HOPE_ROBBED.
constexpr float HOPE_SLEPT = 0.05f;
constexpr float SLEEP_CALM = 0.1f;
constexpr float HOPE_WOKEN_HARM = -0.2f;
constexpr float HOPE_ROBBED = -0.1f;
/// Waking in its own time, it dreamt with chance DREAM_CHANCE: its hunch
/// of the way out turns DREAM_PULL of the way toward wherever the dream
/// seemed to point (anywhere), and it gets DREAM_SURE surer of it. The
/// dream stays on its mind DREAM_FRESH seconds.
constexpr float DREAM_CHANCE = 0.3f;
constexpr float DREAM_PULL = 0.35f;
constexpr float DREAM_SURE = 0.05f;
constexpr float DREAM_FRESH = 600.0f;
constexpr float TAU = 6.2831853f;
/// Woken, it is this dazed for a moment: a little, as the first waking was.
constexpr float SLEEP_DAZE = 0.25f;

/// The eight cells round a spot, in order round it.
constexpr Vec2i RING[8] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};

} // namespace

bool Human::tired() const { return tiredness >= TIRED; }

bool Human::exhausted() const { return tiredness >= EXHAUSTED; }

std::optional<Vec2i> Human::sleepBase() const {
    if (escape_.settled) return escape_.home;
    if (sleep_.sleptYet) return sleep_.sleptAt;
    return std::nullopt;
}

float Human::drowsy() const {
    return std::clamp((tiredness - DROWSY) / (TIRED - DROWSY), 0.0f, 1.0f) * (0.6f + 0.4f * lessons.safeSleep);
}

float Human::sleepTether() const {
    auto base = sleepBase();
    if (!base || cheb(cell, *base) <= SLEEP_TETHER) return 0.0f;
    return TETHER_SLEEP * drowsy();
}

float Human::headBack() const { return sleepTether() > 0.0f ? HEAD_BACK * drowsy() : 0.0f; }

// Drowsy, back to within BASE_NEAR of where it means to sleep, to potter
// about there till tired.
bool Human::returnToSleepPlace(World& world) {
    auto base = sleepBase();
    if (!base) return false;
    auto p = search(world, [&](Vec2i c) { return cheb(c, *base) <= BASE_NEAR; });
    if (!p || p->empty()) return false;
    startWalking(Goal::Wander, *p);
    return true;
}

bool Human::sleepWatched(const World& world, Vec2i c) const {
    if (isErratic() || exhausted() || (traits.brave >= SLEEP_BOLD && modesty() < SLEEP_MODEST)) return false;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && sight.sees(o->cell) && o->sight.sees(c) && !isFriend(o->id) &&
            !inGroupWith(*o) && trust(o->id) < SLEEP_TRUST)
            return true;
    return false;
}

float Human::tiredDrive() const { return std::max(0.0f, (tiredness - TIRED) / (1.0f - TIRED)); }

void Human::tire(float dt) {
    if (state == State::Sleeping) {
        tiredness = std::max(0.0f, tiredness - SLEEP_RECOVERY * dt);
        return;
    }
    bool wasTired = tired(), wasExhausted = exhausted();
    float rate = TIRED_PER_SECOND[size_t(action())] + TIRED_BLEED * bleeding() + TIRED_DREAD * dread_;
    tiredness = std::min(1.0f, tiredness + rate * dt);
    if ((tired() && !wasTired) || tiredness >= 1.0f) rethink_ = true;
    if (tired() && !wasTired) sayLater(talk::Moment::Tired);
    if (exhausted() && !wasExhausted) {
        note("EXHAUSTED");
        sayLater(talk::Moment::Exhausted);
    }
}

// Out of the way: the fewer open cells round it, the better, as far as it
// is worth walking; then safety and company (see the top). Never a cell
// others need to get by, whose open neighbours fall into more than one
// bunch round it (a corridor, a gap), nor one beside a door or water.
bool Human::seekSleep(World& world) {
    float worn = std::clamp((tiredness - TIRED) / (EXHAUSTED - TIRED), 0.0f, 1.0f);
    int range = int(std::lround(SLEEP_REACH - (SLEEP_REACH - SLEEP_REACH_EXHAUSTED) * worn));
    auto base = sleepBase();
    int baseRange = exhausted() ? range : SLEEP_BASE_REACH;
    auto atBase = [&](Vec2i c) { return base && cheb(c, *base) <= BASE_NEAR; };
    // Unknown ground may as well be open.
    auto open = [&](Vec2i c) {
        Tile t = memory.tile(c);
        return t == Tile::Void || passable(t);
    };
    auto done = [&](Vec2i c) {
        if (cheb(c, cell) > (atBase(c) ? baseRange : range) || memory.tile(c) != Tile::Floor || avoids(c)) return false;
        if (Human* o = world.occupant(c); o && o != this) return false;
        int bunches = 0;
        for (int i = 0; i < 8; i++) {
            Tile t = memory.tile(c + RING[i]);
            if (t == Tile::DoorOpen || t == Tile::DoorLocked || drinkable(t)) return false;
            bunches += open(c + RING[i]) && !open(c + RING[(i + 1) % 8]);
        }
        return bunches <= 1 && !sleepWatched(world, c);
    };
    // Where it last saw those it fears or distrusts, and who is in sight,
    // friend or not.
    std::vector<Vec2i> threats, company, anyone;
    for (const auto& [who, at] : social_.lastSeen)
        if (!world.human(who).dead() && (isFoe(who) || suspicious(who) || afraidOf(who))) threats.push_back(at);
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || !sight.sees(o->cell)) continue;
        anyone.push_back(o->cell);
        if (isFriend(o->id) || inGroupWith(*o)) company.push_back(o->cell);
    }
    auto near = [](const std::vector<Vec2i>& at, Vec2i c) {
        return std::any_of(at.begin(), at.end(), [&](Vec2i a) { return cheb(a, c) <= COMPANY_NEAR; });
    };
    bool loner = traits.sociable < SLEEP_LONER;
    auto cost = [&](Vec2i c) {
        int n = 0;
        for (Vec2i d : DIRS8) n += open(c + d);
        float k = 1.0f + float(n * SLEEP_SHY);
        for (Vec2i t : threats) k += float(std::max(0, THREAT_REACH - cheb(c, t))) * THREAT_COST;
        for (const auto& [who, b] : deaths_.bodies)
            if (cheb(c, b.cell) <= BODY_REACH) k += BODY_COST * (1.0f - traits.brave);
        for (int y = -BLOOD_REACH; y <= BLOOD_REACH; y++)
            for (int x = -BLOOD_REACH; x <= BLOOD_REACH; x++)
                if (auto it = memory.blood.find(c + Vec2i{x, y}); it != memory.blood.end())
                    k += it->second.total() * BLOOD_COST * (1.0f + 2.0f * it->second.wet) / 3.0f;
        if (loner && near(anyone, c)) k += LONER_CROWD;
        if (!loner && !company.empty() && !near(company, c)) k += SLEEP_COMPANY * traits.sociable;
        if (base && !atBase(c)) k += SLEEP_UNFAMILIAR;
        return int(k * CELL_COST);
    };
    auto p = search(world, done, true, cost);
    if (!p) return false;
    if (p->empty()) {
        startSleeping(world, false);
        return true;
    }
    startWalking(Goal::Sleep, *p);
    return true;
}

// Dropping where it stands, it goes straight down curled, with no nodding
// off first.
// Caught out worn through, it learns to find somewhere safe in time.
void Human::startSleeping(World& world, bool collapsed) {
    note(collapsed ? "COLLAPSED FROM EXHAUSTION" : "FELL ASLEEP");
    if (collapsed) learn(lessons.safeSleep, true);
    // Worn out on watch, it leaves those it watched unguarded.
    if (sleep_.watchee >= 0) note("FELL ASLEEP ON WATCH");
    sleep_.watchee = -1;
    sleep_.askedWatch = -1;
    sleep_.watchedBy = -1;
    if (!collapsed && !askToWatch(world)) sayLater(talk::Moment::SleepNow);
    auto base = sleepBase();
    sleep_.atSleepSpot = !collapsed && base && cheb(cell, *base) <= BASE_NEAR;
    // What it holds, and who is about, as it shuts its eyes.
    sleep_.cakesAsleep = cakes;
    sleep_.nearAsleep = -1;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && o->state != State::Sleeping && sight.sees(o->cell) &&
            cheb(o->cell, cell) <= BLAME_REACH &&
            (sleep_.nearAsleep < 0 || cheb(o->cell, cell) < cheb(world.human(sleep_.nearAsleep).cell, cell)))
            sleep_.nearAsleep = o->id;
    state = State::Sleeping;
    goal = Goal::Sleep;
    moving = false;
    awaiting_ = false;
    sleep_.roused = false;
    path_.clear();
    timer = 0.0f;
    animTime = collapsed ? LIE_DOWN_TIME : 0.0f;
    sight.blind();
}

float Human::lightSleep() const { return std::clamp(0.2f + 0.6f * (1.0f - traits.brave) + 0.5f * dread_, 0.0f, 1.0f); }

// Not by sight, its eyes shut: by how close the noise is.
bool Human::hearsInSleep(const World& world, float dt) {
    float light = lightSleep() - HEAVY_SLEEP;
    if (light <= 0.0f) return false;
    bool noise = false, steps = false;
    for (const auto& o : world.humans()) {
        if (o.get() == this) continue;
        int d = cheb(o->cell, cell);
        if (o->dead())
            noise |= d <= NOISE_REACH && o->animTime < DEATH_NOISE;
        else
            noise |= d <= NOISE_REACH && (o->state == State::Punching || o->hurtTime > 0.0f);
        steps |= d <= 1 && o->moving && o.get()->sleep_.robbee < 0;
    }
    float rate = (noise ? NOISE_WAKE : 0.0f) + (steps ? STEP_WAKE : 0.0f);
    return rate > 0.0f && rng_.chance(rate * light * dt);
}

void Human::sleep(World& world, float dt) {
    if (hearsInSleep(world, dt)) {
        scare(NOISE_DREAD);
        return wakeUp(world, true, -1, "WOKE UP TO A NOISE");
    }
    bool woken = (tiredness < WAKE_NEED_BELOW &&
                  (starving() || parched() || std::max(bladder, bowels) >= WAKE_NEED_FULL)) ||
                 bursting();
    if (tiredness > RESTED && !woken) return;
    wakeUp(world, false);
}

// Woken in its own time, that was a good place to sleep, and getting
// there in time paid off; set on there, it was not, and it is forgotten.
void Human::wakeUp(World& world, bool abrupt, int by, const char* what) {
    note(what ? what : "WOKE UP", by);
    escape_.daze = std::max(escape_.daze, SLEEP_DAZE);
    if (abrupt) {
        if (by >= 0 && sleep_.sleptYet && cheb(sleep_.sleptAt, cell) <= BASE_NEAR) sleep_.sleptYet = false;
        startIdle(0.0f, 0.0f);
    } else {
        if (sleep_.atSleepSpot && lessons.safeSleep > 0.0f) learn(lessons.safeSleep, false);
        sleep_.safeSleeps = sleep_.sleptYet && cheb(sleep_.sleptAt, cell) <= BASE_NEAR ? sleep_.safeSleeps + 1 : 1;
        sleep_.sleptAt = cell;
        sleep_.sleptYet = true;
        // Slept right through: a little hope, a little calm.
        if (tiredness <= RESTED) {
            lift(HOPE_SLEPT);
            dread_ = std::max(0.0f, dread_ - SLEEP_CALM);
        }
        if (rng_.chance(DREAM_CHANCE))
            dream();
        else
            sayLater(talk::Moment::Woke);
        state = State::Rising;
        sleep_.roused = true;
        animTime = 0.0f;
        timer = ROUSE_TIME + STAND_UP_TIME;
    }
    observe(world);
    if (missedWhileAsleep()) blameWatcher(world);
}

// The nearest it sees on waking, if close; else whoever was nearest as it
// fell asleep. Friends are the last it would think of.
bool Human::missedWhileAsleep() {
    int lost = sleep_.cakesAsleep - cakes;
    sleep_.cakesAsleep = cakes;
    if (lost <= 0) return false;
    note("SOMEONE TOOK MY CAKE WHILE I SLEPT");
    lift(HOPE_ROBBED);
    int blamed = -1, best = 0;
    for (int who : inSight_) {
        auto it = social_.lastSeen.find(who);
        if (it == social_.lastSeen.end() || cheb(it->second, cell) > BLAME_REACH) continue;
        int d = cheb(it->second, cell) + (isFriend(who) ? BLAME_REACH : 0);
        if (blamed < 0 || d < best) blamed = who, best = d;
    }
    if (blamed < 0) blamed = sleep_.nearAsleep;
    sayLater(talk::Moment::CakeGone, -1, blamed);
    if (blamed < 0) return true;
    note("@ MUST HAVE TAKEN IT", blamed);
    regard(blamed, TRUST_BLAMED);
    return true;
}

// Not one somebody sits watch over close by.
bool Human::wouldRob(const World& world, const Human& o) const {
    if (&o == this || o.state != State::Sleeping || o.cakes <= 0 || !sight.sees(o.cell) || cakes >= carryMax() ||
        traits.greedy < ROB_GREEDY || traits.brave < ROB_BRAVE || isFriend(o.id) || inGroupWith(o) ||
        trust(o.id) >= ROB_TRUST || afraidOf(o.id) || dead())
        return false;
    if (o.sleep_.watchedBy >= 0) {
        const Human& w = world.human(o.sleep_.watchedBy);
        if (!w.dead() && w.state != State::Sleeping && w.sleep_.watchee == o.id && cheb(w.cell, o.cell) <= WATCH_GAP + 1)
            return false;
    }
    return true;
}

bool Human::startRob(World& world) {
    Human* best = nullptr;
    for (const auto& o : world.humans())
        if (wouldRob(world, *o) && (!best || cheb(o->cell, cell) < cheb(best->cell, cell))) best = o.get();
    sleep_.robbee = -1;
    return best && goRob(world, *best);
}

bool Human::goRob(World& world, Human& o) {
    sleep_.robbee = -1;
    if (cheb(o.cell, cell) <= 1) {
        sleep_.robbee = o.id;
        state = State::Looting;
        moving = false;
        path_.clear();
        animTime = 0.0f;
        if (o.cell.x != cell.x) facingLeft = o.cell.x < cell.x;
        timer = ROB_TIME;
        return true;
    }
    Vec2i at = o.cell;
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= 1; }, true);
    if (!p) return false;
    startWalking(Goal::Rob, *p);
    sleep_.robbee = o.id;
    return true;
}

// The lighter they sleep, the likelier they wake; woken, they know who,
// and answer it as a blow. Anyone who sees it thinks the worse of it.
void Human::rob(World& world) {
    Human& o = world.human(sleep_.robbee);
    sleep_.robbee = -1;
    bool betrayed = sleep_.betraying;
    sleep_.betraying = false;
    if (o.dead() || o.state != State::Sleeping || cheb(o.cell, cell) > 1 || o.cakes <= 0 || cakes >= carryMax())
        return startIdle(ROB_LOOK_MIN, ROB_LOOK_MAX);
    o.cakes--;
    cakes++;
    for (const auto& w : world.humans())
        if (w.get() != this && w.get() != &o && !w->dead() && w->sight.sees(cell) && !w->isFoe(o.id))
            w->regard(id, TRUST_SAW_ROB);
    // Robbed by the one meant to watch over it, it is done with them.
    bool woke = rng_.chance(ROB_WAKE * (0.5f + o.lightSleep()));
    if (woke) {
        note("@ WOKE UP AS I ROBBED THEM", o.id);
        o.sleep_.cakesAsleep = o.cakes;
        o.wakeUp(world, true, id, "WOKE UP TO @ ROBBING ME");
        o.note(betrayed ? "@ ROBBED ME ON THEIR WATCH" : "@ ROBBED ME IN MY SLEEP", id);
        o.sayLater(talk::Moment::RobbedSleep, id);
        o.lift(HOPE_WOKEN_HARM);
        o.regard(id, betrayed ? TRUST_BETRAYED : TRUST_ROBBED);
        if (betrayed) {
            if (o.social_.companion == id) o.social_.companion = -1;
            if (o.social_.leader == id) o.social_.leader = -1;
            sleep_.watchee = -1;
        }
        o.sleep_.watchedBy = -1;
        o.fight_.attacker = id;
        o.fight_.provoked = true;
    } else {
        note(betrayed ? "ROBBED @ ON MY WATCH" : "ROBBED @ IN THEIR SLEEP", o.id);
    }
    wrong(o, woke);
    startIdle(ROB_LOOK_MIN, ROB_LOOK_MAX);
}

bool Human::sleepsWithGroup(const World& world) const {
    if (tiredness < DROWSY || sleep_.watchee >= 0 || sleep_.askedWatch >= 0) return false;
    for (const auto& o : world.humans())
        if (o.get() != this && o->state == State::Sleeping && sight.sees(o->cell) &&
            cheb(o->cell, cell) <= GROUP_SLEEP_NEAR && (inGroupWith(*o) || isFriend(o->id)))
            return true;
    return false;
}

bool Human::wouldWatch(const World& world, const Human& o) const {
    if (&o == this || o.state != State::Sleeping || !sight.sees(o.cell) || cheb(o.cell, cell) > WATCH_REACH) return false;
    if (!inGroupWith(o) && !isFriend(o.id) && sleep_.askedWatch != o.id) return false;
    for (const auto& h : world.humans())
        if (h.get() != this && !h->dead() && h->state != State::Sleeping && h->sleep_.watchee == o.id) return false;
    return true;
}

const Human* Human::watchThreat(const World& world, const Human& sleeper) const {
    for (const auto& o : world.humans())
        if (o.get() != this && o.get() != &sleeper && !o->dead() && o->state != State::Sleeping &&
            sight.sees(o->cell) && !inGroupWith(*o) && cheb(o->cell, sleeper.cell) <= WATCH_ALARM &&
            (isFoe(o->id) || suspicious(o->id) || afraidOf(o->id) || o->sleep_.robbee == sleeper.id))
            return o.get();
    return nullptr;
}

bool Human::watchCalls(const World& world) const {
    if (sleep_.watchee < 0 || hungry() || thirsty() || needsToGo() || tired()) return true;
    const Human& w = world.human(sleep_.watchee);
    return w.state != State::Sleeping || watchThreat(world, w);
}

bool Human::keepWatch(World& world) {
    if (hungry() || thirsty() || needsToGo() || fight_.fear > 0.0f) {
        sleep_.watchee = sleep_.askedWatch = -1;
        return false;
    }
    if (sleep_.watchee >= 0) {
        const Human& w = world.human(sleep_.watchee);
        if (w.state != State::Sleeping || !sight.sees(w.cell)) sleep_.watchee = -1;
    }
    if (sleep_.watchee < 0) {
        // Asked, it waits while they lie down.
        if (sleep_.askedWatch >= 0) {
            const Human& a = world.human(sleep_.askedWatch);
            if (!a.dead() && a.state != State::Sleeping && a.goal == Goal::Sleep && sight.sees(a.cell)) {
                startIdle(ROB_LOOK_MIN, ROB_LOOK_MAX);
                return true;
            }
        }
        bool willing = sleep_.askedWatch >= 0 || (traits.sociable >= WATCH_LONER && tiredness < WATCH_FRESH &&
                                            (traits.sociable + traits.brave) / 2.0f >= WATCH_WILLING);
        const Human* best = nullptr;
        if (willing) {
            if (sleep_.askedWatch >= 0 && wouldWatch(world, world.human(sleep_.askedWatch)))
                best = &world.human(sleep_.askedWatch);
            else
                for (const auto& o : world.humans())
                    if (wouldWatch(world, *o) && (!best || cheb(o->cell, cell) < cheb(best->cell, cell))) best = o.get();
        }
        sleep_.askedWatch = -1;
        if (!best) return false;
        sleep_.watchee = best->id;
        world.human(sleep_.watchee).sleep_.watchedBy = id;
        note("KEEPING WATCH OVER @", sleep_.watchee);
    }
    Human& w = world.human(sleep_.watchee);
    if (tired()) {
        handOverWatch(world);
        sleep_.watchee = -1;
        return false;
    }
    if (const Human* t = watchThreat(world, w)) {
        for (const auto& o : world.humans())
            if (o->state == State::Sleeping && (o.get() == &w || inGroupWith(*o) || isFriend(o->id)) &&
                cheb(o->cell, w.cell) <= WATCH_REACH)
                o->wakeUp(world, true, id, "@ WOKE ME. SOMEONE WAS COMING");
        note("WOKE THE OTHERS: @ WAS COMING", t->id);
        sayLater(talk::Moment::WakeUp, w.id);
        sleep_.watchee = -1;
        return false;
    }
    // Greedy and not fond of them, it may rob them instead.
    if (w.cakes > 0 && cakes < carryMax() && traits.greedy >= BETRAY_GREEDY && traits.brave >= ROB_BRAVE &&
        !isFriend(w.id) && trust(w.id) < BETRAY_TRUST && rng_.chance(BETRAY_CHANCE)) {
        sleep_.betraying = true;
        if (goRob(world, w)) return true;
        sleep_.betraying = false;
    }
    if (cheb(cell, w.cell) > WATCH_GAP) {
        Vec2i at = w.cell;
        auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= WATCH_GAP; }, true);
        if (!p) {
            sleep_.watchee = -1;
            return false;
        }
        startWalking(Goal::Watch, *p);
        return true;
    }
    sitDown();
    goal = Goal::Watch;
    if (w.cell.x != cell.x) facingLeft = w.cell.x < cell.x;
    return true;
}

bool Human::handOverWatch(World& world) {
    Human* best = nullptr;
    for (const auto& o : world.humans())
        if (o.get() != this && o->state == State::Sleeping && (inGroupWith(*o) || isFriend(o->id)) &&
            sight.sees(o->cell) && cheb(o->cell, cell) <= WATCH_REACH && o->tiredness < HANDOVER_FRESH &&
            (!best || o->tiredness < best->tiredness))
            best = o.get();
    if (!best) return false;
    note("WOKE @ TO TAKE A TURN ON WATCH", best->id);
    sayLater(talk::Moment::YourWatch, best->id);
    best->wakeUp(world, true, id, "@ WOKE ME TO TAKE MY TURN ON WATCH");
    best->sleep_.askedWatch = id;
    return true;
}

bool Human::askToWatch(World& world) {
    if (lightSleep() < ASK_LIGHT) return false;
    Human* best = nullptr;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && o->state != State::Sleeping && sight.sees(o->cell) &&
            cheb(o->cell, cell) <= ASK_REACH && (isFriend(o->id) || inGroupWith(*o)) && o->sleep_.watchee < 0 &&
            o->sleep_.askedWatch < 0 && !o->tired() && !o->hungry() && !o->thirsty() &&
            (!best || cheb(o->cell, cell) < cheb(best->cell, cell)))
            best = o.get();
    if (!best) return false;
    Human& h = *best;
    note("ASKED @ TO KEEP WATCH", h.id);
    sayLater(talk::Moment::WatchMe, h.id);
    if (h.isFriend(id) || h.traits.sociable >= ASK_SOCIABLE) {
        h.note("AGREED TO KEEP WATCH OVER @", id);
        h.sayLater(talk::Moment::WatchYes, id);
    } else if (h.traits.greedy >= ASK_GREEDY && cakes > 0) {
        cakes--;
        h.cakes++;
        note("GAVE @ A CAKE TO KEEP WATCH", h.id);
        h.note("KEEPING WATCH OVER @ FOR A CAKE", id);
        h.sayLater(talk::Moment::WatchCake, id);
    } else {
        h.note("WOULDN'T KEEP WATCH OVER @", id);
        h.sayLater(talk::Moment::WatchNo, id);
        return true;
    }
    h.sleep_.askedWatch = id;
    h.rethink_ = true;
    return true;
}

// Asleep, or not there: it failed them. There, and the harm came anyway:
// they watched it happen, or it was their own doing, known otherwise.
void Human::blameWatcher(World& world) {
    if (sleep_.watchedBy < 0) return;
    const Human& w = world.human(sleep_.watchedBy);
    sleep_.watchedBy = -1;
    if (!w.dead() && w.state != State::Sleeping && cheb(w.cell, cell) <= WATCH_REACH) return;
    note("@ WAS MEANT TO KEEP WATCH", w.id);
    sayLater(talk::Moment::FailedWatch, w.id);
    regard(w.id, TRUST_FAILED_WATCH);
}

bool Human::dreamt() const { return clock_ - sleep_.dreamtAt < DREAM_FRESH; }

// Every dream points somewhere else, so dreamers grow sure of ways that
// disagree, and tell them at greetings all the same.
void Human::dream() {
    sleep_.dreamtAt = clock_;
    note("DREAMT OF BEFORE, I THINK");
    sayLater(talk::Moment::Dream);
    float pull = std::remainder(rng_.range(0.0f, TAU) - escape_.hunch, TAU) * DREAM_PULL;
    escape_.hunch = std::fmod(escape_.hunch + pull + TAU, TAU);
    escape_.hunchSure = std::min(1.0f, escape_.hunchSure + DREAM_SURE);
}
