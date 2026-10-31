// Vending machines: seeing them and what they hold, and buying a cake or an
// empty bottle for keys. Keys bought with are gone, so a cake from a
// machine costs a way out (user).
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>

namespace {

/// Seconds it takes to feed the keys in, press, and take what drops.
constexpr float VEND_TIME = 2.0f;
/// Starving, a machine wins over food this much nearer than it.
constexpr float VEND_STARVING_REACH = 1.5f;
/// From this curious on, it keeps its keys for doors unless starving or
/// knowing of no other food.
constexpr float VEND_KEEP_CURIOUS = 0.7f;
/// From this greedy on, it buys cakes to hoard, hungry or not.
constexpr float VEND_HOARD_GREEDY = 0.75f;

} // namespace

std::optional<std::vector<Vec2i>> Human::machinePath(const World& world, Vend kind) const {
    std::vector<const Machine*> stocked;
    for (const auto& [at, seen] : memory.machines)
        if (seen.kind == kind && seen.stock > 0 && !avoids(at))
            if (const Machine* m = world.machine(at)) stocked.push_back(m);
    if (stocked.empty()) return std::nullopt;
    return search(world, [&](Vec2i c) {
        return std::any_of(stocked.begin(), stocked.end(), [&](const Machine* m) { return m->usedFrom(c); });
    });
}

std::optional<std::vector<Vec2i>> Human::buyCakePath(const World& world, size_t nearest) const {
    if (!hungry() || !canBuy() || cakes >= carryMax()) return std::nullopt;
    if (traits.curious >= VEND_KEEP_CURIOUS && !starving() && nearest != SIZE_MAX) return std::nullopt;
    auto v = machinePath(world, Vend::Cake);
    if (!v) return std::nullopt;
    float reach = starving() ? VEND_STARVING_REACH : 1.0f;
    if (nearest != SIZE_MAX && float(v->size()) >= float(nearest) * reach) return std::nullopt;
    return v;
}

bool Human::hoardBuys() const {
    return traits.greedy >= VEND_HOARD_GREEDY && canBuy() && cakes < carryMax() &&
           std::any_of(memory.machines.begin(), memory.machines.end(),
                       [](const auto& m) { return m.second.kind == Vend::Cake && m.second.stock > 0; });
}

bool Human::buyBottle(World& world) {
    if (!needsBottle() || !canBuy()) return false;
    auto v = machinePath(world, Vend::Bottle);
    return v && goBuy(world, *v, Vend::Bottle);
}

bool Human::goBuy(World& world, const std::vector<Vec2i>& path, Vend kind) {
    vend_.kind = kind;
    if (path.empty()) return startVending(world);
    vend_.at = path.back();
    for (Vec2i d : DIRS4)
        if (const Machine* m = world.machine(path.back() + d); m && m->usedFrom(path.back())) vend_.at = m->at;
    startWalking(Goal::Vend, path);
    return true;
}

// The machine is picked from those beside it as it remembers them; what is
// really in it is seen when it gets there.
bool Human::startVending(World& world) {
    if (!canBuy()) return false;
    for (Vec2i d : DIRS4) {
        if (memory.tile(cell + d) != Tile::Vending) continue;
        const Machine* m = world.machine(cell + d);
        if (!m || !m->usedFrom(cell) || m->kind != vend_.kind) continue;
        auto it = memory.machines.find(m->at);
        if (it == memory.machines.end() || it->second.stock <= 0) continue;
        if (vend_.kind == Vend::Cake ? cakes >= carryMax() : !bottles.empty()) return false;
        vend_.at = m->at;
        state = State::Vending;
        goal = Goal::Vend;
        moving = false;
        path_.clear();
        animTime = 0.0f;
        timer = VEND_TIME;
        if (d.x != 0) facingLeft = d.x < 0;
        return true;
    }
    return false;
}

void Human::takeVended(World& world) {
    observe(world);
    const Machine* m = world.machine(vend_.at);
    bool room = vend_.kind == Vend::Cake ? cakes < carryMax() : bottles.empty();
    if (!m || !canBuy() || !room) return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    if (!world.vend(vend_.at)) {
        note("THE MACHINE WAS EMPTY");
        sayLater(talk::Moment::VendEmpty);
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    }
    keys -= MACHINE_PRICE;
    memory.machines[m->at] = {m->kind, m->stock};
    if (m->kind == Vend::Cake) {
        cakes++;
        note("PAID 3 KEYS FOR A CAKE FROM A MACHINE");
        sayLater(talk::Moment::VendCake);
    } else {
        bottles.push_back(Bottle{});
        note("PAID 3 KEYS FOR A BOTTLE FROM A MACHINE");
        sayLater(talk::Moment::VendBottle);
    }
    startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
}

// A machine is lit and see-through: what it sells and how much is left
// shows from as far as it can be seen. News to the hungry with keys to
// spend, the first time.
bool Human::seeMachine(const Machine& m, Vec2i c) {
    checkTold(c, true);
    bool fresh = memory.machines.emplace(m.at, Memory::SeenMachine{m.kind, m.stock}).second;
    memory.machines[m.at] = {m.kind, m.stock};
    if (!vend_.seen && state != State::Waking) {
        vend_.seen = true;
        note("FOUND A MACHINE THAT TAKES KEYS");
        if (!discover({talk::Moment::FirstVending, c})) sayLater(talk::Moment::FirstVending);
    }
    return fresh && m.stock > 0 && canBuy() && (m.kind == Vend::Cake ? hungry() : needsBottle());
}
