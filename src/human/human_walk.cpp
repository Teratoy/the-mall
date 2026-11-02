// Walking: stepping cell to cell, making way, arriving, idling and sitting,
// what it can reach, and the searches over remembered ground.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <queue>

// One cell along the path. A cell in a slot nobody has entered yet is
// generated just before the human steps into it. If someone stands on the
// next cell, the path is replanned around everyone in the way.
void Human::step(World& world) {
    moving = false;
    // Out of breath, it stands bent over until it has some back.
    if (winded()) {
        timer = run_.winded;
        return;
    }
    // Stepping aside, it thinks again once out of the way.
    if (rethink_ && goal != Goal::GiveWay) {
        choose(world);
        if (state != State::Walking) return;
    }
    // Walking up to someone: close enough to greet, or to call out to one
    // walking away from it, who would otherwise never be caught up with.
    if (goal == Goal::Meet && social_.partner >= 0) {
        Human& o = world.human(social_.partner);
        Vec2i went = o.cell - o.prev, apart = o.cell - cell;
        bool away = o.state == State::Walking && went.x * apart.x + went.y * apart.y > 0;
        int reach = away ? HAIL_RADIUS : GREET_RADIUS;
        if (sight.sees(o.cell) && cheb(cell, o.cell) <= reach) return tryGreet(world, o);
    }
    if (goal == Goal::Fight && fight_.foe >= 0) {
        Human& f = world.human(fight_.foe);
        if (!f.dead() && sight.sees(f.cell) && cheb(cell, f.cell) <= 1) return startPunch(f);
    }
    if (goal == Goal::Share && social_.sharee >= 0) {
        Human& o = world.human(social_.sharee);
        if (wouldShare(o) && sight.sees(o.cell) && cheb(cell, o.cell) <= 1) {
            give(o, Gift::Share);
            witness(world, *this, o, TRUST_SAW_FEED, 0.0f, Episode::What::Gift, EPISODE_SAW_GIFT, int(Gift::Share));
            return;
        }
    }
    // Leading someone to water, it lets them catch up.
    if (goal == Goal::Escort && escorting_.escortee >= 0 && cheb(cell, world.human(escorting_.escortee).cell) > ESCORT_GAP)
        return startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
    // Off on its own business ahead of its group, it lets them catch up.
    if ((goal == Goal::Explore || goal == Goal::Wander || goal == Goal::CheckDoor || goal == Goal::OpenDoor) &&
        waitForCompany(world))
        return;
    if (path_.empty()) return arrive(world);
    Vec2i next = path_.front();
    world.ensureGenerated(next);
    // An airlock door gives at a push from inside.
    if (world.tile(next) == Tile::DoorLocked && pushesOpen(cell, next)) world.pushOpen(next, cell);
    if (!passable(world.tile(next)) || avoids(next)) {
        // The world isn't what it remembered, say a gate slammed, or a trap
        // was found on the way: look again, then think again.
        observe(world);
        return startIdle(0.0f, LOOK_TIME_MIN);
    }
    if (Human* o = world.occupant(next); o && o != this) {
        // A sitter gets up for someone wanting past.
        if (o->state == State::Sitting) o->rethink_ = true;
        auto around = search(world, [&](Vec2i c) { return c == target_; }, true);
        if (!around || around->empty() || world.occupant(around->front())) {
            // No way round: after a moment one of the two steps aside, else
            // in the end it gives up and thinks again.
            if (stuck_ > 0 && makeWay(world, *o)) return;
            if (++stuck_ >= GIVE_UP_AFTER) {
                // Exploring, it turns back rather than make for the same way past them again.
                if (goal == Goal::Explore) heading_ = Vec2i{-heading_.x, -heading_.y};
                return startIdle(0.0f, LOOK_TIME_MIN);
            }
            timer = STEP_TIME * rng_.range(0.5f, 1.0f);
            return;
        }
        path_.assign(around->begin(), around->end());
        next = path_.front();
    }
    path_.pop_front();
    move(world, next);
}

