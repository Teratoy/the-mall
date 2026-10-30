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
// Whoever sees it stash things knows that pile is its stash; a pile nobody
// is known to own is anyone's. Back at its stash, the owner sees what is
// gone, blames whoever it last saw beside it, and moves what is left
// somewhere new. Someone else's stash is left alone unless it needs what is
// in it badly, or is greedy and bold enough: then it steals, out of the
// owner's sight unless desperate, and anyone who sees it thinks less of it;
// an owner who catches it goes for it. The hungry with nothing resent a big
// stash they see, and think less of whoever keeps it.

namespace {

/// Kneeling to put things down, or to pick them up.
constexpr float STASH_TIME = 1.0f;
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
/// Stashing, it keeps this many cakes on it, and its bottle.
constexpr int STASH_KEEP = 1;
/// A pile looks as big as it can get with this much water in it, or
/// STASH_CAKES cakes.
constexpr float STASH_WATER = 2.0f;
/// Robbed, it thinks this much less of whoever it blames; it moves the
/// stash at least this far from where it was.
constexpr float TRUST_STASH_BLAMED = -0.3f;
constexpr int STASH_MOVE = 8;
/// Stealing: it steals once need (STEAL_NEED, more when starving or
/// parched), greed and boldness, less guilt and its trust in the owner,
/// come to STEAL_BAR. Seen by the owner, the thief is thought this much
/// worse of; by anyone else, the other.
constexpr float STEAL_NEED = 0.5f, STEAL_DESPERATE = 0.4f;
constexpr float STEAL_GREEDY = 0.5f, STEAL_BRAVE = 0.3f, STEAL_GUILT = 0.5f, STEAL_TRUST = 0.5f;
constexpr float STEAL_BAR = 0.6f;
constexpr float TRUST_STOLEN = -0.5f, TRUST_SAW_STEAL = -0.3f;
/// Overheard telling of its stash, the owner thinks this much less of the
/// teller. Asked about a stash, a hoarder this greedy (or cunning) denies
/// it to all but friends; seen through, the asker thinks this much less of
/// it. Each owner is asked about it once in ASK_STASH_AGAIN.
constexpr float TRUST_STASH_TOLD = -0.3f;
constexpr float DENY_GREEDY = 0.6f, TRUST_DENIED = -0.3f;
constexpr float ASK_STASH_AGAIN = 120.0f;
/// Resentment: a stash of at least RESENT_SIZE cakes seen while hungry with
/// none (or thirsty with nothing to drink, for water) and no stash of its
/// own costs the owner up to RESENT_TRUST, by how big and how needy; once
/// in RESENT_AGAIN per owner.
constexpr int RESENT_SIZE = 3;
constexpr float RESENT_TRUST = -0.2f, RESENT_AGAIN = 60.0f;

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
    if (n == 0) return false;
    if (!isMine(cell)) {
        stash_.mine.push_back(cell);
        note("MADE A STASH");
    }
    startPutting(n, {});
    stash_.stashing = true;
    return true;
}

bool Human::wouldSteal(const World& world, Vec2i c, const Pile& p) const {
    int owner = memory.stashes.at(c);
    bool forFood = p.cakes > 0 && cakes < carryMax();
    bool needFood = forFood && hungry() && cakes == 0;
    bool needWater = false, forWater = false;
    for (const Bottle& b : p.bottles)
        if (wouldSip(b) && wouldTakeBottle(b)) {
            forWater = true;
            needWater |= thirsty() && sippable() < 0;
        }
    if (!forFood && !forWater) return false;
    bool desperate = (needFood && starving()) || (needWater && parched());
    // Whose it is may not be known: someone's.
    const Human* o = owner >= 0 ? &world.human(owner) : nullptr;
    if (o && !o->dead() && !desperate) {
        if (isFriend(owner) || inGroupWith(*o)) return false;
        // Not under the owner's eyes.
        if (sight.sees(o->cell) && o->sight.sees(c)) return false;
    }
    float urge = (needFood || needWater ? STEAL_NEED : 0.0f) + (desperate ? STEAL_DESPERATE : 0.0f) +
                 STEAL_GREEDY * traits.greedy + STEAL_BRAVE * traits.brave - STEAL_GUILT * mind_.guilt -
                 STEAL_TRUST * trust(owner);
    return urge >= STEAL_BAR;
}

void Human::stoleFrom(World& world, Vec2i c, int owner, const std::string& what) {
    if (owner < 0) {
        note("TOOK " + what + " FROM SOMEONE'S STASH");
        sayLater(talk::Moment::StealStash);
        return;
    }
    note("TOOK " + what + " FROM @'S STASH", owner);
    sayLater(talk::Moment::StealStash, -1, owner);
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->state == State::Sleeping || !o->sight.sees(cell)) continue;
        if (o->id == owner) {
            // Caught at it: no need to wonder who, later.
            if (const Pile* p = world.pile(c)) o->stash_.left[c] = *p;
            o->note("@ TOOK FROM MY STASH", id);
            o->regard(id, TRUST_STOLEN);
            o->sayLater(talk::Moment::CaughtStealing, id);
            o->fight_.foe = id;
            o->fight_.grudge = o->grudgeFor(id);
            o->rethink_ = true;
            continue;
        }
        if (o->isFoe(owner)) continue;
        o->note("SAW @ STEAL FROM #'S STASH", id, owner);
        o->regard(id, TRUST_SAW_STEAL);
    }
}

