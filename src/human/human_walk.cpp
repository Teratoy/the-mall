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
#include <queue>

// One cell along the path. A cell in a slot nobody has entered yet is
// generated just before the human steps into it. If someone stands on the
// next cell, the path is replanned around everyone in the way.
void Human::step(World& world) {
    moving = false;
    // Stepping aside, it thinks again once out of the way.
    if (rethink_ && goal != Goal::GiveWay) {
        choose(world);
        if (state != State::Walking) return;
    }
    if (goal == Goal::Meet && partner_ >= 0) {
        Human& o = world.human(partner_);
        if (sight.sees(o.cell) && cheb(cell, o.cell) <= GREET_RADIUS) return tryGreet(world, o);
    }
    if (goal == Goal::Fight && foe_ >= 0) {
        Human& f = world.human(foe_);
        if (!f.dead() && sight.sees(f.cell) && cheb(cell, f.cell) <= 1) return startPunch(f);
    }
    if (goal == Goal::Share && sharee_ >= 0) {
        Human& o = world.human(sharee_);
        if (wouldShare(o) && sight.sees(o.cell) && cheb(cell, o.cell) <= 1) {
            give(o, Gift::Share);
            witness(world, *this, o, TRUST_SAW_FEED, 0.0f, Episode::What::Gift, EPISODE_SAW_GIFT, int(Gift::Share));
            return;
        }
    }
    // Leading someone to water, it lets them catch up.
    if (goal == Goal::Escort && escortee_ >= 0 && cheb(cell, world.human(escortee_).cell) > ESCORT_GAP)
        return startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
    // Off on its own business ahead of its group, it lets them catch up.
    if ((goal == Goal::Explore || goal == Goal::Wander || goal == Goal::CheckDoor || goal == Goal::OpenDoor) &&
        waitForCompany(world))
        return;
    if (path_.empty()) return arrive(world);
    Vec2i next = path_.front();
    world.ensureGenerated(next);
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
            if (++stuck_ >= GIVE_UP_AFTER) return startIdle(0.0f, LOOK_TIME_MIN);
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
    bool standing = o.state == State::Idle && !o.provoked_;
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
    bool running = goal == Goal::Fight || goal == Goal::Flee;
    stepTime = STEP_TIME * (running ? RUN : 1.0f) * (next.x != prev.x && next.y != prev.y ? DIAGONAL : 1.0f) *
               (1.0f + DAZE_SLOW * daze_) * (exhausted() ? EXHAUSTED_SLOW : 1.0f);
    timer = stepTime;
    world.setOff(cell, *this);
    if (dead()) return;
    takeInReach(world);
    enterChunk(world);
    bool news = observe(world);
    // A hidden trap beside it may catch its eye.
    for (Vec2i d : DIRS8)
        if (const Trap* t = world.trap(cell + d); t && !memory.traps.count(cell + d) && rng_.chance(NOTICE_TRAP_CHANCE)) {
            news |= memory.traps.emplace(cell + d, t->kind).second;
            note(std::string("SPOTTED HIDDEN ") + trapWord(t->kind));
            rememberAt(cell + d, Episode::What::Spotted, EPISODE_SPOTTED, -1, -1, int(t->kind));
        }
    checkDoorsNear();
    if (news && goal != Goal::PassDoor && goal != Goal::GiveWay) rethink_ = true;
    // Seen opened from afar: no need to walk up to it.
    if (goal == Goal::CheckDoor && !lockedDoorBeside(target_)) rethink_ = true;
    if (goal == Goal::Explore && !isFrontier(target_) && !openDoorBeside(target_)) rethink_ = true;
    if (goal == Goal::FetchCake && !inReach(target_, memory.cakes)) rethink_ = true;
    if (goal == Goal::Loot && !worthLooting(corpse_, traits.brave < TIMID)) rethink_ = true;
    if (goal == Goal::Feed && !wouldEat(meal_)) rethink_ = true;
    // They move too; keep the plan fresh.
    if (goal == Goal::Meet || goal == Goal::Follow || goal == Goal::Company || goal == Goal::Fight ||
        goal == Goal::Share)
        rethink_ = true;
}