// Two walking into each other: the shorter step aside wins, so whoever is
// nearer a side room or a corner backs into it. Someone just standing
// there is the one to move; nobody busy eating, greeting, fighting or
// seated is asked.
bool Human::makeWay(World& world, Human& o) {
    bool standing = o.state == State::Idle && !o.fight_.provoked;
    bool blocked = o.state == State::Walking && !o.moving && o.goal != Goal::GiveWay;
    if (!standing && !blocked) return false;
    bool headOn = blocked && !o.path_.empty() && o.path_.front() == cell;
    auto theirs = o.asideFrom(world, *this);
    auto mine = headOn ? asideFrom(world, o) : std::nullopt;
    if (mine && (!theirs || mine->size() < theirs->size() || (mine->size() == theirs->size() && id < o.id))) {
        startWalking(Goal::GiveWay, *mine);
        return true;
    }
    if (!theirs) return false;
    o.startWalking(Goal::GiveWay, *theirs);
    o.timer = 0.0f;
    stuck_ = 0;
    timer = STEP_TIME;
    return true;
}

std::optional<std::vector<Vec2i>> Human::asideFrom(const World& world, const Human& passer) const {
    std::unordered_set<Vec2i, Vec2iHash> way(passer.path_.begin(), passer.path_.end());
    way.insert(passer.cell);
    return search(world, [&](Vec2i c) { return !way.count(c) && !world.occupant(c); }, true);
}

void Human::move(World& world, Vec2i next) {
    prev = cell;
    world.moveOccupant(cell, next, this);
    cell = next;
    moving = true;
    stuck_ = 0;
    if (next.x != prev.x) facingLeft = next.x < prev.x;
    stepTime = STEP_TIME * (running() ? RUN : 1.0f) * (next.x != prev.x && next.y != prev.y ? DIAGONAL : 1.0f) *
               (1.0f + DAZE_SLOW * escape_.daze) * (exhausted() ? EXHAUSTED_SLOW : 1.0f);
    timer = stepTime;
    world.setOff(cell, *this);
    if (dead()) return;
    // Come up to look at something, it looks before it takes anything.
    if (goal != Goal::Inspect || !path_.empty()) takeInReach(world);
    enterChunk(world);
    bool news = observe(world);
    // A hidden trap beside it may catch its eye.
    for (Vec2i d : DIRS8)
        if (const Trap* t = world.trap(cell + d);
            t && !memory.traps.count(cell + d) &&
            rng_.chance(NOTICE_TRAP_CHANCE * (world.light(cell + d) > 0 ? 1.0f : NOTICE_TRAP_DARK))) {
            news |= memory.traps.emplace(cell + d, t->kind).second;
            note(std::string("SPOTTED HIDDEN ") + trapWord(t->kind));
            rememberAt(cell + d, Episode::What::Spotted, EPISODE_SPOTTED, -1, -1, int(t->kind));
            trapWhereTold(cell + d);
            // The first it ever spots it stops and looks at.
            if (!inspect_.trap) {
                inspect_.trap = true;
                if (!discover({talk::Moment::SpotTrap, cell + d, 1, -1, std::nullopt, true}))
                    sayLater(talk::Moment::SpotTrap);
            }
        }
    checkDoorsNear();
    if (news && goal != Goal::PassDoor && goal != Goal::GiveWay) rethink_ = true;
    // Seen opened from afar: no need to walk up to it.
    if (goal == Goal::CheckDoor && !lockedDoorBeside(target_)) rethink_ = true;
    if (goal == Goal::Explore && !isFrontier(target_) && !openDoorBeside(target_) && !untriedDoorBeside(target_, true))
        rethink_ = true;
    if (inspectStale()) rethink_ = true;
    if (goal == Goal::FetchCake && !cakeInReach(target_)) rethink_ = true;
    // Seen empty, or its keys gone, the machine is no use.
    if (goal == Goal::Vend && (!canBuy() || stockOf(memory.machines[vend_.at]) <= 0)) rethink_ = true;
    // The voice it was going to has come into sight, or gone stale.
    if (goal == Goal::Voice && !voiceToGo()) rethink_ = true;
    // Making for a voice with a key, a gate seen near it is worth going to.
    if (goal == Goal::Rescue && keys > 0 && rescue_.rescuee >= 0) {
        auto it = rescue_.cries.find(rescue_.rescuee);
        if (it != rescue_.cries.end() && !it->second.gate && gateNear(it->second.at)) rethink_ = true;
    }
    if (goal == Goal::Loot && !worthLooting(deaths_.corpse, traits.brave < TIMID)) rethink_ = true;
    if (goal == Goal::Feed && !wouldEat(mind_.meal)) rethink_ = true;
    // They move too; keep the plan fresh.
    if (goal == Goal::Meet || goal == Goal::Follow || goal == Goal::Company || goal == Goal::Fight ||
        goal == Goal::Share || goal == Goal::Ask)
        rethink_ = true;
}