void Human::resent(Vec2i, const Pile& p, int owner) {
    if (owner < 0 || owner == id || !stash_.mine.empty()) return;
    bool food = hungry() && cakes == 0 && p.cakes >= RESENT_SIZE;
    bool water = thirsty() && sippable() < 0 && p.water() >= 1.0f;
    if (!food && !water) return;
    if (auto it = stash_.resented.find(owner); it != stash_.resented.end() && clock_ - it->second < RESENT_AGAIN) return;
    stash_.resented[owner] = clock_;
    float need = std::max(food ? hunger : 0.0f, water ? thirst : 0.0f);
    float size = std::min(1.0f, float(p.cakes) / float(STASH_CAKES) + p.water() / STASH_WATER);
    regard(owner, RESENT_TRUST * need * (0.5f + 0.5f * size));
    note("@ HOARDS WHILE I GO WITHOUT", owner);
    sayLater(talk::Moment::Resent, -1, owner);
}

bool Human::wantsFromPile(const World& world, Vec2i c, const Pile& p) const {
    if (othersStash(c)) return wouldSteal(world, c, p);
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
    for (const Bottle& b : p.bottles)
        if (wouldTakeBottle(b) && (bottles.empty() || wouldSip(b))) return true;
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
        if (!wantsFromPile(world, c, *p)) continue;
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
        if (stash_.stashing) seenAtStash(world, c, true);
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
    int owner = othersStash(c) ? memory.stashes.at(c) : -1;
    const Pile* p = world.pile(c);
    int gotCakes = 0, gotBottles = 0;
    float poured = 0.0f;
    if (p && wantsFromPile(world, c, *p)) {
        // From its own stash, a cake at a time, and water only to drink;
        // all it can carry when moving it.
        gotCakes = world.takePileCakes(c, own && !moving ? (hungry() && cakes == 0 ? 1 : 0) : carryMax() - cakes);
        cakes += gotCakes;
        bool forWater = !own || (thirsty() && sippable() < 0);
        // The fullest bottle it would drink, one with none taking any; holding
        // one, the two poured together into the fuller, which it keeps.
        int best = -1;
        for (size_t i = 0; forWater && (p = world.pile(c)) && i < p->bottles.size(); i++) {
            const Bottle& b = p->bottles[i];
            bool fits = wouldTakeBottle(b) && (wouldSip(b) || (bottles.empty() && !own));
            if (fits && (best < 0 || b.water > p->bottles[size_t(best)].water)) best = int(i);
        }
        if (best >= 0) {
            bool keptOwn = false;
            float was = bottleWater();
            if (auto left = takeBottle(world.takePileBottle(c, size_t(best)), &keptOwn)) world.putDown(c, 0, {*left});
            if (keptOwn)
                poured = bottleWater() - was;
            else
                gotBottles = 1;
        }
    }
    p = world.pile(c);
    if (p)
        memory.piles[c] = *p;
    else
        memory.piles.erase(c);
    if (own && (gotCakes > 0 || gotBottles > 0 || poured > 0.0f)) seenAtStash(world, c, false);
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
    if (!parts.empty() && owner >= 0) {
        stoleFrom(world, c, owner, listed(parts));
    } else if (!parts.empty()) {
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
        if ((own || othersStash(c)) && !wantsFromPile(world, c, p)) continue;
        bool cakesThere = food && p.cakes > 0 && (own || cakes < carryMax());
        bool waterThere = false;
        for (const Bottle& b : p.bottles) waterThere |= water && wouldSip(b) && wouldTakeBottle(b);
        if ((cakesThere || waterThere) && !avoids(c)) worth.push_back(c);
    }
    if (worth.empty()) return std::nullopt;
    return search(world, [&](Vec2i c) {
        for (Vec2i at : worth)
            if (cheb(c, at) <= 1) return true;
        return false;
    });
}

// Seen at it is as good as seen making it: nobody can talk them out of it.
void Human::seenAtStash(World& world, Vec2i c, bool stashing) {
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->state == State::Sleeping || !o->sight.sees(c)) continue;
        o->memory.stashSeenMade.insert(c);
        o->memory.stashTips.erase(c);
        if (!o->memory.stashes.emplace(c, id).second) continue;
        o->note(stashing ? "SAW @ STASH THINGS" : "SAW @ TAKE FROM THEIR STASH", id);
        o->sayLater(talk::Moment::SawStash, -1, id);
        if (const Pile* p = world.pile(c)) o->resent(c, *p, id);
    }
}

