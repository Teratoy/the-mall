#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <string>

// Things put down: cakes and bottles lying on the floor, in plain sight.
// A human kneels to put them down where it stands, and kneels on or beside
// a pile to take what it wants from it: cakes while it has room, bottles
// with water it would drink while it has room, or that water poured into
// its own. Piles count as food and water it knows of.
//
// Stashes (AI/hoarding_plan.md): a hoarder, its pockets full and knowing
// of more cakes than it can carry, takes the load to a stash of its own and
// puts it down there, keeping a cake on it, then goes back for more, until
// the stash is as big as it wants. A stash goes somewhere tucked away that
// nobody it sees is watching, near where it keeps: home, where it sleeps,
// or where it last drank. Far from all of its stashes, it makes a new one.
// It takes from its own stash only what it needs: a cake when hungry with
// none on it, water when thirsty with none to drink.
//
// Whoever sees it stash things knows that pile is its stash, and leaves it
// alone (stealing is human_stash's step 4); a pile nobody is known to own is
// anyone's. Back at its stash, the owner sees what is gone, blames whoever
// it last saw beside it, and moves what is left somewhere new.

namespace {

/// Kneeling to put things down, or to pick them up.
constexpr float STASH_TIME = 1.0f;
/// Its own bottles under this are worth topping up from one lying there.
constexpr float PILE_TOPUP = 0.9f;
/// Poured from a bottle lying there, up to this much at a time.
constexpr float PILE_POUR = 1.0f;
/// The hoard drive, 0 to about 1: what greed, the hoard lesson, being
/// sociable (taken off), being settled and gloom each add at their fullest.
/// From HOARD_FROM on it keeps stashes, wanting up to STASH_CAKES cakes in
/// one at a full drive.
constexpr float HOARD_GREEDY = 0.7f, HOARD_LESSON = 0.5f, HOARD_SOCIABLE = 0.3f;
constexpr float HOARD_SETTLED = 0.2f, HOARD_GLOOM = 0.2f;
constexpr float HOARD_FROM = 0.3f;
constexpr int STASH_CAKES = 8;
/// A stash farther than this is too far to bring a load to: it makes a new
/// one.
constexpr int STASH_RANGE = 30;
/// A new stash goes no farther than this from where it keeps; the more
/// open cells round a spot, the more each costs, in tenths of a cell.
constexpr int STASH_SPOT_REACH = 10;
constexpr int STASH_SHY = 40;
/// Stashing, it keeps this many cakes on it; a spare bottle at least this
/// full is stashed too.
constexpr int STASH_KEEP = 1;
constexpr float STASH_BOTTLE = 0.5f;
/// Robbed, it thinks this much less of whoever it blames; it moves the
/// stash at least this far from where it was.
constexpr float TRUST_STASH_BLAMED = -0.3f;
constexpr int STASH_MOVE = 8;

/// The emptiest of its bottles under PILE_TOPUP, -1 if none.
int emptiestBelow(const std::vector<Bottle>& bottles) {
    int best = -1;
    for (size_t i = 0; i < bottles.size(); i++)
        if (bottles[i].water < PILE_TOPUP && (best < 0 || bottles[i].water < bottles[size_t(best)].water)) best = int(i);
    return best;
}

std::string count(int n, const char* one, const char* many) { return n == 1 ? std::string(one) : std::to_string(n) + many; }

std::string listed(const std::vector<std::string>& parts) {
    std::string s;
    for (size_t i = 0; i < parts.size(); i++) s += (i == 0 ? "" : i + 1 == parts.size() ? " AND " : ", ") + parts[i];
    return s;
}

} // namespace

float Human::hoardDrive() const {
    return std::max(0.0f, HOARD_GREEDY * traits.greedy + HOARD_LESSON * lessons.hoard - HOARD_SOCIABLE * traits.sociable +
                              (escape_.settled ? HOARD_SETTLED : 0.0f) + HOARD_GLOOM * gloom());
}

int Human::stashTarget() const {
    float d = hoardDrive();
    if (d < HOARD_FROM) return 0;
    return 1 + int(std::lround(std::min(1.0f, (d - HOARD_FROM) / (1.0f - HOARD_FROM)) * (STASH_CAKES - 1)));
}

bool Human::isMine(Vec2i c) const { return std::find(stash_.mine.begin(), stash_.mine.end(), c) != stash_.mine.end(); }

bool Human::exposed(Vec2i c) const {
    return std::find(stash_.exposed.begin(), stash_.exposed.end(), c) != stash_.exposed.end();
}

std::optional<Vec2i> Human::nearStash() const {
    std::optional<Vec2i> best;
    for (Vec2i c : stash_.mine)
        if (!exposed(c) && cheb(c, cell) <= STASH_RANGE && (!best || cheb(c, cell) < cheb(*best, cell))) best = c;
    return best;
}