void Human::arrive(World& world) {
    if (errandGoal(goal)) errand_.want = Want::Count;
    switch (goal) {
    case Goal::OpenDoor: {
        // Come to let someone out, it opens their gate whatever else is beside it.
        auto door = lockedDoorBeside(cell);
        bool rescue = rescue_.rescuee >= 0 && std::abs(rescue_.gate.x - cell.x) + std::abs(rescue_.gate.y - cell.y) == 1 &&
                      memory.tile(rescue_.gate) == Tile::DoorLocked;
        if (rescue) door = rescue_.gate;
        if (door && keys > 0 && world.unlock(*door)) {
            // Knowing what the double doors cost, the curious pay to see.
            if (server_.keyWise && memory.airlocks.count(*door) && keysFor(*door) == 2) {
                note("OPENED THE DOUBLE DOORS ANYWAY");
                sayLater(talk::Moment::DareAirlock);
            }
            keys--;
            note("UNLOCKED A DOOR");
            repeated(Repeat::Doors);
            rememberAt(*door, Episode::What::Door, EPISODE_DOOR, id);
            for (const auto& h : world.humans())
                if (h.get() != this && !h->dead() && h->sight.sees(*door))
                    h->rememberAt(*door, Episode::What::Door, EPISODE_SAW_DOOR, id);
            lift(HOPE_DOOR);
            if (lessons.keysFirst > 0.0f) learn(lessons.keysFirst, false);
            observe(world);
            meetAirlock(*door);
            // Letting someone out, it waits for them at the gate.
            if (letOut(world, *door)) return startIdle(UNLOCK_TIME, UNLOCK_TIME * 2.0f);
            startPassDoor(*door);
            timer = UNLOCK_TIME;
            return;
        }
        // Someone opened it while it wasn't looking, or it was never there:
        // a look, so it stops coming back to a door it remembers wrong.
        observe(world);
        return startIdle(0.0f, LOOK_TIME_MIN);
    }
    case Goal::Explore:
        if (auto door = openDoorBeside(cell)) return startPassDoor(*door);
        if (auto door = untriedDoorBeside(cell, true)) {
            if (tryDoor(world, *door)) return startPassDoor(*door);
            return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
        }
        break;
    case Goal::Wander:
        return rest();
    case Goal::Inspect:
        if (inspect_.going) {
            Find f = std::move(*inspect_.going);
            inspect_.going.reset();
            if (!gone(f)) return lookAt(f, true);
            sayFind(f);
        }
        return choose(world);
    case Goal::Wash:
        if (startWash(world)) return;
        return choose(world);
    case Goal::Drink:
        if (seekWater(world)) return;
        return choose(world);
    case Goal::Fill:
        if (auto w = waterBeside(cell); w && startFilling(world, *w)) return;
        return choose(world);
    case Goal::Ask:
    case Goal::Pick:
        return choose(world);
    case Goal::Stash:
        if (stashHere()) return;
        return choose(world);
    case Goal::Vend:
        if (startVending(world)) return;
        return choose(world);
    case Goal::Relieve:
        if (seekRelief(world)) return;
        return choose(world);
    case Goal::Sleep:
        if (seekSleep(world)) return;
        return choose(world);
    case Goal::Calm:
        if (startCalming(world)) return;
        return choose(world);
    case Goal::Headbang:
        if (cheb(headbang_.wall, cell) == 1 && headbang_.wall.y == cell.y && memory.tile(headbang_.wall) == Tile::Wall &&
            stuckForGood())
            return startHeadbanging(world, headbang_.wall);
        return choose(world);
    case Goal::Bang:
        if (std::abs(rescue_.bangAt.x - cell.x) + std::abs(rescue_.bangAt.y - cell.y) == 1 &&
            memory.tile(rescue_.bangAt) == Tile::DoorLocked)
            return startBanging(world, rescue_.bangAt);
        return choose(world);
    case Goal::CheckDoor:
        // Never tried, it tries the handle; it gives only from an airlock's inside.
        if (auto door = untriedDoorBeside(cell, false); door && tryDoor(world, *door)) return startPassDoor(*door);
        // Still locked: a look at it, then on to something else.
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    case Goal::Meet:
    case Goal::Follow:
    case Goal::Company:
    case Goal::FetchCake:
    case Goal::Fight:
    case Goal::Share:
        return choose(world);
    case Goal::Flee:
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    case Goal::GiveWay:
        startIdle(GIVE_WAY_WAIT_MIN, GIVE_WAY_WAIT_MAX);
        goal = Goal::GiveWay;
        return;
    case Goal::Avoid:
    case Goal::Escort:
    case Goal::Escorted:
    case Goal::Track:
        return choose(world);
    case Goal::Rob:
        if (startRob(world)) return;
        return choose(world);
    case Goal::Loot:
        if (startLoot(world, traits.brave < TIMID)) return;
        return choose(world);
    case Goal::Feed:
        if (startFeed(world, mind_.meal)) return;
        return choose(world);
    case Goal::Rescue:
        if (rescue_.rescuee >= 0 && startRescue(world)) return;
        return choose(world);
    case Goal::Voice:
        if (startVoice(world)) return;
        return choose(world);
    default:
        break;
    }
    if (rng_.chance(LOOK_AROUND_CHANCE))
        startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    else
        choose(world);
}