// Never its own, nor a friend's; only what o doesn't know of, and only a
// stash it last saw with something in it.
std::optional<Human::Tell> Human::stashNewsFor(const Human& o, bool& forPay) const {
    forPay = false;
    if (isFoe(o.id)) return std::nullopt;
    bool close = isFriend(o.id) || inGroupWith(o);
    bool selling = !close && traits.greedy >= CUNNING_GREEDY;
    if (selling && o.cakes <= o.spareFrom()) return std::nullopt;
    if (!close && !selling && !(o.hungry() && o.cakes == 0)) return std::nullopt;
    std::optional<std::pair<Vec2i, int>> best;
    for (const auto& [c, owner] : memory.stashes) {
        if (owner < 0 || owner == id || owner == o.id || isFriend(owner) || o.memory.stashes.count(c)) continue;
        auto it = memory.piles.find(c);
        if (it == memory.piles.end() || it->second.empty()) continue;
        if (!best || cheb(c, o.cell) < cheb(best->first, o.cell) ||
            (cheb(c, o.cell) == cheb(best->first, o.cell) && (c.x < best->first.x || (c.x == best->first.x && c.y < best->first.y))))
            best = {c, owner};
    }
    if (!best) return std::nullopt;
    Tell t{Tell::Kind::Stash, best->first};
    t.owner = best->second;
    forPay = selling;
    return t;
}

// Only to the hungry with nothing: a real stash of someone it dislikes, to
// set the two on each other; else, beside a table far off the foe never
// saw, a stash that isn't there.
std::optional<Human::Tell> Human::stashLieFor(const World& world, const Human& foe) const {
    if (!cunning() || !isFoe(foe.id) || !foe.hungry() || foe.cakes > 0) return std::nullopt;
    std::optional<std::pair<Vec2i, int>> rival;
    for (const auto& [c, owner] : memory.stashes) {
        if (owner < 0 || owner == id || owner == foe.id || trust(owner) >= 0.0f || foe.memory.stashes.count(c)) continue;
        auto it = memory.piles.find(c);
        if (it == memory.piles.end() || it->second.cakes == 0) continue;
        if (!rival || trust(owner) < trust(rival->second)) rival = {c, owner};
    }
    if (rival) {
        Tell t{Tell::Kind::Stash, rival->first};
        t.owner = rival->second;
        t.lie = true;
        return t;
    }
    std::optional<Vec2i> best;
    for (const Table& tb : world.tables()) {
        if (memory.tile(tb.at) != Tile::Table || cheb(tb.at, foe.cell) < FALSE_TIP_FAR) continue;
        for (Vec2i d : DIRS4) {
            Vec2i c = tb.at + d;
            if (memory.tile(c) != Tile::Floor || foe.memory.tile(c) != Tile::Void || memory.piles.count(c)) continue;
            if (!best || cheb(c, foe.cell) < cheb(*best, foe.cell)) best = c;
        }
    }
    if (!best) return std::nullopt;
    Tell t{Tell::Kind::Stash, *best};
    t.lie = t.nothing = true;
    return t;
}

void Human::stashToldOf(World& world, const Tell& t, Human& to) {
    Human& owner = world.human(t.owner);
    if (&owner == this || &owner == &to || owner.dead() || owner.state == State::Sleeping) return;
    if (cheb(owner.cell, cell) > HEAR_RADIUS || !owner.sight.sees(cell) || !owner.isMine(t.cell)) return;
    if (!owner.exposed(t.cell)) owner.stash_.exposed.push_back(t.cell);
    owner.note("@ TOLD # ABOUT MY STASH", id, to.id);
    owner.regard(id, TRUST_STASH_TOLD);
    owner.sayLater(talk::Moment::StashTold, id);
}

void Human::askAboutStash(Human& o) {
    if (!(hungry() && cakes == 0) && trust(o.id) >= 0.0f) return;
    if (auto it = stash_.askedAbout.find(o.id); it != stash_.askedAbout.end() && clock_ - it->second < ASK_STASH_AGAIN) return;
    std::optional<Vec2i> best;
    for (const auto& [c, owner] : memory.stashes)
        if (owner == o.id && (!best || cheb(c, cell) < cheb(*best, cell) ||
                              (cheb(c, cell) == cheb(*best, cell) && (c.x < best->x || (c.x == best->x && c.y < best->y)))))
            best = c;
    if (!best) return;
    Vec2i c = *best;
    stash_.askedAbout[o.id] = clock_;
    sayLater(talk::Moment::AskStash, o.id);
    note("ASKED @ ABOUT THEIR STASH", o.id);
    bool deny = o.hoardDrive() >= HOARD_FROM && (o.cunning() || o.traits.greedy >= DENY_GREEDY) && !o.isFriend(id);
    if (!deny) {
        o.note("TOLD @ THE STASH IS MINE", id);
        o.sayLater(talk::Moment::OwnStash, id);
        return;
    }
    o.note("TOLD @ THE STASH ISN'T MINE", id);
    o.sayLater(talk::Moment::DenyStash, id);
    if (memory.stashSeenMade.count(c)) {
        note("@ DENIED THEIR STASH. I SAW THEM MAKE IT", o.id);
        regard(o.id, TRUST_DENIED);
        sayLater(talk::Moment::CaughtDenying, o.id);
        return;
    }
    // Believed: not theirs, so anyone's.
    note("@ SAYS THE STASH ISN'T THEIRS", o.id);
    memory.stashes.erase(c);
}