void Human::checkStash(Vec2i c, const Pile* now) {
    auto it = stash_.left.find(c);
    if (it != stash_.left.end()) {
        const Pile& was = it->second;
        int cakesGone = was.cakes - (now ? now->cakes : 0);
        int bottlesGone = int(was.bottles.size()) - (now ? int(now->bottles.size()) : 0);
        if (cakesGone > 0 || bottlesGone > 0) {
            note("SOMEONE TOOK FROM MY STASH");
            auto by = stash_.nearBy.find(c);
            int blamed = by == stash_.nearBy.end() ? -1 : by->second;
            sayLater(talk::Moment::StashRobbed, -1, blamed);
            if (blamed >= 0) {
                note("@ MUST HAVE TAKEN IT", blamed);
                regard(blamed, TRUST_STASH_BLAMED);
            }
            if (now && !exposed(c)) stash_.exposed.push_back(c);
        }
    }
    if (now) {
        stash_.left[c] = *now;
        return;
    }
    // Gone: no stash of its own there any more.
    std::erase(stash_.mine, c);
    std::erase(stash_.exposed, c);
    stash_.left.erase(c);
    stash_.nearBy.erase(c);
}

bool Human::wantsToStash() const {
    int target = stashTarget();
    if (target == 0) return false;
    auto near = nearStash();
    int held = 0;
    if (near)
        if (auto it = memory.piles.find(*near); it != memory.piles.end()) held = it->second.cakes;
    bool spare = bottles.size() >= 2 && std::min(bottles[0].water, bottles[1].water) >= STASH_BOTTLE;
    if (spare) return true;
    // A stash found out, still holding something, to be moved.
    for (Vec2i c : stash_.exposed)
        if (auto it = memory.piles.find(c); it != memory.piles.end() && !it->second.empty()) return true;
    if (held >= target || cakes < carryMax()) return false;
    // More to be had than it can carry.
    if (!memory.cakes.empty()) return true;
    for (const auto& [c, p] : memory.piles)
        if (p.cakes > 0 && !isMine(c) && !othersStash(c)) return true;
    return false;
}

// A nook nobody it sees is watching, as near where it keeps as it can get,
// and away from any stash of its own that was found out. Moving one, it
// fills its pockets from it first.
bool Human::seekStash(World& world) {
    if (cakes < carryMax())
        for (Vec2i c : stash_.exposed) {
            auto it = memory.piles.find(c);
            if (it == memory.piles.end() || it->second.cakes == 0) continue;
            auto p = search(world, [&](Vec2i at) { return cheb(at, c) <= 1; });
            if (!p) continue;
            if (p->empty()) return takeFromPiles(world);
            startWalking(Goal::Pick, *p);
            return true;
        }
    if (auto near = nearStash()) {
        stash_.spot = *near;
    } else {
        Vec2i base = escape_.settled ? escape_.home : sleep_.sleptYet ? sleep_.sleptAt : water_.drankYet ? water_.last : cell;
        auto open = [&](Vec2i c) {
            Tile t = memory.tile(c);
            return t == Tile::Void || passable(t);
        };
        auto p = search(world, [&](Vec2i c) {
            if (cheb(c, base) > STASH_SPOT_REACH || memory.tile(c) != Tile::Floor || avoids(c)) return false;
            if (memory.piles.count(c) || world.hasCake(c) || world.hasKey(c)) return false;
            for (Vec2i e : stash_.exposed)
                if (cheb(c, e) < STASH_MOVE) return false;
            for (Vec2i d : DIRS8) {
                Tile t = memory.tile(c + d);
                if (t == Tile::DoorOpen || t == Tile::DoorLocked) return false;
            }
            return !watchedAt(world, c);
        }, true, [&](Vec2i c) {
            int n = 0;
            for (Vec2i d : DIRS8) n += open(c + d);
            return 1 + n * STASH_SHY;
        });
        if (!p) return false;
        stash_.spot = p->empty() ? cell : p->back();
    }
    if (stash_.spot == cell) return stashHere();
    auto p = search(world, [&](Vec2i c) { return c == stash_.spot; });
    if (!p || p->empty()) return false;
    startWalking(Goal::Stash, *p);
    return true;
}

bool Human::stashHere() {
    if (cell != stash_.spot) return false;
    int n = std::max(0, cakes - STASH_KEEP);
    std::vector<size_t> which;
    if (bottles.size() >= 2) {
        size_t spare = bottles[0].water < bottles[1].water ? 0 : 1;
        if (bottles[spare].water >= STASH_BOTTLE) which.push_back(spare);
    }
    if (n == 0 && which.empty()) return false;
    if (!isMine(cell)) {
        stash_.mine.push_back(cell);
        note("MADE A STASH");
    }
    startPutting(n, std::move(which));
    stash_.stashing = true;
    return true;
}