void Human::startIdle(float lo, float hi) {
    state = State::Idle;
    moving = false;
    stuck_ = 0;
    path_.clear();
    timer = rng_.range(lo, hi);
}

void Human::rest() {
    // Never sits down by a smell.
    if (stinksHere()) return startIdle(IDLE_TIME_MIN, IDLE_TIME_MAX);
    if (!rng_.chance(SIT_CHANCE * sway(1.0f - traits.curious, SIT_CURIOUS) * (1.0f + DREAD_SIT * dread_) *
                     (1.0f + GUILT_SIT * mind_.guilt) * (1.0f + DESPAIR_SIT * gloom())))
        return startIdle(IDLE_TIME_MIN, IDLE_TIME_MAX);
    sitDown();
}

void Human::sitDown() {
    chat_.restedAt = clock_;
    startIdle(SIT_TIME_MIN, SIT_TIME_MAX);
    state = State::Sitting;
    water_.lying = false;
    animTime = 0.0f;
    glance_ = SIT_GLANCE;
}

void Human::lieDown() {
    sitDown();
    water_.lying = true;
    timer = rng_.range(LIE_TIME_MIN, LIE_TIME_MAX);
}

void Human::sitUp() {
    state = State::Sitting;
    water_.lying = false;
    animTime = 0.0f;
    timer = rng_.range(WAKE_SIT_MIN, WAKE_SIT_MAX);
    glance_ = 0.0f;
    escape_.comingTo = true;
    note("WOKE UP, REMEMBERING NOTHING");
    remember(Episode::What::Woke, EPISODE_WOKE, id);
    sayLater(talk::Moment::Wake);
}

