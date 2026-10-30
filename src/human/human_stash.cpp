#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <string>

// Things put down: cakes and bottles lying on the floor, in plain sight.
// A human kneels to put them down where it stands, and kneels on or beside
// a pile to take what it wants from it: cakes while it has room, bottles
// with water it would drink while it has room, or that water poured into
// its own. Piles count as food and water it knows of. Who put them there,
// and why, is for stashes (AI/hoarding_plan.md).

namespace {

/// Kneeling to put things down, or to pick them up.
constexpr float STASH_TIME = 1.0f;
/// Its own bottles under this are worth topping up from one lying there.
constexpr float PILE_TOPUP = 0.9f;
/// Poured from a bottle lying there, up to this much at a time.
constexpr float PILE_POUR = 1.0f;

/// The emptiest of its bottles under PILE_TOPUP, -1 if none.
int emptiestBelow(const std::vector<Bottle>& bottles) {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++)
        if (bottles[i].water < PILE_TOPUP && (best < 0 || bottles[i].water < bottles[size_t(best)].water)) best = int(i);
    return best;
}

std::string count(int n, const char* one, const char* many) { return n == 1 ? std::string(one) : std::to_string(n) + many; }

} // namespace

bool Human::wantsFromPile(const Pile& p) const {
    if (p.cakes > 0 && cakes < carryMax()) return true;
    for (const Bottle& b : p.bottles) {
        if (bottles.empty()) return true;
        if (!wouldSip(b)) continue;
        if (bottles.size() < size_t(BOTTLE_MAX)) return true;
        if (emptiestBelow(bottles) >= 0) return true;
    }
    return false;
}

// Seen from where it stands, so it goes by the pile as it truly is.
bool Human::takeFromPiles(World& world) {
    for (Vec2i d : REACH) {
        Vec2i c = cell + d;
        const Pile* p = world.pile(c);
        if (!p) {
            memory.piles.erase(c);
            continue;
        }
        memory.piles[c] = *p;
        if (!wantsFromPile(*p)) continue;
        stash_ = {c, false, 0, {}};
        state = State::Stashing;
        moving = false;
        path_.clear();
        animTime = 0.0f;
        if (c.x != cell.x) facingLeft = c.x < cell.x;
        timer = STASH_TIME;
        return true;
    }
    return false;
}

void Human::startPutting(int n, std::vector<size_t> which) {
    stash_ = {cell, true, n, std::move(which)};
    state = State::Stashing;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    timer = STASH_TIME;
}

// What it has may have changed while it knelt: it puts down only what it
// still has, and takes only what is still there.
void Human::finishStash(World& world) {
    Vec2i c = stash_.at;
    if (stash_.putting) {
        int n = std::clamp(stash_.cakes, 0, cakes);
        std::vector<Bottle> down;
        std::sort(stash_.bottles.rbegin(), stash_.bottles.rend());
        for (size_t i : stash_.bottles)
            if (i < bottles.size()) {
                down.push_back(bottles[i]);
                bottles.erase(bottles.begin() + long(i));
            }
        cakes -= n;
        world.putDown(c, n, down);
        if (const Pile* p = world.pile(c)) memory.piles[c] = *p;
        std::vector<std::string> parts;
        if (n > 0) parts.push_back(count(n, "A CAKE", " CAKES"));
        if (!down.empty()) parts.push_back(count(int(down.size()), "A BOTTLE", " BOTTLES"));
        if (!parts.empty()) {
            note("PUT DOWN " + parts[0] + (parts.size() > 1 ? " AND " + parts[1] : ""));
            sayLater(talk::Moment::PutDown);
        }
        stash_ = {};
        return choose(world);
    }
    const Pile* p = world.pile(c);
    int gotCakes = 0, gotBottles = 0;
    float poured = 0.0f;
    if (p) {
        gotCakes = world.takePileCakes(c, carryMax() - cakes);
        cakes += gotCakes;
        // Bottles while there is room, the fullest it would drink first; one
        // with none takes any.
        while ((p = world.pile(c)) && bottles.size() < size_t(BOTTLE_MAX)) {
            int best = -1;
            for (size_t i = 0; i < p->bottles.size(); i++) {
                const Bottle& b = p->bottles[i];
                if ((wouldSip(b) || bottles.empty()) && (best < 0 || b.water > p->bottles[size_t(best)].water))
                    best = int(i);
            }
            if (best < 0) break;
            bottles.push_back(world.takePileBottle(c, size_t(best)));
            gotBottles++;
        }
        // Then its own topped up from what is left.
        for (size_t i = 0; (p = world.pile(c)) && i < p->bottles.size(); i++) {
            int mine = emptiestBelow(bottles);
            if (mine < 0) break;
            if (wouldSip(p->bottles[i])) poured += world.pourFromPile(c, i, bottles[size_t(mine)], PILE_POUR);
        }
    }
    if (p)
        memory.piles[c] = *p;
    else
        memory.piles.erase(c);
    std::vector<std::string> parts;
    if (gotCakes > 0) parts.push_back(count(gotCakes, "A CAKE", " CAKES"));
    if (gotBottles > 0) parts.push_back(count(gotBottles, "A BOTTLE", " BOTTLES"));
    if (!parts.empty()) {
        note("PICKED UP " + parts[0] + (parts.size() > 1 ? " AND " + parts[1] : "") + " LYING THERE");
        sayLater(talk::Moment::PickUp);
    }
    if (poured > 0.0f) note("FILLED MY BOTTLE FROM ONE LYING THERE");
    if ((gotCakes > 0 && hungry()) || ((gotBottles > 0 || poured > 0.0f) && thirsty())) rethink_ = true;
    stash_ = {};
    choose(world);
}

std::optional<std::vector<Vec2i>> Human::pilePath(const World& world, bool food, bool water) const {
    std::vector<Vec2i> worth;
    for (const auto& [c, p] : memory.piles) {
        bool cakesThere = food && p.cakes > 0 && cakes < carryMax();
        bool waterThere = false;
        for (const Bottle& b : p.bottles) waterThere |= water && wouldSip(b);
        if ((cakesThere || waterThere) && !avoids(c)) worth.push_back(c);
    }
    if (worth.empty()) return std::nullopt;
    return search(world, [&](Vec2i c) {
        for (Vec2i at : worth)
            if (cheb(c, at) <= 1) return true;
        return false;
    });
}