bool Human::wantsFromPile(Vec2i c, const Pile& p) const {
    if (othersStash(c)) return false;
    bool own = isMine(c);
    // A stash of its own that was found out is emptied, to be moved.
    if (own && exposed(c) && p.cakes > 0 && cakes < carryMax()) return true;
    if (own) {
        if (p.cakes > 0 && hungry() && cakes == 0) return true;
        if (thirsty() && sippable() < 0)
            for (const Bottle& b : p.bottles)
                if (wouldSip(b)) return true;
        return false;
    }
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
        if (!wantsFromPile(c, *p)) continue;
        stash_.at = c;
        stash_.putting = stash_.stashing = false;
        stash_.cakes = 0;
        stash_.bottles.clear();
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
    stash_.at = cell;
    stash_.putting = true;
    stash_.stashing = false;
    stash_.cakes = n;
    stash_.bottles = std::move(which);
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
        if (const Pile* p = world.pile(c)) {
            memory.piles[c] = *p;
            if (isMine(c)) stash_.left[c] = *p;
        }
        // Seen stashing, the pile is known for its stash.
        for (const auto& o : world.humans()) {
            if (!stash_.stashing || o.get() == this || o->dead() || !o->sight.sees(c)) continue;
            if (!o->memory.stashes.emplace(c, id).second) continue;
            o->note("SAW @ STASH THINGS", id);
            o->sayLater(talk::Moment::SawStash, -1, id);
        }
        std::vector<std::string> parts;
        if (n > 0) parts.push_back(count(n, "A CAKE", " CAKES"));
        if (!down.empty()) parts.push_back(count(int(down.size()), "A BOTTLE", " BOTTLES"));
        if (!parts.empty()) {
            note((stash_.stashing ? "STASHED " : "PUT DOWN ") + listed(parts));
            sayLater(stash_.stashing ? talk::Moment::StashIt : talk::Moment::PutDown);
        }
        stash_.putting = stash_.stashing = false;
        return choose(world);
    }
    bool own = isMine(c), moving = own && exposed(c);
    const Pile* p = world.pile(c);
    int gotCakes = 0, gotBottles = 0;
    float poured = 0.0f;
    if (p && wantsFromPile(c, *p)) {
        // From its own stash, a cake at a time, and water only to drink;
        // all it can carry when moving it.
        gotCakes = world.takePileCakes(c, own && !moving ? (hungry() && cakes == 0 ? 1 : 0) : carryMax() - cakes);
        cakes += gotCakes;
        bool forWater = !own || (thirsty() && sippable() < 0);
        // Bottles while there is room, the fullest it would drink first; one
        // with none takes any.
        while (forWater && (p = world.pile(c)) && bottles.size() < size_t(BOTTLE_MAX)) {
            int best = -1;
            for (size_t i = 0; i < p->bottles.size(); i++) {
                const Bottle& b = p->bottles[i];
                if ((wouldSip(b) || (bottles.empty() && !own)) && (best < 0 || b.water > p->bottles[size_t(best)].water))
                    best = int(i);
            }
            if (best < 0) break;
            bottles.push_back(world.takePileBottle(c, size_t(best)));
            gotBottles++;
            if (own) break;
        }
        // Then its own topped up from what is left.
        for (size_t i = 0; forWater && gotBottles == 0 && (p = world.pile(c)) && i < p->bottles.size(); i++) {
            int mine = emptiestBelow(bottles);
            if (mine < 0) break;
            if (wouldSip(p->bottles[i])) poured += world.pourFromPile(c, i, bottles[size_t(mine)], PILE_POUR);
        }
    }
    p = world.pile(c);
    if (p)
        memory.piles[c] = *p;
    else
        memory.piles.erase(c);
    // What it took itself is no theft: it remembers the stash as it left it.
    if (own && p) {
        stash_.left[c] = *p;
    } else if (own) {
        std::erase(stash_.mine, c);
        std::erase(stash_.exposed, c);
        stash_.left.erase(c);
        stash_.nearBy.erase(c);
    }
    std::vector<std::string> parts;
    if (gotCakes > 0) parts.push_back(count(gotCakes, "A CAKE", " CAKES"));
    if (gotBottles > 0) parts.push_back(count(gotBottles, "A BOTTLE", " BOTTLES"));
    if (!parts.empty()) {
        note(moving ? "TOOK " + listed(parts) + " FROM MY STASH TO MOVE IT"
             : own  ? "TOOK " + listed(parts) + " FROM MY STASH"
                    : "PICKED UP " + listed(parts) + " LYING THERE");
        sayLater(own ? talk::Moment::FromStash : talk::Moment::PickUp);
    }
    if (poured > 0.0f) note(own ? "FILLED MY BOTTLE FROM MY STASH" : "FILLED MY BOTTLE FROM ONE LYING THERE");
    if ((gotCakes > 0 && hungry()) || ((gotBottles > 0 || poured > 0.0f) && thirsty())) rethink_ = true;
    choose(world);
}

std::optional<std::vector<Vec2i>> Human::pilePath(const World& world, bool food, bool water) const {
    std::vector<Vec2i> worth;
    for (const auto& [c, p] : memory.piles) {
        bool own = isMine(c);
        if (othersStash(c) || (own && !wantsFromPile(c, p))) continue;
        bool cakesThere = food && p.cakes > 0 && (own || cakes < carryMax());
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