void Human::startRising() {
    // Up for the first time, steadying itself, the fog not yet gone.
    if (escape_.comingTo) {
        note("GOT UP, STILL IN A FOG");
        sayLater(talk::Moment::WakeStand);
    }
    escape_.comingTo = false;
    sleep_.roused = false;
    state = State::Rising;
    animTime = 0.0f;
    timer = STAND_UP_TIME;
}

void Human::startWalking(Goal g, std::vector<Vec2i> path) {
    state = State::Walking;
    awaiting_ = false;
    run_.on = false;
    goal = g;
    path_.assign(path.begin(), path.end());
    target_ = path.empty() ? cell : path.back();
    if (g == Goal::Explore && target_ != cell) heading_ = target_ - cell;
}

int Human::headingCost(Vec2i c) const {
    Vec2i d = c - cell;
    if (d == Vec2i{0, 0} || heading_ == Vec2i{0, 0}) return 0;
    float along = float(d.x * heading_.x + d.y * heading_.y) /
                  (std::hypot(float(d.x), float(d.y)) * std::hypot(float(heading_.x), float(heading_.y)));
    return int(float(HEADING_COST) * (1.0f - along) / 2.0f);
}

// Straight through: the door, its other half, and one step in.
// Through a door, the way out feels a little more like the way it went.
void Human::startPassDoor(Vec2i door) {
    Vec2i d = door - cell;
    float hx = std::cos(escape_.hunch) * (1.0f - HUNCH_FOLLOW) + float(d.x) * HUNCH_FOLLOW;
    float hy = std::sin(escape_.hunch) * (1.0f - HUNCH_FOLLOW) + float(d.y) * HUNCH_FOLLOW;
    if (hx != 0.0f || hy != 0.0f) escape_.hunch = std::atan2(hy, hx);
    heading_ = d;
    startWalking(Goal::PassDoor, {door, door + d, door + d * 2});
}

// A plate under a table goes off as the cake is lifted; one underfoot
// already went off as it was stepped on.
void Human::takeInReach(World& world) {
    for (Vec2i d : REACH) {
        Vec2i c = cell + d;
        bool underfoot = d == Vec2i{};
        if (!underfoot && (world.tile(c) != Tile::Table || avoids(c))) continue;
        if (!world.hasKey(c)) memory.keys.erase(c);
        if (!world.hasCake(c)) memory.cakes.erase(c);
        bool key = world.hasKey(c), cake = cakes < carryMax() && world.hasCake(c) && shyOf(c) == Shy::None;
        if (!key && !cake) continue;
        if (!underfoot) world.setOff(c, *this);
        if (key && world.takeKey(c)) {
            keys++;
            note("PICKED UP A KEY");
            memory.keys.erase(c);
            if (social_.leader >= 0) sayLater(talk::Moment::PartOwnKey, social_.leader);
            social_.leader = -1;
        }
        if (cake && world.takeCake(c)) {
            cakes++;
            note("PICKED UP A CAKE");
            memory.cakes.erase(c);
            gotFor(Want::Fetch);
            if (hungry()) rethink_ = true;
        }
    }
}

std::optional<Vec2i> Human::inReach(Vec2i at, const std::unordered_set<Vec2i, Vec2iHash>& items) const {
    for (Vec2i d : REACH) {
        Vec2i c = at + d;
        if (items.count(c) && (d == Vec2i{} || (memory.tile(c) == Tile::Table && !avoids(c)))) return c;
    }
    return std::nullopt;
}