void Human::arrive(World& world) {
    switch (goal) {
    case Goal::OpenDoor: {
        // Come to let someone out, it opens their gate whatever else is beside it.
        auto door = lockedDoorBeside(cell);
        bool rescue = rescuee_ >= 0 && std::abs(rescueGate_.x - cell.x) + std::abs(rescueGate_.y - cell.y) == 1 &&
                      memory.tile(rescueGate_) == Tile::DoorLocked;
        if (rescue) door = rescueGate_;
        if (door && keys > 0 && world.unlock(*door)) {
            keys--;
            note("UNLOCKED A DOOR");
            rememberAt(*door, Episode::What::Door, EPISODE_DOOR, id);
            for (const auto& h : world.humans())
                if (h.get() != this && !h->dead() && h->sight.sees(*door))
                    h->rememberAt(*door, Episode::What::Door, EPISODE_SAW_DOOR, id);
            lift(HOPE_DOOR);
            if (lessons.keysFirst > 0.0f) learn(lessons.keysFirst, false);
            observe(world);
            // Letting someone out, it waits for them at the gate.
            if (letOut(world, *door)) return startIdle(UNLOCK_TIME, UNLOCK_TIME * 2.0f);
            startPassDoor(*door);
            timer = UNLOCK_TIME;
            return;
        }
        return choose(world);
    }
    case Goal::Explore:
        if (auto door = openDoorBeside(cell)) return startPassDoor(*door);
        break;
    case Goal::Wander:
        return rest();
    case Goal::Wash:
        if (startWash(world)) return;
        return choose(world);
    case Goal::Drink:
        if (seekWater(world)) return;
        return choose(world);
    case Goal::Relieve:
        if (seekRelief(world)) return;
        return choose(world);
    case Goal::Sleep:
        if (seekSleep(world)) return;
        return choose(world);
    case Goal::CheckDoor:
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
        if (startFeed(world, meal_)) return;
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
    if (!rng_.chance(SIT_CHANCE * sway(1.0f - traits.curious, SIT_CURIOUS) * (1.0f + DREAD_SIT * dread_) *
                     (1.0f + GUILT_SIT * guilt_) * (1.0f + DESPAIR_SIT * gloom())))
        return startIdle(IDLE_TIME_MIN, IDLE_TIME_MAX);
    sitDown();
}

void Human::sitDown() {
    restedAt_ = clock_;
    startIdle(SIT_TIME_MIN, SIT_TIME_MAX);
    state = State::Sitting;
    animTime = 0.0f;
    glance_ = SIT_GLANCE;
}

void Human::sitUp() {
    state = State::Sitting;
    animTime = 0.0f;
    timer = rng_.range(WAKE_SIT_MIN, WAKE_SIT_MAX);
    glance_ = 0.0f;
    comingTo_ = true;
    note("WOKE UP, REMEMBERING NOTHING");
    remember(Episode::What::Woke, EPISODE_WOKE, id);
    sayLater(talk::Moment::Wake);
}

void Human::startRising() {
    comingTo_ = false;
    roused_ = false;
    state = State::Rising;
    animTime = 0.0f;
    timer = STAND_UP_TIME;
}

void Human::startWalking(Goal g, std::vector<Vec2i> path) {
    state = State::Walking;
    awaiting_ = false;
    goal = g;
    path_.assign(path.begin(), path.end());
    target_ = path.empty() ? cell : path.back();
}

// Straight through: the door, its other half, and one step in.
// Through a door, the way out feels a little more like the way it went.
void Human::startPassDoor(Vec2i door) {
    Vec2i d = door - cell;
    float hx = std::cos(hunch_) * (1.0f - HUNCH_FOLLOW) + float(d.x) * HUNCH_FOLLOW;
    float hy = std::sin(hunch_) * (1.0f - HUNCH_FOLLOW) + float(d.y) * HUNCH_FOLLOW;
    if (hx != 0.0f || hy != 0.0f) hunch_ = std::atan2(hy, hx);
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
        bool key = world.hasKey(c), cake = cakes < carryMax() && world.hasCake(c);
        if (!key && !cake) continue;
        if (!underfoot) world.setOff(c, *this);
        if (key && world.takeKey(c)) {
            keys++;
            note("PICKED UP A KEY");
            memory.keys.erase(c);
            if (leader_ >= 0) sayLater(talk::Moment::PartOwnKey, leader_);
            leader_ = -1;
        }
        if (cake && world.takeCake(c)) {
            cakes++;
            note("PICKED UP A CAKE");
            memory.cakes.erase(c);
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
    float along = (float(d.x) * std::cos(hunch_) + float(d.y) * std::sin(hunch_)) / std::hypot(float(d.x), float(d.y));
    return int(float(most) * out_ * (1.0f - along) / 2.0f);
}

std::optional<Vec2i> Human::lockedDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::DoorLocked) return c + d;
    return std::nullopt;
}