void Human::checkDoorsNear() {
    for (int y = -DOOR_CHECK_RANGE; y <= DOOR_CHECK_RANGE; y++)
        for (int x = -DOOR_CHECK_RANGE; x <= DOOR_CHECK_RANGE; x++) {
            Vec2i c = cell + Vec2i{x, y};
            if (memory.tile(c) != Tile::DoorLocked) continue;
            // The whole door, however wide, along its wall.
            for (Vec2i d : DIRS4)
                for (Vec2i e = c; memory.tile(e) == Tile::DoorLocked; e = e + d) doorChecked_[e] = clock_;
        }
}

bool Human::doorDue(Vec2i c) const {
    auto door = lockedDoorBeside(c);
    if (!door) return false;
    auto it = doorChecked_.find(*door);
    return it == doorChecked_.end() || clock_ - it->second >= DOOR_RECHECK_TIME;
}

bool Human::knownPassable(Vec2i c) const { return passable(memory.tile(c)); }

// Only once it has pushed it from there and felt it give.
bool Human::pushesOpen(Vec2i from, Vec2i door) const {
    if (!memory.pushes.count(door)) return false;
    auto it = memory.airlocks.find(door);
    return it != memory.airlocks.end() && from == door + it->second;
}

// The starving would rather be hurt than go without the cake on a dart plate.
bool Human::avoids(Vec2i c) const {
    auto it = memory.traps.find(c);
    return it != memory.traps.end() && !(it->second == TrapKind::Darts && starving());
}

bool Human::isFrontier(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return false;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::Void) return true;
    return false;
}

int Human::hunchCost(Vec2i c, int most) const {
    Vec2i d = c - cell;
    if (d == Vec2i{0, 0}) return 0;
    float along = (float(d.x) * std::cos(escape_.hunch) + float(d.y) * std::sin(escape_.hunch)) / std::hypot(float(d.x), float(d.y));
    return int(float(most) * escape_.out * (1.0f - along) / 2.0f);
}

// An airlock door that gives at a push from here is no locked door.
std::optional<Vec2i> Human::lockedDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::DoorLocked && !pushesOpen(c, c + d)) return c + d;
    return std::nullopt;
}

bool Human::doorKnownLocked(Vec2i door) const {
    return doors_.learnt || doors_.tried.count(door) || doors_.told.count(door);
}

std::optional<Vec2i> Human::untriedDoorBeside(Vec2i c, bool anySide) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (int i = 0; i < 4; i++) {
        Vec2i door = c + DIRS4[i];
        if (memory.tile(door) != Tile::DoorLocked || pushesOpen(c, door)) continue;
        if (!doorKnownLocked(door)) return door;
        // The way from the door back to c.
        auto it = doors_.tried.find(door);
        if (anySide && (it == doors_.tried.end() || !(it->second & (1u << ((i + 2) % 4))))) return door;
    }
    return std::nullopt;
}

bool Human::doorsLeftToTry() const {
    for (Vec2i door : doors_.seen)
        for (Vec2i d : DIRS4)
            if (untriedDoorBeside(door + d, true) == door) return true;
    return false;
}

std::vector<Vec2i> Human::doorCells(Vec2i door) const {
    std::vector<Vec2i> cells{door};
    for (Vec2i d : DIRS4)
        for (Vec2i e = door + d; memory.tile(e) == Tile::DoorLocked; e = e + d) cells.push_back(e);
    return cells;
}

// The handle first: most give nothing, and after a few it stops hoping.
bool Human::tryDoor(World& world, Vec2i door) {
    int side = 0;
    while (side < 4 && door + DIRS4[side] != cell) side++;
    bool fresh = !doorKnownLocked(door);
    for (Vec2i c : doorCells(door)) doors_.tried[c] |= uint8_t(1u << side);
    // An airlock door gives from inside, and from then on it knows it does.
    if (world.pushOpen(door, cell)) {
        memory.pushes.insert(door);
        note("PUSHED A DOOR OPEN");
        sayLater(talk::Moment::DoorGave);
        observe(world);
        return true;
    }
    world.noise(NoiseKind::Knob, door, id);
    if (!fresh) return false;
    note("TRIED A DOOR: LOCKED");
    if (++doors_.locked >= DOOR_LESSON && !doors_.learnt) {
        doors_.learnt = true;
        note("LEARNT THAT SHUT DOORS ARE LOCKED");
        sayLater(talk::Moment::LearnDoors);
    } else {
        sayLater(talk::Moment::TryDoorLocked);
    }
    return false;
}

// An airlock door that gives at a push from here is as good as open.
std::optional<Vec2i> Human::openDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4) {
        bool open = memory.tile(c + d) == Tile::DoorOpen || (memory.tile(c + d) == Tile::DoorLocked && pushesOpen(c, c + d));
        if (open && memory.tile(c + d * 2) == Tile::Void) return c + d;
    }
    return std::nullopt;
}

// Dijkstra over remembered passable cells, keeping off known traps, 8
// ways, cutting a corner only when both cells beside it are open. Ties break at random so equal
// choices don't always go the same way.
std::optional<std::vector<Vec2i>> Human::search(const World& world, const std::function<bool(Vec2i)>& done,
                                                bool avoidCrowd, const std::function<int(Vec2i)>& goalCost) const {
    return searchFrom(world, cell, done, avoidCrowd, goalCost);
}

namespace {

/// What one search knows of a cell: its best cost so far and the cell it
/// came from, and whether the cell is open to it, worked out once.
struct SearchCell {
    int cost;
    Vec2i from;
    uint32_t reached = 0;
    uint32_t judged = 0;
    bool open;
};

/// The cells of every search, kept between searches so they need not be
/// made and cleared each time: a cell counts for a search only when its
/// stamp is that search's. Held per chunk slot, the last slot looked up
/// kept at hand, since a search mostly steps within one slot.
class SearchGrid {
public:
    void begin() {
        if (++stamp_ == 0) {
            for (auto& block : blocks_) std::fill(block.begin(), block.end(), SearchCell{});
            stamp_ = 1;
        }
        lastSlot_ = {INT32_MIN, INT32_MIN};
        last_ = nullptr;
    }
    uint32_t stamp() const { return stamp_; }

    SearchCell& at(Vec2i c) {
        Vec2i slot{floorDiv(c.x, CHUNK), floorDiv(c.y, CHUNK)};
        if (slot != lastSlot_) {
            auto [it, made] = slots_.try_emplace(slot, blocks_.size());
            if (made) blocks_.emplace_back(size_t(CHUNK * CHUNK));
            lastSlot_ = slot;
            last_ = blocks_[it->second].data();
        }
        Vec2i l = c - slot * CHUNK;
        return last_[l.y * CHUNK + l.x];
    }

    /// The cell and the eight round it, by [dy + 1][dx + 1]: away from a
    /// slot's edge, all nine lie in one block at fixed steps from the cell.
    void around(Vec2i c, SearchCell* out[3][3]) {
        SearchCell* mid = &at(c);
        Vec2i l = c - lastSlot_ * CHUNK;
        if (l.x > 0 && l.y > 0 && l.x < CHUNK - 1 && l.y < CHUNK - 1) {
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) out[dy + 1][dx + 1] = mid + dy * CHUNK + dx;
            return;
        }
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) out[dy + 1][dx + 1] = &at(c + Vec2i{dx, dy});
    }

private:
    std::unordered_map<Vec2i, size_t, Vec2iHash> slots_;
    std::vector<std::vector<SearchCell>> blocks_;
    uint32_t stamp_ = 0;
    Vec2i lastSlot_{INT32_MIN, INT32_MIN};
    SearchCell* last_ = nullptr;
};

/// One grid per depth, in case a search is ever begun from inside another.
std::vector<std::unique_ptr<SearchGrid>> searchGrids;
size_t searchDepth = 0;

} // namespace