std::optional<Vec2i> Human::openDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::DoorOpen && memory.tile(c + d * 2) == Tile::Void) return c + d;
    return std::nullopt;
}

// Dijkstra over remembered passable cells, keeping off known traps, 8
// ways, cutting a corner only when both cells beside it are open. Ties break at random so equal
// choices don't always go the same way.
std::optional<std::vector<Vec2i>> Human::search(const World& world, const std::function<bool(Vec2i)>& done,
                                                bool avoidCrowd, const std::function<int(Vec2i)>& goalCost) const {
    return searchFrom(world, cell, done, avoidCrowd, goalCost);
}

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
    auto shunned = shunZone(world, start);
    auto open = [&](Vec2i c) {
        if (!knownPassable(c) || avoids(c) || (shunned && shunned(c))) return false;
        if (!avoidCrowd) return true;
        Human* o = world.occupant(c);
        return !o || o == this || !sight.sees(c);
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> frontier;
    std::unordered_map<Vec2i, int, Vec2iHash> cost;
    std::unordered_map<Vec2i, Vec2i, Vec2iHash> from;
    Rng tie(rng_.next());
    frontier.push({0, 0, start});
    cost[start] = 0;
    int expanded = 0;
    while (!frontier.empty() && expanded < SEARCH_LIMIT) {
        Node n = frontier.top();
        frontier.pop();
        int extra = 0;
        if (!n.ending) {
            if (n.cost > cost[n.at]) continue;
            if (done(n.at) && goalCost) extra = goalCost(n.at);
        }
        if (n.ending || (extra <= 0 && done(n.at))) {
            std::vector<Vec2i> path;
            for (Vec2i c = n.at; c != start; c = from[c]) path.push_back(c);
            return std::vector<Vec2i>(path.rbegin(), path.rend());
        }
        if (extra > 0) frontier.push({n.cost + extra, uint32_t(tie.next()), n.at, true});
        expanded++;
        for (Vec2i d : DIRS8) {
            Vec2i m = n.at + d;
            if (!open(m)) continue;
            bool diagonal = d.x != 0 && d.y != 0;
            if (diagonal && (!open(n.at + Vec2i{d.x, 0}) || !open(n.at + Vec2i{0, d.y}))) continue;
            int c = n.cost + (diagonal ? 14 : 10) + bloodCost(m) + messCost(world, m);
            auto it = cost.find(m);
            if (it != cost.end() && it->second <= c) continue;
            cost[m] = c;
            from[m] = n.at;
            frontier.push({c, uint32_t(tie.next()), m});
        }
    }
    return std::nullopt;
}