// A done cell with a goal cost goes back in the queue as an ending, at
// what it costs to get there and end there; the first ending out wins.
std::optional<std::vector<Vec2i>> Human::searchFrom(const World& world, Vec2i start,
                                                    const std::function<bool(Vec2i)>& done, bool avoidCrowd,
                                                    const std::function<int(Vec2i)>& goalCost) const {
    struct Node {
        int cost;
        uint32_t tie;
        Vec2i at;
        bool ending = false;
        bool operator>(const Node& o) const { return cost != o.cost ? cost > o.cost : tie > o.tie; }
    };
    if (searchGrids.size() <= searchDepth) searchGrids.push_back(std::make_unique<SearchGrid>());
    SearchGrid& grid = *searchGrids[searchDepth];
    struct Depth {
        Depth() { searchDepth++; }
        ~Depth() { searchDepth--; }
    } depth;
    grid.begin();
    const uint32_t stamp = grid.stamp();
    auto shunned = shunZone(world, start);
    auto open = [&](Vec2i c, SearchCell& s) {
        if (s.judged == stamp) return s.open;
        s.judged = stamp;
        s.open = false;
        if (!knownPassable(c) || avoids(c) || (shunned && shunned(c))) return false;
        if (searchReach_ > 0 && cheb(c, start) > searchReach_) return false;
        Human* o = avoidCrowd ? world.occupant(c) : nullptr;
        s.open = !o || o == this || !sight.sees(c);
        return s.open;
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> frontier;
    Rng tie(rng_.next());
    frontier.push({0, 0, start});
    {
        SearchCell& s = grid.at(start);
        s.reached = stamp;
        s.cost = 0;
    }
    int expanded = 0;
    while (!frontier.empty() && expanded < SEARCH_LIMIT) {
        Node n = frontier.top();
        frontier.pop();
        int extra = 0;
        if (!n.ending) {
            if (n.cost > grid.at(n.at).cost) continue;
            if (done(n.at) && goalCost) extra = goalCost(n.at);
        }
        if (n.ending || (extra <= 0 && done(n.at))) {
            std::vector<Vec2i> path;
            for (Vec2i c = n.at; c != start; c = grid.at(c).from) path.push_back(c);
            return std::vector<Vec2i>(path.rbegin(), path.rend());
        }
        if (extra > 0) frontier.push({n.cost + extra, uint32_t(tie.next()), n.at, true});
        expanded++;
        // Each way judged once: the corners look at the straight ways beside them.
        SearchCell* near[3][3];
        grid.around(n.at, near);
        bool way[3][3];
        for (Vec2i d : DIRS8) way[d.y + 1][d.x + 1] = open(n.at + d, *near[d.y + 1][d.x + 1]);
        for (Vec2i d : DIRS8) {
            Vec2i m = n.at + d;
            bool diagonal = d.x != 0 && d.y != 0;
            // A shut airlock door is a way out to whoever is inside it.
            bool pushed = !diagonal && memory.tile(m) == Tile::DoorLocked && pushesOpen(n.at, m) && !avoids(m);
            if (!way[d.y + 1][d.x + 1] && !pushed) continue;
            if (diagonal && (!way[1][d.x + 1] || !way[d.y + 1][1])) continue;
            // Blood, mess and smell only add, so a cell already reached as cheaply is skipped unweighed.
            int c = n.cost + (diagonal ? 14 : 10);
            SearchCell& s = *near[d.y + 1][d.x + 1];
            if (s.reached == stamp && s.cost <= c) continue;
            c += bloodCost(m) + messCost(world, m) + smellCost(m);
            if (s.reached == stamp && s.cost <= c) continue;
            s.reached = stamp;
            s.cost = c;
            s.from = n.at;
            frontier.push({c, uint32_t(tie.next()), m});
        }
    }
    return std::nullopt;
}
