// Set-up situations for what random soaks seldom reach: each scenario
// places a few humans in a fresh spawn room, sets them up (traits by seed,
// needs, trust, memory), lets the world run, and checks that the thing
// happened, by the lines it leaves in their logs.
// Usage: build/scenarios [name]   (make scenarios; part of make check)
//   With a name, runs only that scenario and prints every log line.
//   LOGLINES=path writes every distinct log line written, as the soak does,
//   for tools/coverage.py.
// Exits non-zero if any scenario fails. Deterministic, like the soak.
#include "human/human_tuning.h"
#include "probe.h"
#include "human/human_tuning.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

using P = SoakProbe;

namespace {

/// The first seed from from on whose traits pass: traits are rolled from
/// the seed, so this is how a scenario picks what a human is like.
uint64_t seedWhere(const std::function<bool(const Traits&)>& like, uint64_t from = 1) {
    for (uint64_t s = from;; s++)
        if (like(Traits::random(s))) return s;
}

/// A fresh world with its spawn room, and room to place more humans. Unless
/// it is set as in play (asInPlay), nothing lies on the floor and everyone
/// is handed a full bottle, as they all woke with one before bottles were
/// found lying.
struct Stage {
    World world;
    Rng rng;
    std::vector<Human*> humans;
    bool asInPlay;

    explicit Stage(uint64_t seed, uint64_t first, bool asInPlay) : world(seed), rng(seed), asInPlay(asInPlay) {
        humans.push_back(&world.makeSpawn(HumanLook::random(rng), first));
        if (asInPlay) return;
        P::clearPiles(world, *humans[0]);
        humans[0]->bottles = {Bottle{1.0f}};
    }
    Human& first() { return *humans[0]; }
    const Human& first() const { return *humans[0]; }

    /// Free floor nearest the first human, at least gap cells from it.
    Vec2i freeNear(Vec2i from, int gap) const {
        for (int r = gap; r < 30; r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) {
                    if (std::max(std::abs(x), std::abs(y)) != r) continue;
                    Vec2i c = from + Vec2i{x, y};
                    if (world.tile(c) == Tile::Floor && !world.occupant(c) && !world.table(c) && !world.statue(c) &&
                        !world.hasKey(c) && !world.hasCake(c))
                        return c;
                }
        return from;
    }
    /// The free floor nearest the first human at least min cells from c.
    Vec2i farFrom(Vec2i c, int min) const {
        for (int r = 0; r < 60; r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) {
                    if (std::max(std::abs(x), std::abs(y)) != r) continue;
                    Vec2i at = first().cell + Vec2i{x, y};
                    if (cheb(at, c) >= min && world.tile(at) == Tile::Floor && !world.occupant(at) &&
                        !world.table(at) && !world.statue(at))
                        return at;
                }
        return first().cell;
    }
    Human& add(uint64_t seed, int gap = 2) {
        Human& h = P::addHuman(world, HumanLook::random(rng), freeNear(first().cell, gap), seed);
        if (!asInPlay) h.bottles = {Bottle{1.0f}};
        humans.push_back(&h);
        return h;
    }
    /// Sets a basin a few cells from the first human, as the spawn room
    /// once had, and gives its top left cell; none if there is no room.
    std::optional<Vec2i> basin() {
        for (Vec2i d : {Vec2i{5, 0}, Vec2i{-6, 0}, Vec2i{0, 5}, Vec2i{0, -6}, Vec2i{5, 5}, Vec2i{-6, -6}})
            if (P::placeBasin(world, first().cell + d)) return first().cell + d;
        return std::nullopt;
    }
    void run(float seconds) {
        run(seconds, [] {});
    }
    /// Runs, calling each() after every step.
    template <typename F> void run(float seconds, F each) {
        const float dt = 1.0f / 30.0f;
        for (int f = 0; f < int(seconds / dt); f++) {
            world.update(dt);
            each();
        }
    }
    /// Someone's log has this line.
    bool logged(const std::string& line) const {
        for (const auto& h : world.humans())
            for (const auto& e : h->log.entries())
                if (e.what == line) return true;
        return false;
    }
};

/// Something a scenario expects to have happened.
struct Check {
    std::string label;
    std::function<bool(const Stage&)> ok;
};
/// Any one of these lines in someone's log.
Check logged(std::vector<std::string> anyOf) {
    std::string label = anyOf.front();
    return {label, [anyOf](const Stage& s) {
                for (const auto& l : anyOf)
                    if (s.logged(l)) return true;
                return false;
            }};
}
/// None of these lines in anyone's log.
Check unlogged(std::string line) {
    return {"no " + line, [line](const Stage& s) { return !s.logged(line); }};
}
/// The first line, then nothing of the second for at least gap seconds.
Check heldOff(std::string first, std::string then, float gap) {
    return {first + ", then " + then + " no sooner than " + std::to_string(int(gap)) + " s on",
            [=](const Stage& s) {
                float at = -1.0f, later = 1e9f;
                for (const auto& h : s.world.humans())
                    for (const auto& e : h->log.entries()) {
                        if (e.what == first && at < 0.0f) at = e.time;
                        if (e.what == then) later = std::min(later, e.time);
                    }
                return at >= 0.0f && later - at >= gap;
            }};
}
using Checks = std::vector<Check>;
/// This line in h's log, or (said false) not in it.
Check loggedBy(const Human& h, const std::string& line, bool said = true) {
    return {(said ? "#" : "no #") + std::to_string(h.id) + " " + line, [&h, line, said](const Stage&) {
                for (const auto& e : h.log.entries())
                    if (e.what == line) return said;
                return !said;
            }};
}

/// h remarked on the room it woke in this way (a fog remark).
Check remarked(const Human& h, talk::Moment m) {
    return {std::string("#") + std::to_string(h.id) + " remarked " + std::to_string(int(m) - int(talk::Moment::FirstStatue)),
            [&h, m](const Stage&) { return (P::fogSights(h) >> (int(m) - int(talk::Moment::FirstStatue))) & 1u; }};
}

/// h said something just before (after false) or within 4 s after this line in its log.
Check saidAround(const Human& h, const std::string& line, bool after) {
    return {std::string("#") + std::to_string(h.id) + " said something " + (after ? "after " : "before ") + line,
            [&h, line, after](const Stage&) {
                const auto& es = h.log.entries();
                for (size_t i = 0; i < es.size(); i++) {
                    if (es[i].what != line) continue;
                    if (!after) return i > 0 && es[i - 1].what.rfind("SAID ", 0) == 0;
                    for (size_t j = i + 1; j < es.size() && es[j].time - es[i].time <= 4.0f; j++)
                        if (es[j].what.rfind("SAID ", 0) == 0) return true;
                    return false;
                }
                return false;
            }};
}

struct Scenario {
    const char* name;
    const char* what;
    /// The seed of the human the world is made with (its traits).
    uint64_t first;
    /// Sets the stage, runs it, and says what should have happened.
    std::function<Checks(Stage&)> play;
    /// Known not to happen yet, and why: reported, but doesn't fail the run.
    const char* known = nullptr;
    /// Starts as in play: a bottle lying near the first human, none in hand.
    bool asInPlay = false;
};

// Traits a scenario asks for. Seeds are searched from different starts so
// two humans with the same wish aren't the same human.
bool cunning(const Traits& t) { return t.greedy >= 0.55f && t.brave >= 0.5f; }
bool kindly(const Traits& t) { return t.sociable >= 0.55f && t.greedy <= 0.45f && t.brave >= 0.45f; }
/// Keeps its place against others (Human::territorial), and how it lets
/// a thirsty stranger at its water: free, for a cake, or not at all.
bool territorial(const Traits& t) { return 0.5f * t.brave + 0.5f * t.greedy - 0.4f * t.sociable >= 0.35f; }
bool guardsFree(const Traits& t) { return territorial(t) && t.sociable >= 0.5f; }
bool guardsForCake(const Traits& t) { return territorial(t) && t.sociable < 0.4f && t.greedy >= 0.55f; }
bool guardsAll(const Traits& t) { return territorial(t) && t.sociable < 0.4f && t.greedy < 0.45f; }
bool plain(const Traits&) { return true; }
// Which way round blood and a body are put together (AI/blame_plan.md).
bool blamesBefore(const Traits& t) { return t.brave < 0.3f && t.curious < 0.6f; }
bool blamesAfter(const Traits& t) { return t.brave >= 0.6f && t.curious < 0.6f; }

// How much shame a human feels (Human::modesty: sociable and timid).
bool modest(const Traits& t) { return (t.sociable + 1.0f - t.brave) / 2.0f >= 0.75f; }
bool shameless(const Traits& t) { return (t.sociable + 1.0f - t.brave) / 2.0f < 0.27f; }
/// Shameless, but neither so bold nor so curious it puts washing off.
bool shamelessMeek(const Traits& t) { return shameless(t) && t.brave <= 0.62f && t.curious < 0.3f; }

/// This line in h's own log, or (with yes false) not.
Check own(const Human& h, std::string line, bool yes = true) {
    std::string label = (yes ? "#" : "no #") + std::to_string(h.id) + " " + line;
    return {label, [&h, line, yes](const Stage&) {
                bool found = false;
                for (const auto& e : h.log.entries()) found |= e.what == line;
                return found == yes;
            }};
}

/// A modest human and a shameless one by a basin they know, calm, each
/// set up the same way by dirty().
Checks washOrNot(Stage& s, const std::function<void(Human&)>& dirty,
                 const std::function<Checks(const Human& modest, const Human& shameless)>& expect,
                 bool (*slobLike)(const Traits&) = shameless) {
    auto b = s.basin();
    if (!b) return {{"room for a basin near the spawn", [](const Stage&) { return false; }}};
    Human& prim = s.first();
    Human& slob = s.add(seedWhere(slobLike, 12000), 2);
    for (Human* h : {&prim, &slob}) {
        P::knowChunk(s.world, *h);
        h->hunger = h->thirst = h->tiredness = 0.05f;
        h->bladder = h->bowels = 0.0f;
        dirty(*h);
    }
    s.run(60);
    return expect(prim, slob);
}

/// A stranger, thirsty and not yet parched, near a settler at home by
/// the basin, with a cake or not.
Checks guardedWater(Stage& s, bool strangerHasCake, Checks expect) {
    Human& settler = s.first();
    auto b = s.basin();
    if (!b) return {{"room for a basin near the spawn", [](const Stage&) { return false; }}};
    P::settle(settler, s.freeNear(*b, 1));
    Human& stranger = s.add(seedWhere(plain, 4000), 3);
    stranger.thirst = 0.7f;
    // An empty bottle, so it needs the settler's water.
    stranger.bottles = {Bottle{}};
    stranger.cakes = strangerHasCake ? 1 : 0;
    settler.hunger = stranger.hunger = 0.1f;
    settler.thirst = 0.1f;
    s.run(60);
    return expect;
}

/// The cells of the water body at c (4-connected, within the chunk's
/// reach), and every other water cell of the chunk c is in.
std::vector<Vec2i> waterBody(const World& w, Vec2i c) {
    std::vector<Vec2i> body{c};
    for (size_t i = 0; i < body.size(); i++)
        for (Vec2i d : {Vec2i{1, 0}, Vec2i{-1, 0}, Vec2i{0, 1}, Vec2i{0, -1}}) {
            Vec2i n = body[i] + d;
            if (drinkable(w.tile(n)) && std::find(body.begin(), body.end(), n) == body.end()) body.push_back(n);
        }
    return body;
}
/// Fouls a water body with feces and piss, as the human remembers tasting it.
void foulAll(World& w, Human& h, const std::vector<Vec2i>& body, float foul) {
    for (Vec2i c : body) {
        w.mess().foulWater(c, foul);
        P::tastedFoul(h, c, foul);
    }
}

/// The basin by the spawn fouled, as taster remembers tasting it; taster
/// moved out of sight of it, knowing the chunk; and clean water put
/// farther off the other way (at cleanAt). The foul cells, none if it
/// can't be done.
std::vector<Vec2i> foulAndClean(Stage& s, Human& taster, Vec2i* cleanAt = nullptr) {
    auto b = s.basin();
    if (!b) return {};
    auto foul = waterBody(s.world, *b);
    foulAll(s.world, taster, foul, 0.5f);
    P::place(s.world, taster, s.farFrom(*b, Sight::RADIUS + 5));
    P::knowChunk(s.world, taster);
    int near = cheb(taster.cell, *b);
    Vec2i slot = World::slotOf(taster.cell);
    std::optional<Vec2i> clean;
    for (int y = 0; y < CHUNK; y++)
        for (int x = 0; x < CHUNK; x++) {
            Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
            int d = cheb(c, taster.cell);
            if (s.world.tile(c) != Tile::Floor || s.world.table(c) || s.world.statue(c) || s.world.occupant(c) ||
                d <= near + 5 || d > near + 20)
                continue;
            if (!clean || cheb(c, *b) > cheb(*clean, *b)) clean = c;
        }
    if (!clean) return {};
    P::addWater(s.world, *clean);
    taster.memory.see(*clean, Tile::Basin);
    taster.hunger = 0.1f;
    if (cleanAt) *cleanAt = *clean;
    return foul;
}
/// Runs the stage, and how close h came to the foul cells.
/// Two toilets on floor either side of h, both in sight: a foul one,
/// pissed and shat in, nearer, and a clean one farther off. Gives the
/// cells h would stand at to use each (foul first); none if the floor
/// isn't there.
std::optional<std::pair<Vec2i, Vec2i>> foulAndCleanToilet(Stage& s, const Human& h) {
    Vec2i foulAt = h.cell + Vec2i{3, 0}, cleanAt = h.cell + Vec2i{-6, 0};
    for (Vec2i c : {foulAt, foulAt - Vec2i{1, 0}, cleanAt, cleanAt + Vec2i{1, 0}})
        if (s.world.tile(c) != Tile::Floor || s.world.occupant(c) || s.world.table(c) || s.world.statue(c))
            return std::nullopt;
    P::placeToilet(s.world, foulAt, foulAt - Vec2i{1, 0}, 1.5f, 0.3f);
    P::placeToilet(s.world, cleanAt, cleanAt + Vec2i{1, 0});
    return std::pair{foulAt - Vec2i{1, 0}, cleanAt + Vec2i{1, 0}};
}

/// Runs the stage, a second at a time, noting each cell h stood at in
/// state.
std::vector<Vec2i> stoodAt(Stage& s, const Human& h, Human::State state, int seconds) {
    std::vector<Vec2i> at;
    for (int t = 0; t < seconds * 30; t++) {
        s.run(1.0f / 30.0f);
        if (h.state == state && std::find(at.begin(), at.end(), h.cell) == at.end()) at.push_back(h.cell);
    }
    return at;
}

int keptOff(Stage& s, const Human& h, const std::vector<Vec2i>& foul, int seconds) {
    int closest = 1 << 20;
    for (int t = 0; t < seconds; t++) {
        s.run(1);
        for (Vec2i c : foul) closest = std::min(closest, cheb(h.cell, c));
    }
    return closest;
}

/// A cunning liar who counts victim a foe, knowing a trap (in its memory
/// only, unless real) next to it that victim doesn't know of.
void setUpLie(Stage& s, Human& liar, Human& victim, TrapKind bait = TrapKind::Spikes, int gap = 1) {
    P::setTrust(liar, victim, -0.6f);
    liar.learnTrap(s.freeNear(liar.cell, gap), bait);
    liar.hunger = victim.hunger = 0.1f;
    liar.thirst = victim.thirst = 0.1f;
}
/// The liar never lets on: no foe line said to victim.
Check straightFace(const Human& liar) {
    return {"no foe line from the liar", [&liar](const Stage&) {
                for (const auto& e : liar.log.entries())
                    if (e.what == "SAID \"YOU AGAIN.\" TO @") return false;
                return true;
            }};
}

/// None of these lines in anyone's log.
Check notLogged(std::vector<std::string> noneOf) {
    return {"not " + noneOf.front(), [noneOf](const Stage& s) {
                for (const auto& l : noneOf)
                    if (s.logged(l)) return false;
                return true;
            }};
}

/// The first human shut in a slam room with no key, nobody's keys lying
/// about, and everyone added fed and watered.
Human& shutInFirst(Stage& s) {
    Human& caller = s.first();
    P::clearChunk(s.world, caller.cell);
    caller.keys = 0;
    P::shutInBox(s.world, caller, {1, 0});
    return caller;
}
void calm(Human& h) { h.hunger = h.thirst = h.tiredness = 0.05f; }
/// A terminal or console desk stood on clear floor at least gap cells from
/// the first human, with a cell clear all round it; its cell (the desk's
/// top-left), or the human's own if none fits.
Vec2i computerNear(Stage& s, Rig kind, int gap) {
    const int w = kind == Rig::Desk ? DESK_W : 1, d = kind == Rig::Desk ? DESK_D : 1;
    auto clear = [&](Vec2i at) {
        for (int y = -1; y <= d; y++)
            for (int x = -1; x <= w; x++) {
                Vec2i c = at + Vec2i{x, y};
                if (s.world.tile(c) != Tile::Floor || s.world.occupant(c) || s.world.table(c) || s.world.statue(c) ||
                    World::slotOf(c) != World::slotOf(at))
                    return false;
            }
        return true;
    };
    Vec2i from = s.first().cell;
    for (int r = gap; r < 20; r++)
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++) {
                Vec2i at = from + Vec2i{x, y};
                if (std::max(std::abs(x), std::abs(y)) != r || !clear(at)) continue;
                P::standComputer(s.world, kind, at);
                return at;
            }
    std::fprintf(stderr, "scenario: no room for a computer\n");
    return from;
}
/// h's first lines said after each time this line is in its log (within 4 s), all different, at least n of them.
Check saidDifferently(const Human& h, const std::string& line, size_t n) {
    return {"#" + std::to_string(h.id) + " said " + std::to_string(n) + "+ different lines after " + line,
            [&h, line, n](const Stage&) {
                const auto& es = h.log.entries();
                std::vector<std::string> said;
                for (size_t i = 0; i < es.size(); i++) {
                    if (es[i].what != line) continue;
                    for (size_t j = i + 1; j < es.size() && es[j].time - es[i].time <= 4.0f; j++)
                        if (es[j].what.rfind("SAID ", 0) == 0) {
                            said.push_back(es[j].what);
                            break;
                        }
                }
                for (size_t i = 0; i < said.size(); i++)
                    for (size_t j = i + 1; j < said.size(); j++)
                        if (said[i] == said[j]) return false;
                return said.size() >= n;
            }};
}
/// h, with 3 keys, walled into a little box whose only way out is a
/// server room's outer door, having met the airlock this many times before
/// (AI/server_room_keys_plan.md).
const ServerRoom& airlockPocket(Stage& s, Human& h, int met) {
    P::clearChunk(s.world, h.cell);
    const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
    calm(h);
    s.run(8);
    P::wallIn(s.world, r.outer + Vec2i{0, 3}, Vec2i{0, -2});
    P::place(s.world, h, r.outer + Vec2i{0, 3});
    P::metAirlock(h, met);
    h.keys = 3;
    return r;
}
/// Two humans parched, with empty bottles and no keys, in a spawn room they
/// know wall to wall, with nothing in it; follower keeps company with leader.
void lostPair(Stage& s, Human& leader, Human& follower) {
    P::clearChunk(s.world, leader.cell);
    for (Human* h : {&leader, &follower}) {
        h->hunger = h->tiredness = 0.1f;
        h->thirst = 0.85f;
        h->keys = 0;
        h->bottles = {Bottle{}};
        P::knowChunk(s.world, *h);
    }
    P::setMet(leader, follower, 0.0f);
    P::setMet(follower, leader, 0.0f);
    P::setTrust(leader, follower, 0.4f);
    P::setTrust(follower, leader, 0.4f);
    P::keepWith(follower, leader);
}
/// Nobody asked anyone where water is more than once.
Check askedOnce() {
    return {"nobody asks the same one where water is twice", [](const Stage& s) {
                for (const auto& h : s.world.humans()) {
                    std::map<int, int> asked;
                    for (const auto& e : h->log.entries())
                        if (e.what == "ASKED @ WHERE THERE'S WATER" && (asked[e.who] += e.times) > 1) return false;
                }
                return true;
            }};
}
/// Runs a second at a time, and says whether anyone was ever seen lying down.
Check runSeeingLying(Stage& s, float seconds) {
    bool lay = false;
    for (float t = 0; t < seconds; t += 1.0f) {
        s.run(1.0f);
        for (const auto& h : s.world.humans()) lay |= h->lyingDown();
    }
    return {"someone lay down awake", [lay](const Stage&) { return lay; }};
}
/// Once all are awake, the hearers stand beside the slam room as the one
/// shut in cries out once.
void hearCry(Stage& s, Human& caller, std::vector<Human*> hearers) {
    s.run(8);
    for (Human* h : hearers) P::place(s.world, *h, s.freeNear(caller.cell, 4));
    P::cryNow(caller);
    s.run(1);
}
/// Caring about who is shut in: neither greedy nor erratic.
bool caring(const Traits& t) { return kindly(t) && t.greedy <= 0.4f; }

/// Some line in someone's log starting with from and ending with to.
Check loggedLike(std::string from, std::string to) {
    return {from + "..." + to, [from, to](const Stage& s) {
                for (const auto& h : s.world.humans())
                    for (const auto& e : h->log.entries())
                        if (e.what.rfind(from, 0) == 0 && e.what.size() >= to.size() &&
                            e.what.compare(e.what.size() - to.size(), to.size(), to) == 0)
                            return true;
                return false;
            }};
}

/// A hoarder's stash of cakes near the spawn, known to another human for
/// the hoarder's, with the hoarder this far off (beside it at 1); the other
/// human that hungry, with no cakes.
struct StashScene {
    Human* owner;
    Human* other;
    Vec2i at;
};
StashScene stashScene(Stage& s, int ownerFar, float hunger, uint64_t otherSeed, int cakes = 5) {
    Human& owner = s.first();
    P::clearChunk(s.world, owner.cell);
    Vec2i at = s.freeNear(owner.cell, 3);
    s.world.putDown(at, cakes, {});
    P::ownStash(s.world, owner, at);
    if (ownerFar > 1) P::place(s.world, owner, s.farFrom(at, ownerFar));
    else {
        // Beside it, it stays: it has made its home there.
        P::place(s.world, owner, s.freeNear(at, 1));
        P::settle(owner, owner.cell);
    }
    Human& other = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(at, 2), otherSeed);
    s.humans.push_back(&other);
    P::knowStash(s.world, other, at, owner);
    other.cakes = 0;
    other.hunger = hunger;
    owner.cakes = 1;
    owner.hunger = 0.1f;
    owner.thirst = other.thirst = 0.1f;
    return {&owner, &other, at};
}

/// A thirsty asker with an empty bottle (or none) close to a holder with
/// water, both far from the spawn basin, so the asker knows of no water.
/// The asker is timid, so it asks rather than fights; the holder has made
/// its home where it stands, so it stays about.
Checks askForWater(Stage& s, Checks expect, const std::function<void(Human& holder, Human& asker)>& setUp) {
    Human& holder = s.first();
    auto b = s.basin();
    if (!b) return {{"a basin near the spawn", [](const Stage&) { return false; }}};
    P::place(s.world, holder, s.farFrom(*b, 25));
    P::settle(holder, holder.cell);
    Human& asker = s.add(seedWhere([](const Traits& t) { return t.sociable >= 0.55f && t.brave < 0.3f; }, 11000), 1);
    holder.hunger = asker.hunger = 0.1f;
    holder.thirst = 0.1f;
    asker.thirst = 0.7f;
    asker.bottles = {Bottle{}};
    asker.cakes = 0;
    // Water dealt over, not discoveries looked at.
    P::seenItAll(holder);
    P::seenItAll(asker);
    setUp(holder, asker);
    s.run(100);
    return expect;
}

/// The first human comes to beside a locked door out of the spawn
/// chunk, and a second, like `hearer`, beyond it in the next chunk, close
/// enough to hear a call. Both calm, the first knowing its chunk, so its way
/// out lies by that door.
Checks callStage(Stage& s, const std::function<bool(const Traits&)>& hearer, uint64_t from,
                 const std::function<Checks(Human& caller, Human& heard)>& expect) {
    Human& caller = s.first();
    Vec2i home = caller.cell;
    Vec2i slot = World::slotOf(home);
    std::optional<Vec2i> door;
    for (int y = 0; y < CHUNK; y++)
        for (int x = 0; x < CHUNK; x++) {
            Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
            if (s.world.tile(c) == Tile::DoorLocked && (!door || cheb(c, home) < cheb(*door, home))) door = c;
        }
    if (!door) return {{"a locked door out of the spawn chunk", [](const Stage&) { return false; }}};
    // Which way is in, and which beyond.
    Vec2i in{0, 0};
    for (Vec2i d : DIRS4)
        if (s.world.tile(*door + d) == Tile::Floor) in = d;
    P::place(s.world, caller, *door + in);
    s.world.ensureGenerated(*door - in);
    std::optional<Vec2i> beyond;
    for (int r = 4; r <= 14 && !beyond; r++)
        for (int y = -r; y <= r && !beyond; y++)
            for (int x = -r; x <= r && !beyond; x++) {
                Vec2i c = *door + Vec2i{x, y};
                if (cheb(c, *door) == r && World::slotOf(c) != slot && s.world.tile(c) == Tile::Floor &&
                    !s.world.occupant(c))
                    beyond = c;
            }
    if (!beyond) return {{"floor beyond the door", [](const Stage&) { return false; }}};
    Human& heard = P::addHuman(s.world, HumanLook::random(s.rng), *beyond, seedWhere(hearer, from));
    heard.bottles = {Bottle{1.0f}};
    s.humans.push_back(&heard);
    calm(caller);
    calm(heard);
    P::knowChunk(s.world, caller);
    s.run(60);
    return expect(caller, heard);
}

/// Two friends come to: the first knows a basin near by and is calm, the
/// other is parched with an empty bottle; if asleep, it drops asleep once
/// both are up, with nobody asked to keep watch.
Checks leadToDrink(Stage& s, bool asleep, Checks expect) {
    Human& helper = s.first();
    auto b = s.basin();
    if (!b) return {{"a basin near the spawn", [](const Stage&) { return false; }}};
    Human& friendly = s.add(seedWhere([](const Traits& t) { return t.sociable >= 0.5f; }, 19000), 2);
    calm(helper);
    calm(friendly);
    P::setTrust(helper, friendly, 0.8f);
    P::setTrust(friendly, helper, 0.8f);
    s.run(10);
    P::knowChunk(s.world, helper);
    friendly.thirst = 0.9f;
    friendly.bottles = {Bottle{}};
    // Tired itself, the helper gives up any watch it would keep over them.
    if (asleep) {
        P::collapse(s.world, friendly);
        helper.tiredness = 0.55f;
    }
    s.run(asleep ? 60 : 20);
    return expect;
}

/// The first human wakes as in play and finds its bottle; a thirsty stranger
/// with none comes to beside it and takes it. Later thirsty, the first goes
/// back for it and finds it gone: a loss, or (hard, having learnt the thirst
/// lesson) a theft.
Checks bottleTaken(Stage& s, bool hard) {
    Human& h = s.first();
    h.hunger = h.thirst = 0.1f;
    if (hard) h.lessons.nearWater = 1.0f;
    s.run(8);
    auto at = P::foundBottle(h);
    if (!at) return {loggedBy(h, "FOUND A BOTTLE LYING THERE")};
    Human& taker = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(*at, 1),
                               seedWhere([](const Traits& t) { return t.brave >= 0.4f; }, 21000));
    s.humans.push_back(&taker);
    taker.hunger = 0.1f;
    taker.thirst = 0.7f;
    s.run(20);
    h.thirst = 0.7f;
    s.run(40);
    Checks expect{loggedBy(h, "FOUND A BOTTLE LYING THERE"), loggedBy(taker, "PICKED UP A BOTTLE LYING THERE")};
    if (hard) {
        expect.push_back(loggedBy(h, "SOMEONE TOOK MY BOTTLE"));
        expect.push_back(loggedBy(h, "@ MUST HAVE TAKEN IT"));
        expect.push_back(loggedBy(h, "MY BOTTLE IS GONE", false));
    } else {
        expect.push_back(loggedBy(h, "MY BOTTLE IS GONE"));
        expect.push_back(loggedBy(h, "SOMEONE TOOK MY BOTTLE", false));
    }
    return expect;
}

/// One human, knowing its chunk, with one cake on a table in it, darted
/// once before (wary 0.5), or having seen someone else darted, and as
/// hungry as asked. A second human, whom it trusts, told it of the cake
/// if tipped.
Checks wareCake(Stage& s, float hunger, Checks expect, bool tipped = false, bool saw = false) {
    Human& h = s.first();
    std::optional<Vec2i> cake;
    for (Vec2i c : s.world.cakes())
        if (s.world.tile(c) == Tile::Table && World::slotOf(c) == World::slotOf(h.cell) &&
            (!cake || cheb(c, h.cell) < cheb(*cake, h.cell) ||
             (cheb(c, h.cell) == cheb(*cake, h.cell) && (c.x < cake->x || (c.x == cake->x && c.y < cake->y)))))
            cake = c;
    if (!cake) return {{"a cake on a table in the spawn chunk", [](const Stage&) { return false; }}};
    P::clearChunk(s.world, h.cell);
    P::addCake(s.world, *cake);
    P::knowChunk(s.world, h, true);
    P::setWary(h, saw ? 0.0f : 0.5f, 0.0f);
    // A volley seen going off far down the chunk, as the world tells it.
    if (saw) h.learnTrap(s.farFrom(*cake, 12), TrapKind::Darts);
    h.hunger = hunger;
    h.thirst = 0.1f;
    h.cakes = 0;
    if (tipped) {
        Human& friend_ = s.add(seedWhere(plain, 8000), 6);
        P::setTrust(h, friend_, 0.6f);
        P::setMet(h, friend_, 0.0f);
        P::setMet(friend_, h, 0.0f);
        friend_.hunger = friend_.thirst = 0.1f;
        h.memory.cakeTips[*cake] = friend_.id;
    }
    s.run(60);
    return expect;
}

/// One human outside the gate of the slam room nearest the spawn, knowing
/// that chunk and its bait, shut in once before, hungry, with keys or not.
Checks wareSlam(Stage& s, int keys, Checks expect) {
    Human& h = s.first();
    std::optional<std::pair<Vec2i, Vec2i>> room;
    // Chunks round the spawn, ring by ring, until one has a slam room.
    Vec2i home = World::slotOf(h.cell);
    for (int r = 1; r <= 3; r++)
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++)
                if (std::max(std::abs(x), std::abs(y)) == r &&
                    std::none_of(s.world.traps().begin(), s.world.traps().end(),
                                 [](const auto& t) { return t.second.kind == TrapKind::Slam; }))
                    s.world.ensureGenerated((home + Vec2i{x, y}) * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
    for (const auto& [c, t] : s.world.traps())
        if (t.kind == TrapKind::Slam && (!room || cheb(c, h.cell) < cheb(room->first, h.cell) ||
                                         (cheb(c, h.cell) == cheb(room->first, h.cell) &&
                                          (c.x < room->first.x || (c.x == room->first.x && c.y < room->first.y)))))
            room = {c, t.from};
    if (!room) return {{"a slam room near the spawn", [](const Stage&) { return false; }}};
    auto [plate, gate] = *room;
    // Out of the gate, away from the plate, down the corridor and on.
    Vec2i out{0, 0};
    for (Vec2i d : DIRS4)
        if (cheb(gate + d, plate) > cheb(gate, plate) && passable(s.world.tile(gate + d))) out = d;
    Vec2i at = gate + out;
    while (s.world.tile(at + out) == Tile::Floor && cheb(at, gate) < 14) at = at + out;
    P::place(s.world, h, s.freeNear(at, 0));
    P::knowChunk(s.world, h, true);
    P::setWary(h, 0.0f, 0.5f);
    h.keys = keys;
    h.hunger = 0.6f;
    h.thirst = 0.1f;
    h.cakes = 0;
    // The bait is the only cake it knows of.
    h.memory.cakes = {plate};
    s.run(60);
    return expect;
}

/// How much the first human's dread rose at each of two bodies found in
/// turn, the second when it has already found bodiesBefore (and so has
/// hardened, or worn down, as its traits go). Dread is cleared before each.
std::pair<float, float> bodyDreads(Stage& s, int bodiesBefore) {
    Human& finder = s.first();
    Human& a = s.add(seedWhere(plain, 9900), 4);
    Human& b = s.add(seedWhere(plain, 9950), 12);
    calm(finder);
    float jumps[2] = {-1.0f, -1.0f};
    size_t seen = 0;
    float last = 0.0f;
    auto watch = [&] {
        if (P::bodiesSeen(finder) > seen && seen < 2) jumps[seen] = P::dread(finder) - last;
        seen = P::bodiesSeen(finder);
        last = P::dread(finder);
    };
    P::setDread(finder, 0.0f);
    P::kill(s.world, a);
    s.run(15, watch);
    P::setRepeats(finder, P::Repeat::Bodies, bodiesBefore);
    P::setDread(finder, 0.0f);
    last = 0.0f;
    P::place(s.world, b, s.freeNear(finder.cell, 2));
    P::kill(s.world, b);
    s.run(15, watch);
    return {jumps[0], jumps[1]};
}

const std::vector<Scenario> SCENARIOS = {
    {"wary-darts", "darted once, a human not yet hungry leaves the next table cake alone", seedWhere(plain, 9000),
     [](Stage& s) {
         return wareCake(s, 0.3f, {logged({"LEFT A CAKE ALONE. IT COULD BE RIGGED"}), notLogged({"PICKED UP A CAKE"})});
     }},
    {"wary-starving", "darted once, a starving human still risks the table cake", seedWhere(plain, 9000),
     [](Stage& s) { return wareCake(s, 0.85f, {logged({"PICKED UP A CAKE"})}); }},
    {"wary-saw", "having seen someone darted, a human not yet hungry leaves a table cake alone a while", seedWhere(plain, 9000),
     [](Stage& s) {
         // Less wary than if darted itself: it gives way sooner to hunger.
         return wareCake(s, 0.25f, {heldOff("LEFT A CAKE ALONE. IT COULD BE RIGGED", "PICKED UP A CAKE", 30.0f)}, false,
                         true);
     }},
    {"wary-tip", "darted once, it takes a cake a friend told it of on their word", seedWhere(plain, 9000),
     [](Stage& s) { return wareCake(s, 0.3f, {logged({"PICKED UP A CAKE"})}, true); }},
    {"wary-slam", "shut in once, with no key it leaves the bait in a room with one door", seedWhere(plain, 9000),
     [](Stage& s) {
         return wareSlam(s, 0, {logged({"LEFT THE CAKE IN A ROOM WITH ONE DOOR. NOT WITHOUT A KEY"}),
                                notLogged({"SHUT IN A SLAM ROOM"})});
     }},
    {"wary-slam-key", "shut in once, with a key in hand it goes for the bait anyway", seedWhere(plain, 9000),
     [](Stage& s) { return wareSlam(s, 1, {logged({"PICKED UP A CAKE"})}); }},
    {"winded", "short of breath, a run in a fight ends winded",
     seedWhere([](const Traits& t) { return t.sociable >= 0.6f && t.greedy >= 0.5f && !cunning(t); }, 4200),
     [](Stage& s) {
         Human& robber = s.first();
         Human& victim = s.add(seedWhere([](const Traits& t) { return t.brave < 0.4f && t.greedy >= 0.8f; }, 5000), 4);
         P::clearChunk(s.world, robber.cell);
         robber.hunger = 0.6f;
         robber.cakes = 0;
         victim.hunger = 0.1f;
         victim.cakes = 2;
         robber.thirst = victim.thirst = robber.tiredness = victim.tiredness = 0.1f;
         P::setTrust(robber, victim, 0.2f);
         P::setTrust(victim, robber, 0.2f);
         P::setLungs(robber, 0.3f);
         P::setLungs(victim, 0.3f);
         s.run(60);
         return Checks{logged({"OUT OF BREATH"})};
     }},
    {"share", "a friend with cakes to spare feeds a starving friend", 99,
     [](Stage& s) {
         Human& giver = s.first();
         Human& starving = s.add(seedWhere(kindly, 1000));
         giver.cakes = 3;
         giver.hunger = 0.1f;
         starving.hunger = 0.97f;
         starving.cakes = 0;
         P::setTrust(giver, starving, 0.6f);
         P::setTrust(starving, giver, 0.6f);
         s.run(60);
         return Checks{logged({"GAVE @ A CAKE"}), logged({"@ GAVE ME A CAKE"})};
     }},
    {"keep-watch", "a rested friend keeps watch over one falling asleep", 99,
     [](Stage& s) {
         Human& sleeper = s.first();
         Human& watcher = s.add(seedWhere(kindly, 2000));
         sleeper.tiredness = 0.97f;
         watcher.tiredness = 0.0f;
         sleeper.hunger = watcher.hunger = 0.1f;
         sleeper.thirst = watcher.thirst = 0.1f;
         P::setTrust(sleeper, watcher, 0.6f);
         P::setTrust(watcher, sleeper, 0.6f);
         s.run(90);
         return Checks{logged({"KEEPING WATCH OVER @", "AGREED TO KEEP WATCH OVER @"})};
     }},
    {"rob-sleeper", "a greedy, bold stranger takes a cake off a sleeper", 99,
     [](Stage& s) {
         Human& sleeper = s.first();
         Human& robber = s.add(seedWhere([](const Traits& t) { return t.greedy >= 0.65f && t.brave >= 0.5f; }, 3000));
         sleeper.tiredness = 0.99f;
         sleeper.cakes = 2;
         sleeper.hunger = robber.hunger = 0.1f;
         robber.cakes = 0;
         robber.tiredness = 0.0f;
         s.run(120);
         return Checks{logged({"ROBBED @ IN THEIR SLEEP", "@ WOKE UP AS I ROBBED THEM"})};
     }},
    {"rob-companion", "a hungry human robs the one it keeps with, then leaves them, and never misses them",
     seedWhere([](const Traits& t) { return t.sociable >= 0.6f && t.greedy >= 0.5f && !cunning(t); }, 4200),
     [](Stage& s) {
         Human& robber = s.first();
         Human& victim = s.add(seedWhere([](const Traits& t) { return t.brave < 0.4f && t.greedy >= 0.8f; }, 5000));
         P::clearChunk(s.world, robber.cell);
         robber.hunger = 0.6f;
         robber.cakes = 0;
         victim.hunger = 0.1f;
         victim.cakes = 2;
         robber.thirst = victim.thirst = robber.tiredness = victim.tiredness = 0.1f;
         P::setTrust(robber, victim, 0.2f);
         P::setTrust(victim, robber, 0.2f);
         P::setMet(robber, victim, 0.0f);
         P::setMet(victim, robber, 0.0f);
         P::setCompanion(robber, victim);
         s.run(120);
         // Nothing it does after the first blow treats them as company.
         Check after{"no company line for the victim after robbing them", [&robber, &victim](const Stage&) {
                         float hit = -1.0f;
                         for (const auto& e : robber.log.entries()) {
                             if (hit < 0.0f && e.what.rfind("HIT @", 0) == 0 && e.who == victim.id) hit = e.time;
                             if (hit >= 0.0f && e.time > hit && e.who == victim.id &&
                                 (e.what == "LOST @" || e.what == "KEEPING WITH @" || e.what == "FOLLOWING @, WHO HAS A KEY"))
                                 return false;
                         }
                         return hit >= 0.0f && P::companion(robber) != victim.id;
                     }};
         return Checks{logged({"HIT @ AND TOOK A CAKE"}), logged({"PARTED FROM @ AFTER WHAT I DID"}), after};
     }},
    {"water-free", "a friendly settler by the basin lets a thirsty stranger drink", seedWhere(guardsFree),
     [](Stage& s) { return guardedWater(s, false, {logged({"LET @ DRINK MY WATER"})}); }},
    {"water-price", "a grasping settler asks a cake for its water, and is paid or cheated",
     seedWhere(guardsForCake),
     [](Stage& s) {
         return guardedWater(s, true,
                             {logged({"ASKED @ A CAKE FOR MY WATER"}),
                              logged({"PAID @ A CAKE FOR WATER", "DRANK @'S WATER AND DIDN'T PAY"})});
     }},
    {"water-refused", "a stingy settler turns a thirsty stranger away", seedWhere(guardsAll),
     [](Stage& s) { return guardedWater(s, false, {logged({"TURNED @ AWAY FROM MY WATER"})}); }},
    {"lie-water", "a cunning human sends a thirsty foe, who knows of no water, to water over spikes",
     seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // Far from a basin set by the spawn, so the foe knows of no water; the liar
         // knows the whole chunk.
         auto b = s.basin();
         if (!b) return Checks{{"room for a basin near the spawn", [](const Stage&) { return false; }}};
         P::knowChunk(s.world, liar);
         P::place(s.world, liar, s.farFrom(*b, 25));
         Human& victim = s.add(seedWhere(plain, 5000));
         setUpLie(s, liar, victim);
         victim.thirst = 0.7f;
         // Nothing to drink on either, nor to fight over.
         victim.bottles = {Bottle{}};
         liar.bottles.clear();
         P::seenItAll(liar);
         P::seenItAll(victim);
         s.run(60);
         return Checks{logged({"TOLD @ A WAY TO WATER OVER SPIKES"}), straightFace(liar)};
     }},
    {"lie-route", "a cunning human sends a foe with a key to a door over spikes", seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // The liar knows its whole chunk, doors and all; the foe only
         // what it sees.
         P::knowChunk(s.world, liar);
         Human& victim = s.add(seedWhere(plain, 6000));
         setUpLie(s, liar, victim);
         victim.keys = 1;
         s.run(60);
         return Checks{logged({"TOLD @ A WAY OVER SPIKES"}), logged({"@ TOLD ME THE WAY TO A DOOR"}),
                       straightFace(liar)};
     }},
    {"lie-darts", "the same over a real dart plate, which the foe walks into and sees through",
     seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // Set up apart from how the spawn room came out: the plate lies in the
         // corridor to the room's locked door, a few steps in from it, walled
         // in to one cell there, so the way to that door is over it.
         const Vec2i home = World::slotOf(liar.cell);
         std::optional<Vec2i> inner;
         for (int y = 0; y < CHUNK && !inner; y++)
             for (int x = 0; x < CHUNK && !inner; x++) {
                 Vec2i c = home * CHUNK + Vec2i{x, y};
                 if (s.world.tile(c) != Tile::DoorLocked) continue;
                 for (Vec2i d : DIRS4)
                     if (World::slotOf(c + d) == home && s.world.tile(c + d) == Tile::Floor) inner = c + d;
             }
         if (!inner) return Checks{{"a locked door out of the room", [](const Stage&) { return false; }}};
         // Along the corridor from the door, the first cell at least four
         // steps in where it is just three cells across.
         auto floor = [&](Vec2i c) { return s.world.tile(c) == Tile::Floor; };
         std::optional<Vec2i> plate;
         Vec2i across{};
         std::unordered_map<Vec2i, int, Vec2iHash> dist{{*inner, 0}};
         std::deque<Vec2i> open{*inner};
         while (!open.empty() && !plate) {
             Vec2i p = open.front();
             open.pop_front();
             for (Vec2i a : {Vec2i{1, 0}, Vec2i{0, 1}}) {
                 Vec2i along{a.y, a.x};
                 if (dist[p] >= 4 && floor(p + a) && floor(p - a) && !floor(p + a * 2) && !floor(p - a * 2) &&
                     floor(p + along) && floor(p - along)) {
                     plate = p;
                     across = a;
                     break;
                 }
             }
             for (Vec2i d : DIRS4)
                 if (floor(p + d) && World::slotOf(p + d) == home && !dist.count(p + d)) {
                     dist[p + d] = dist[p] + 1;
                     open.push_back(p + d);
                 }
         }
         if (!plate) return Checks{{"a corridor to the locked door", [](const Stage&) { return false; }}};
         P::addWall(s.world, *plate + across);
         P::addWall(s.world, *plate - across);
         P::knowChunk(s.world, liar);
         // A restless loner: it goes its own way, down the route, not with the liar.
         Human& victim = s.add(seedWhere([](const Traits& t) { return t.sociable < 0.3f && t.curious > 0.5f; }, 6000));
         P::setTrust(liar, victim, -0.6f);
         liar.hunger = victim.hunger = 0.1f;
         liar.thirst = victim.thirst = 0.1f;
         P::addTrap(s.world, *plate, TrapKind::Darts);
         liar.learnTrap(*plate, TrapKind::Darts);
         victim.keys = 1;
         // Hungry with nothing to eat, a key holder makes for a door sooner.
         victim.hunger = 0.7f;
         s.run(150);
         return Checks{logged({"TOLD @ A WAY OVER DARTS"}), logged({"@ LIED ABOUT THE WAY"})};
     }},
    {"lie-cake", "a cunning human sends a hungry foe to a rigged cake, and is blamed for the darts",
     seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // The cake farthest off within the chunk, so the foe hasn't seen it,
         // made rigged for real; the liar knows it is.
         std::optional<Vec2i> cake;
         for (Vec2i c : s.world.cakes())
             if (s.world.tile(c) == Tile::Table && World::slotOf(c) == World::slotOf(liar.cell) &&
                 (!cake || cheb(c, liar.cell) > cheb(*cake, liar.cell) ||
                  (cheb(c, liar.cell) == cheb(*cake, liar.cell) && (c.x < cake->x || (c.x == cake->x && c.y < cake->y)))))
                 cake = c;
         if (!cake) return Checks{{"a cake on a table in the spawn chunk", [](const Stage&) { return false; }}};
         // The only cake about, so neither goes for another (or fights over it).
         P::clearChunk(s.world, liar.cell);
         P::addCake(s.world, *cake);
         P::knowChunk(s.world, liar);
         P::addTrap(s.world, *cake, TrapKind::Darts);
         liar.memory.cakes.insert(*cake);
         liar.learnTrap(*cake, TrapKind::Darts);
         Human& victim = s.add(seedWhere(plain, 7000));
         P::setTrust(liar, victim, -0.6f);
         liar.hunger = 0.1f;
         liar.thirst = victim.thirst = 0.1f;
         victim.hunger = 0.75f;
         victim.cakes = 0;
         s.run(150);
         return Checks{logged({"TOLD @ OF A RIGGED CAKE"}), logged({"@ SENT ME TO A RIGGED CAKE"})};
     }},
    {"false-cake", "a cunning human sends a hungry foe to a bare table, and is found out", seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // Every table bare, and the liar knows it.
         P::clearChunk(s.world, liar.cell);
         P::knowChunk(s.world, liar, true);
         Human& victim = s.add(seedWhere(plain, 9500));
         P::setTrust(liar, victim, -0.6f);
         liar.hunger = 0.1f;
         liar.thirst = victim.thirst = 0.1f;
         victim.hunger = 0.75f;
         victim.cakes = 0;
         s.run(150);
         return Checks{logged({"TOLD @ OF A CAKE THAT ISN'T THERE"}), logged({"@ TOLD ME WHERE A CAKE IS"}),
                       logged({"NOTHING WAS WHERE @ SAID"})};
     }},
    {"false-key", "a cunning human sends a keyless foe to a bare table for a key, and is found out",
     seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         P::clearChunk(s.world, liar.cell);
         P::knowChunk(s.world, liar, true);
         Human& victim = s.add(seedWhere(plain, 9600));
         P::setTrust(liar, victim, -0.6f);
         liar.hunger = victim.hunger = 0.1f;
         liar.thirst = victim.thirst = 0.1f;
         victim.keys = 0;
         s.run(150);
         return Checks{logged({"TOLD @ OF A KEY THAT ISN'T THERE"}), logged({"@ TOLD ME WHERE A KEY IS"}),
                       logged({"NOTHING WAS WHERE @ SAID"})};
     }},
    {"gain-cake", "a cunning stranger sends a hungry rival away from a cake it wants, then takes it",
     seedWhere([](const Traits& t) { return cunning(t) && t.greedy >= 0.7f; }),
     [](Stage& s) {
         Human& liar = s.first();
         // Set up apart from how the spawn room came out: the rival would see
         // the bare table across the open room once its daze lifts, so that is
         // through the door, on the table farthest off in the chunk beyond,
         // and the cake on the room's table farthest from the door. The liar
         // knows both chunks and the cake, and stands about ten cells from the
         // cake on the side away from the door; the rival, a stranger, wakes
         // a few cells behind it, as far from the cake as it can be and still
         // be a rival for it, so it sees the cake late and the liar is nearer.
         const Vec2i home = World::slotOf(liar.cell);
         std::optional<Vec2i> door, cake, bare;
         for (int y = 0; y < CHUNK && !bare; y++)
             for (int x = 0; x < CHUNK && !bare; x++) {
                 Vec2i c = home * CHUNK + Vec2i{x, y};
                 Tile t = s.world.tile(c);
                 if (t != Tile::DoorOpen && t != Tile::DoorLocked) continue;
                 for (Vec2i d : DIRS4) {
                     Vec2i out = c + d;
                     if (World::slotOf(out) == home) continue;
                     s.world.ensureGenerated(out);
                     for (const Table& tb : s.world.tables())
                         if (P::chunkOf(s.world, tb.at) == P::chunkOf(s.world, out) && (!bare || cheb(tb.at, c) > cheb(*bare, c)))
                             bare = tb.at;
                     if (bare) door = c;
                 }
             }
         if (!bare) return Checks{{"a table through a door of the spawn room", [](const Stage&) { return false; }}};
         for (const Table& t : s.world.tables())
             if (World::slotOf(t.at) == home && (!cake || cheb(t.at, *door) > cheb(*cake, *door))) cake = t.at;
         if (!cake) return Checks{{"a table in the spawn room", [](const Stage&) { return false; }}};
         P::clearChunk(s.world, *bare);
         P::knowChunkOf(s.world, liar, *bare);
         P::clearChunk(s.world, *cake);
         P::addCake(s.world, *cake);
         P::knowChunk(s.world, liar, true);
         auto open = [&](Vec2i c) {
             return s.world.tile(c) == Tile::Floor && !s.world.occupant(c) && !s.world.table(c) && !s.world.statue(c);
         };
         std::optional<Vec2i> stand;
         for (int y = -10; y <= 10; y++)
             for (int x = -10; x <= 10; x++) {
                 Vec2i c = *cake + Vec2i{x, y};
                 if (cheb(c, *cake) < 8 || !open(c)) continue;
                 if (!stand || cheb(c, *door) > cheb(*stand, *door)) stand = c;
             }
         if (!stand) return Checks{{"floor near the cake", [](const Stage&) { return false; }}};
         P::place(s.world, liar, *stand);
         std::optional<Vec2i> behind;
         for (int y = -10; y <= 10; y++)
             for (int x = -10; x <= 10; x++) {
                 Vec2i c = *stand + Vec2i{x, y};
                 if (cheb(c, *stand) > 6 || cheb(c, *cake) >= RIVAL_NEAR || !open(c)) continue;
                 if (!behind || cheb(c, *cake) > cheb(*behind, *cake)) behind = c;
             }
         if (!behind) return Checks{{"floor behind the liar", [](const Stage&) { return false; }}};
         Human& rival = s.add(seedWhere(plain, 9700));
         P::place(s.world, rival, *behind);
         liar.hunger = 0.4f;
         liar.cakes = 0;
         rival.hunger = 0.68f;
         rival.cakes = 0;
         liar.thirst = rival.thirst = 0.1f;
         s.run(90);
         Check took{"the liar picked up the cake after the lie", [&liar](const Stage&) {
                        float lied = -1.0f;
                        for (const auto& e : liar.log.entries()) {
                            if (e.what == "SENT @ AWAY FROM A CAKE I WANT") lied = e.time;
                            if (lied >= 0.0f && e.what == "PICKED UP A CAKE") return true;
                        }
                        return false;
                    }};
         return Checks{logged({"SENT @ AWAY FROM A CAKE I WANT"}), logged({"@ TOLD ME WHERE A CAKE IS"}), took};
     }},
    {"gain-trap", "a cunning stranger sells spikes that aren't there for a cake",
     seedWhere([](const Traits& t) { return cunning(t) && t.greedy >= 0.7f; }),
     [](Stage& s) {
         Human& liar = s.first();
         P::knowChunk(s.world, liar);
         Human& buyer = s.add(seedWhere(plain, 9800));
         buyer.cakes = 4;
         liar.hunger = buyer.hunger = 0.1f;
         liar.thirst = buyer.thirst = 0.1f;
         s.run(60);
         return Checks{logged({"SOLD @ A TRAP THAT ISN'T THERE"}), logged({"PAID @ A CAKE FOR NEWS"})};
     }},
    {"slander", "a cunning gossip makes someone it is only wary of out to be dangerous", seedWhere(cunning),
     [](Stage& s) {
         Human& teller = s.first();
         Human& listener = s.add(seedWhere(kindly, 7500));
         Human& third = s.add(seedWhere(plain, 8000), 12);
         P::setTrust(teller, listener, 0.6f);
         P::setTrust(listener, teller, 0.6f);
         // Met long ago, so news is what brings them together again.
         P::setMet(teller, listener, -100.0f);
         P::setMet(listener, teller, -100.0f);
         // Too mild to pass on honestly (GOSSIP_WORTH 0.2).
         P::setTrust(teller, third, -0.1f);
         for (Human* h : {&teller, &listener, &third}) h->hunger = h->thirst = 0.1f;
         s.run(60);
         return Checks{logged({"LIED TO @ ABOUT #"}), logged({"# WARNED ME ABOUT @"})};
     }},
    {"gossip", "friends pass on a name and a warning about a third", 99,
     [](Stage& s) {
         Human& teller = s.first();
         Human& friendOf = s.add(seedWhere(kindly, 7000));
         Human& third = s.add(seedWhere(plain, 8000), 8);
         P::setTrust(teller, friendOf, 0.6f);
         P::setTrust(friendOf, teller, 0.6f);
         P::setTrust(teller, third, -0.6f);
         teller.memory.names[third.id] = third.name;
         teller.hunger = friendOf.hunger = third.hunger = 0.1f;
         teller.thirst = friendOf.thirst = third.thirst = 0.1f;
         s.run(60);
         return Checks{logged({"HEARD @'S NAME FROM #"}), logged({"# WARNED ME ABOUT @"})};
     }},
    {"body-known", "a human finds a fresh body and knows who it was, not someone's bones", 1,
     [](Stage& s) {
         Human& finder = s.first();
         Human& victim = s.add(seedWhere(plain, 9900));
         P::kill(s.world, victim);
         finder.hunger = finder.thirst = finder.tiredness = 0.1f;
         s.run(20);
         return Checks{logged({"FOUND @ DEAD"}), unlogged("FOUND SOMEONE'S BONES")};
     }},
    {"body-bones", "a human finds a stranger's bare bones and can't tell whose", 1,
     [](Stage& s) {
         Human& finder = s.first();
         Human& victim = s.add(seedWhere(plain, 9900));
         P::kill(s.world, victim, 2000.0f);
         finder.hunger = finder.thirst = finder.tiredness = 0.1f;
         s.run(20);
         return Checks{logged({"FOUND SOMEONE'S BONES"}), unlogged("FOUND @ DEAD")};
     }},
    {"spilled-pool", "a killer looks at its victim's blood unafraid, while a bystander is frightened by it", 99,
     [](Stage& s) {
         Human& killer = s.first();
         Human& victim = s.add(seedWhere(plain, 9900), 1);
         Human& bystander = s.add(seedWhere(plain, 9950), 3);
         for (Human* h : s.humans) h->hunger = h->thirst = h->tiredness = 0.05f;
         s.run(6);
         victim.takeHit(s.world, killer, 10.0f);
         // A pool of the victim's blood, round where it fell.
         Vec2i at = victim.cell;
         for (int i = 0; i < 6; i++)
             s.world.blood().drop(s.world, at.x * CELL_PX + CELL_PX / 2.0f, at.y * CELL_PX + CELL_PX / 2.0f, 4.0f,
                                  victim.id, s.rng);
         s.run(10);
         auto has = [](const Human& h, const std::string& line) {
             for (const auto& e : h.log.entries())
                 if (e.what == line) return true;
             return false;
         };
         int k = killer.id, b = bystander.id;
         return Checks{{"killer saw the blood it spilled",
                        [=](const Stage& st) { return has(st.world.human(k), "SAW THE BLOOD I SPILLED FROM @"); }},
                       {"killer not frightened by it",
                        [=](const Stage& st) { return !has(st.world.human(k), "SAW A FRESH POOL OF BLOOD"); }},
                       {"bystander frightened by it",
                        [=](const Stage& st) { return has(st.world.human(b), "SAW A FRESH POOL OF BLOOD"); }}};
     }},
    {"blame-before", "a timid human sees a bloodied stranger, then a fresh body: blames, runs, shouts it",
     seedWhere(blamesBefore),
     [](Stage& s) {
         Human& witness = s.first();
         Human& killer = s.add(seedWhere(plain, 9500), 4);
         Human& victim = s.add(seedWhere(plain, 9600), 3);
         for (Human* h : s.humans) h->hunger = h->thirst = h->tiredness = 0.05f;
         s.run(3);
         P::bloody(killer);
         s.run(2);
         P::kill(s.world, victim);
         s.run(0.2f);
         // One who comes along after the blood is gone has only the shout to go by.
         P::clean(killer);
         Human& hearer = s.add(seedWhere(kindly, 9700), 2);
         hearer.hunger = hearer.thirst = hearer.tiredness = 0.05f;
         P::setTrust(hearer, witness, 0.5f);
         s.run(30);
         return Checks{logged({"@ MUST HAVE KILLED #"}), logged({"RAN FROM @, A KILLER"}),
                       logged({"# SAYS @ IS A KILLER"})};
     }},
    {"blame-after", "a brave friend finds a fresh body, then a bloodied stranger near it, and goes for them",
     seedWhere(blamesAfter),
     [](Stage& s) {
         Human& witness = s.first();
         Human& victim = s.add(seedWhere(plain, 9800), 2);
         Human& killer = s.add(seedWhere(plain, 9900), 5);
         P::setTrust(witness, victim, 0.6f);
         P::setMet(witness, victim, 0.0f);
         for (Human* h : s.humans) h->hunger = h->thirst = h->tiredness = 0.05f;
         s.run(3);
         P::kill(s.world, victim);
         s.run(3);
         P::bloody(killer);
         s.run(30);
         return Checks{logged({"@ MUST HAVE KILLED #"}), logged({"WENT FOR @ FOR WHAT THEY DID"})};
     }},
    {"bottle-pour", "a bottle of foul water is poured out at the basin and filled with clean", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         h.bottles = {Bottle{0.4f, 0.6f, 0.0f}};
         h.hunger = h.thirst = h.tiredness = 0.1f;
         s.run(90);
         return Checks{logged({"POURED OUT MY BOTTLE. IT WAS FOUL"}), logged({"FILLED MY BOTTLE"})};
     }},
    {"bottle-give", "a friend asked for water pours some into the asker's bottle", seedWhere(kindly, 12000),
     [](Stage& s) {
         return askForWater(s, {logged({"ASKED @ FOR WATER"}), logged({"GAVE @ SOME WATER"}), logged({"@ GAVE ME SOME WATER"})},
                            [](Human& holder, Human& asker) {
                                P::setTrust(holder, asker, 0.6f);
                                P::setTrust(asker, holder, 0.6f);
                            });
     }},
    {"bottle-only", "a friend hands its only bottle to a thirsty friend with none", seedWhere(kindly, 12500),
     [](Stage& s) {
         return askForWater(s, {logged({"GAVE @ A BOTTLE"}), logged({"@ GAVE ME A BOTTLE"})}, [](Human& holder, Human& asker) {
             P::setTrust(holder, asker, 0.6f);
             P::setTrust(asker, holder, 0.6f);
             asker.bottles.clear();
         });
     }},
    {"bottle-sell", "a grasping stranger asked for water sells it for a cake",
     seedWhere([](const Traits& t) { return t.greedy >= 0.55f && t.sociable < 0.4f && t.brave < 0.4f; }, 13014),
     [](Stage& s) {
         return askForWater(s, {logged({"SOLD @ SOME WATER FOR A CAKE"}), logged({"BOUGHT SOME WATER FROM @ FOR A CAKE"})},
                            [](Human&, Human& asker) { asker.cakes = 1; });
     },
     "since light limits sight (light_plan step 4), the asker wanders dark corridors, comes back parched and is "
     "told off its place before it asks, and takes the water by force"},
    {"bottle-lie", "a cunning stranger says its bottle is empty, and is seen through",
     seedWhere([](const Traits& t) { return cunning(t) && t.sociable < 0.4f; }, 14000),
     [](Stage& s) {
         return askForWater(s, {logged({"TOLD @ MY BOTTLE WAS EMPTY"}), logged({"@ SAID THEIR BOTTLE WAS EMPTY. IT WASN'T"})},
                            [](Human&, Human&) {});
     },
     "since light limits sight (light_plan step 4), the asker wanders dark corridors, comes back parched and is "
     "told off its place before it asks, and takes the water by force"},
    {"bottle-refuse", "a stingy stranger won't give any of its water",
     seedWhere([](const Traits& t) { return t.greedy < 0.35f && t.sociable < 0.4f && t.brave < 0.3f; }, 15119),
     [](Stage& s) {
         // The asker counts the stranger a friend, so it surely asks; the
         // stranger doesn't, so it surely refuses.
         return askForWater(s, {logged({"WOULDN'T GIVE @ WATER"}), logged({"@ WOULDN'T GIVE ME WATER"})},
                            [](Human& holder, Human& asker) { P::setTrust(asker, holder, 0.8f); });
     }},
    {"bottle-snatch", "a bold, thirsty human with nothing to drink takes a foe's bottle in a fight", 99,
     [](Stage& s) {
         Human& holder = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         P::place(s.world, holder, s.farFrom(*b, 25));
         Human& thief = s.add(seedWhere([](const Traits& t) { return t.brave >= 0.7f; }, 16000), 2);
         P::setTrust(thief, holder, -0.6f);
         holder.hunger = thief.hunger = 0.1f;
         holder.thirst = 0.1f;
         thief.thirst = 0.75f;
         thief.bottles = {Bottle{}};
         s.run(60);
         return Checks{logged({"HIT @ AND TOOK THEIR BOTTLE"}), logged({"@ HIT ME AND TOOK MY BOTTLE"})};
     }},
    {"wake-bottle", "a human wakes near a bottle, notes it, leaves it, and goes back for it once thirsty", 99,
     [](Stage& s) {
         Human& h = s.first();
         h.hunger = h.thirst = 0.1f;
         s.run(12);
         auto at = P::foundBottle(h);
         bool leftIt = h.bottles.empty();
         if (!at) return Checks{loggedBy(h, "FOUND A BOTTLE LYING THERE")};
         // Walked off a way, then thirsty.
         P::place(s.world, h, s.farFrom(*at, 10));
         h.thirst = 0.7f;
         s.run(40);
         return Checks{loggedBy(h, "FOUND A BOTTLE LYING THERE"),
                       {"left it lying while not thirsty", [leftIt](const Stage&) { return leftIt; }},
                       loggedBy(h, "PICKED UP A BOTTLE LYING THERE"), loggedBy(h, "DRANK FROM MY BOTTLE")};
     },
     nullptr, true},
    {"wake-first", "waking as in play, it says where it is before the bottle it finds", 99,
     [](Stage& s) {
         Human& h = s.first();
         s.run(20);
         // Its first line, on waking, shows its full time before the next,
         // for the bottle found beside it, is said.
         return Checks{loggedBy(h, "WOKE UP, REMEMBERING NOTHING"), loggedBy(h, "FOUND A BOTTLE LYING THERE"),
                       {"the waking line is read before the bottle line", [&h](const Stage&) {
                            std::vector<float> said;
                            bool wokeFirst = false;
                            for (const auto& e : h.log.entries()) {
                                if (e.what == "FOUND A BOTTLE LYING THERE") wokeFirst = said.size() == 1;
                                if (e.what.rfind("SAID ", 0) == 0) said.push_back(e.time);
                            }
                            return wokeFirst && said.size() >= 2 && said[1] - said[0] >= SPEECH_TIME;
                        }}};
     },
     nullptr, true},
    {"bottle-gone", "a stranger takes the bottle a human found and left; it finds it gone, a loss", 99,
     [](Stage& s) { return bottleTaken(s, false); }, nullptr, true},
    {"bottle-stolen", "the same, once it has learnt the thirst lesson: stolen, and it blames the stranger", 99,
     [](Stage& s) { return bottleTaken(s, true); }, nullptr, true},
    {"scarce-once", "its bottle runs dry, then it fills it: water is scarce, said once only", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         P::place(s.world, h, s.farFrom(*b, 20));
         h.bottles = {Bottle{0.3f}};
         h.hunger = 0.1f;
         h.thirst = 0.75f;
         s.run(90);
         const Human* who = &h;
         return Checks{loggedBy(h, "MY BOTTLE RAN DRY"), loggedBy(h, "FILLED MY BOTTLE"),
                       {"#0 WATER IS SCARCE once", [who](const Stage&) {
                            int n = 0;
                            for (const auto& e : who->log.entries())
                                if (e.what == "WATER IS SCARCE. I'M KEEPING THIS BOTTLE") n += e.times;
                            return n == 1;
                        }}};
     }},
    {"scarce-keeps", "having seen water is scarce, a friend won't hand over its only bottle", seedWhere(kindly, 12500),
     [](Stage& s) {
         return askForWater(s, {logged({"ASKED @ FOR WATER"}), notLogged({"GAVE @ A BOTTLE"})},
                            [](Human& holder, Human& asker) {
                                P::setTrust(holder, asker, 0.6f);
                                P::setTrust(asker, holder, 0.6f);
                                P::setScarce(holder);
                                asker.bottles.clear();
                            });
     }},
    {"hard-no-sell", "a grasping stranger who learnt the thirst lesson won't sell its water",
     seedWhere([](const Traits& t) { return t.greedy >= 0.55f && t.sociable < 0.4f && t.brave < 0.4f; }, 13014),
     [](Stage& s) {
         return askForWater(s, {logged({"ASKED @ FOR WATER"}), notLogged({"SOLD @ SOME WATER FOR A CAKE"})},
                            [](Human& holder, Human& asker) {
                                asker.cakes = 1;
                                holder.lessons.nearWater = 1.0f;
                            });
     },
     "since light limits sight (light_plan step 4), the asker wanders dark corridors, comes back parched and is "
     "told off its place before it asks, and takes the water by force"},
    {"pile-cakes", "a hungry human with no cakes picks up cakes lying on the floor", 99,
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         s.world.putDown(s.freeNear(h.cell, 3), 3, {});
         h.cakes = 0;
         h.hunger = 0.7f;
         h.thirst = 0.1f;
         s.run(40);
         return Checks{loggedLike("PICKED UP", "CAKES LYING THERE")};
     }},
    {"pile-water", "a thirsty human with an empty bottle picks up a full one lying there", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         P::place(s.world, h, s.farFrom(*b, 25));
         s.world.putDown(s.freeNear(h.cell, 2), 0, {Bottle{1.0f}});
         h.bottles = {Bottle{}};
         h.hunger = 0.1f;
         h.thirst = 0.7f;
         s.run(40);
         return Checks{logged({"PICKED UP A BOTTLE LYING THERE"}), logged({"LEFT MY EMPTY BOTTLE FOR A FULLER ONE"}),
                       logged({"DRANK FROM MY BOTTLE"})};
     }},
    {"pile-pour-over", "a thirsty human pours its clean, low bottle into a fuller one lying there and takes that", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         P::place(s.world, h, s.farFrom(*b, 25));
         s.world.putDown(s.freeNear(h.cell, 1), 0, {Bottle{0.7f}});
         h.bottles = {Bottle{0.1f}};
         h.hunger = 0.1f;
         h.thirst = 0.7f;
         s.run(40);
         return Checks{logged({"POURED MY BOTTLE INTO A FULLER ONE"})};
     }},
    {"pile-top-up", "a human pours a clean bottle lying there into its own fuller one", 99,
     [](Stage& s) {
         Human& h = s.first();
         s.world.putDown(s.freeNear(h.cell, 1), 0, {Bottle{0.3f}});
         h.bottles = {Bottle{0.5f}};
         h.hunger = h.thirst = 0.1f;
         s.run(40);
         return Checks{logged({"FILLED MY BOTTLE FROM ONE LYING THERE"})};
     }},
    {"pile-dirty", "holding a bottle, a human leaves a dirty one lying there alone", 99,
     [](Stage& s) {
         Human& h = s.first();
         s.world.putDown(s.freeNear(h.cell, 1), 0, {Bottle{0.9f, 0.4f, 0.0f}});
         h.bottles = {Bottle{0.2f}};
         h.hunger = 0.1f;
         h.thirst = 0.7f;
         s.run(40);
         return Checks{unlogged("FILLED MY BOTTLE FROM ONE LYING THERE"), unlogged("POURED MY BOTTLE INTO A FULLER ONE"),
                       unlogged("PICKED UP A BOTTLE LYING THERE")};
     }},
    {"pile-dirty-parched", "parched, its bottle empty, a human takes a full dirty one lying there", 99,
     [](Stage& s) {
         Human& h = s.first();
         s.world.putDown(s.freeNear(h.cell, 1), 0, {Bottle{0.9f, 0.4f, 0.0f}});
         h.bottles = {Bottle{}};
         h.hunger = 0.1f;
         h.thirst = 0.85f;
         s.run(40);
         return Checks{logged({"LEFT MY EMPTY BOTTLE FOR ONE OF DIRTY WATER"})};
     }},
    {"put-down", "a human kneels and puts cakes and a bottle down", 99,
     [](Stage& s) {
         Human& h = s.first();
         h.cakes = 2;
         h.bottles = {Bottle{1.0f}};
         h.hunger = h.thirst = 0.1f;
         s.run(8);
         P::putDown(h, 2, {0});
         s.run(5);
         return Checks{logged({"PUT DOWN 2 CAKES AND A BOTTLE"})};
     }},
    {"stash", "a grasping loner with full pockets stashes the cakes it can't carry",
     seedWhere([](const Traits& t) { return t.greedy >= 0.8f && t.sociable <= 0.3f; }),
     [](Stage& s) {
         Human& h = s.first();
         P::knowChunk(s.world, h, true);
         h.cakes = h.carryMax();
         h.hunger = h.thirst = h.tiredness = 0.1f;
         s.run(120);
         return Checks{logged({"MADE A STASH"}), loggedLike("STASHED", "")};
     }},
    {"from-stash", "a hoarder, hungry with no cakes on it, eats from its stash", 99,
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         Vec2i at = s.freeNear(h.cell, 4);
         s.world.putDown(at, 3, {});
         P::ownStash(s.world, h, at);
         h.cakes = 0;
         h.hunger = 0.7f;
         h.thirst = 0.1f;
         s.run(40);
         return Checks{logged({"TOOK A CAKE FROM MY STASH"}), logged({"ATE A CAKE"})};
     }},
    {"stash-seen", "a hungry stranger sees a hoarder stash cakes and leaves them be", 99,
     [](Stage& s) {
         Human& owner = s.first();
         P::clearChunk(s.world, owner.cell);
         Human& other = s.add(seedWhere(plain, 17000), 3);
         owner.cakes = 4;
         owner.hunger = owner.thirst = other.thirst = 0.1f;
         other.hunger = 0.6f;
         other.cakes = 0;
         s.run(10);
         P::stashHere(owner);
         s.run(40);
         return Checks{logged({"SAW @ STASH THINGS"}),
                       {"nobody else picked it up", [](const Stage& st) {
                            return !st.logged("PICKED UP 3 CAKES LYING THERE") && !st.logged("PICKED UP 2 CAKES LYING THERE") &&
                                   !st.logged("PICKED UP A CAKE LYING THERE");
                        }}};
     }},
    {"stash-robbed", "a stash is picked over while its owner is away, who finds it short and moves the rest", 99,
     [](Stage& s) {
         Human& owner = s.first();
         P::clearChunk(s.world, owner.cell);
         P::knowChunk(s.world, owner);
         Vec2i at = s.freeNear(owner.cell, 3);
         s.world.putDown(at, 6, {});
         P::ownStash(s.world, owner, at);
         P::place(s.world, owner, s.farFrom(at, 18));
         Human& other = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(at, 1), seedWhere(plain, 18014));
         s.humans.push_back(&other);
         other.cakes = 0;
         other.hunger = 0.6f;
         owner.cakes = 0;
         owner.hunger = 0.45f;
         owner.thirst = other.thirst = 0.1f;
         s.run(90);
         return Checks{logged({"SOMEONE TOOK FROM MY STASH"}), loggedLike("TOOK", "FROM MY STASH TO MOVE IT")};
     }},
    {"steal-stash", "a hungry human steals from a stash it knows is someone's, out of their sight", 99,
     [](Stage& s) {
         stashScene(s, 20, 0.7f, seedWhere([](const Traits& t) { return t.greedy >= 0.4f && t.brave >= 0.4f; }, 19000));
         s.run(60);
         return Checks{loggedLike("TOOK", "FROM @'S STASH")};
     }},
    {"caught-stealing", "a starving human steals from a stash under its owner's eyes, who goes for it", 99,
     [](Stage& s) {
         stashScene(s, 1, 0.92f, seedWhere(plain, 19500));
         s.run(40);
         return Checks{loggedLike("TOOK", "FROM @'S STASH"), logged({"@ TOOK FROM MY STASH"})};
     }},
    {"resent", "a hungry human with nothing sees a big stash and resents its owner", 99,
     [](Stage& s) {
         // A timid, honest one, so it resents rather than steals.
         stashScene(s, 20, 0.7f, seedWhere([](const Traits& t) { return t.greedy < 0.3f && t.brave < 0.3f; }, 20000), 6);
         s.run(30);
         return Checks{logged({"@ HOARDS WHILE I GO WITHOUT"})};
     }},
    {"tell-stash", "a friend tells a hungry friend where someone's stash is", 99,
     [](Stage& s) {
         StashScene sc = stashScene(s, 25, 0.1f, seedWhere(kindly, 21000));
         Human& hungry = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(sc.other->cell, 2), seedWhere(kindly, 21500));
         s.humans.push_back(&hungry);
         P::setTrust(*sc.other, hungry, 0.6f);
         P::setTrust(hungry, *sc.other, 0.6f);
         hungry.hunger = 0.6f;
         hungry.cakes = 0;
         hungry.thirst = 0.1f;
         s.run(40);
         return Checks{logged({"TOLD @ WHERE A STASH IS"}), logged({"@ TOLD ME WHERE A STASH IS"})};
     }},
    {"stash-overheard", "a hoarder overhears its stash told of, and means to move it", 99,
     [](Stage& s) {
         StashScene sc = stashScene(s, 1, 0.1f, seedWhere(kindly, 21000));
         Human& friendOf = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(sc.other->cell, 1), seedWhere(kindly, 21500));
         s.humans.push_back(&friendOf);
         friendOf.hunger = friendOf.thirst = 0.1f;
         // All three keep to where they are; the friend already knows the
         // stash is the owner's, so it leaves it be. The owner minds the
         // telling all the same.
         P::settle(*sc.other, sc.other->cell);
         P::settle(friendOf, friendOf.cell);
         P::knowStash(s.world, friendOf, sc.at, *sc.owner);
         s.run(12);
         P::tellStash(*sc.other, friendOf, sc.at, *sc.owner);
         s.run(15);
         return Checks{logged({"@ TOLD # ABOUT MY STASH"})};
     }},
    {"lie-stash", "a cunning human sets a hungry foe on the stash of someone it dislikes", seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         P::clearChunk(s.world, liar.cell);
         Human& owner = s.add(seedWhere(plain, 22000), 25);
         Vec2i at = s.freeNear(owner.cell, 2);
         s.world.putDown(at, 4, {});
         P::ownStash(s.world, owner, at);
         P::knowStash(s.world, liar, at, owner);
         P::setTrust(liar, owner, -0.3f);
         Human& victim = s.add(seedWhere(plain, 22500));
         setUpLie(s, liar, victim);
         victim.hunger = 0.7f;
         victim.cakes = 0;
         s.run(60);
         return Checks{logged({"SET @ ON #'S STASH"}), logged({"@ TOLD ME WHERE A STASH IS"})};
     }},
    {"false-stash", "with no bare table to send it to, a cunning human tells a hungry foe of a stash that isn't there",
     seedWhere(cunning),
     [](Stage& s) {
         Human& liar = s.first();
         // A cake on every table, and the liar knows it: no false cake tip.
         for (const Table& t : s.world.tables())
             if (World::slotOf(t.at) == World::slotOf(liar.cell)) P::addCake(s.world, t.at);
         P::knowChunk(s.world, liar, true);
         Human& victim = s.add(seedWhere(plain, 23000));
         setUpLie(s, liar, victim);
         victim.hunger = 0.7f;
         victim.cakes = 0;
         s.run(150);
         return Checks{logged({"TOLD @ OF A STASH THAT ISN'T THERE"}), logged({"@ TOLD ME WHERE A STASH IS"})};
     }},
    {"deny-stash", "a hoarder denies its stash to a wary stranger who saw it made, and is seen through",
     seedWhere([](const Traits& t) { return t.greedy >= 0.8f && t.sociable <= 0.3f; }),
     [](Stage& s) {
         StashScene sc = stashScene(s, 1, 0.1f, seedWhere(plain, 24000));
         P::sawStashMade(s.world, *sc.other, sc.at, *sc.owner);
         P::setTrust(*sc.other, *sc.owner, -0.2f);
         s.run(60);
         return Checks{logged({"TOLD @ THE STASH ISN'T MINE"}), logged({"@ DENIED THEIR STASH. I SAW THEM MAKE IT"})};
     }},
    {"foul-memory", "a human who tasted foul water goes to clean water it knows instead, farther off", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto foul = foulAndClean(s, h);
         if (foul.empty()) return Checks{{"a basin near the spawn and floor for clean water", [](const Stage&) { return false; }}};
         // No bottle to drink from or fill: it has to kneel at the water.
         h.bottles.clear();
         h.thirst = 0.7f;
         int closest = keptOff(s, h, foul, 120);
         return Checks{logged({"DRANK WATER"}),
                       {"no foul water drunk",
                        [](const Stage& st) { return !st.logged("DRANK FOUL WATER") && !st.logged("DRANK FOUL WATER AGAIN"); }},
                       {"kept off the foul basin", [closest](const Stage&) { return closest > 2; }}};
     }},
    {"foul-warned", "a human going for water it doesn't know is foul is warned off by one it walks with",
     seedWhere([](const Traits& t) { return t.sociable < 0.3f; }),
     [](Stage& s) {
         Human& knower = s.first();
         Vec2i cleanAt;
         auto foul = foulAndClean(s, knower, &cleanAt);
         if (foul.empty()) return Checks{{"a basin near the spawn and floor for clean water", [](const Stage&) { return false; }}};
         Human& h = s.add(seedWhere(plain, 6000));
         P::knowChunk(s.world, h);
         h.memory.see(cleanAt, Tile::Basin);
         P::setTrust(h, knower, 0.5f);
         P::setTrust(knower, h, 0.5f);
         P::setMet(h, knower, 0.0f);
         P::setMet(knower, h, 0.0f);
         P::keepWith(h, knower);
         h.hunger = 0.1f;
         knower.thirst = 0.1f;
         h.thirst = 0.7f;
         int closest = keptOff(s, h, foul, 120);
         return Checks{logged({"WARNED @ THE WATER IS FOUL"}), logged({"@ WARNED ME THE WATER IS FOUL"}),
                       {"kept off the foul basin", [closest](const Stage&) { return closest > 2; }}};
     }},
    {"toilet-foul", "seeing a toilet fouled, a human needing to go uses the clean one farther off, and finds no flush",
     seedWhere([](const Traits& t) { return (t.sociable + 1.0f - t.brave) / 2.0f < 0.8f; }),
     [](Stage& s) {
         Human& h = s.first();
         auto fronts = foulAndCleanToilet(s, h);
         if (!fronts) return Checks{{"floor for two toilets by the spawn", [](const Stage&) { return false; }}};
         h.bladder = 0.95f;
         h.hunger = h.thirst = 0.1f;
         auto at = stoodAt(s, h, Human::State::Relieving, 60);
         auto [foul, clean] = *fronts;
         return Checks{loggedBy(h, "SAW A FOUL TOILET"), loggedBy(h, "FOUND NO FLUSH"),
                       {"went at the clean toilet, not the foul one",
                        [at, foul, clean](const Stage&) {
                            return std::count(at.begin(), at.end(), clean) && !std::count(at.begin(), at.end(), foul);
                        }},
                       {"its own piss is no news: one SAW A FOUL TOILET", [&h](const Stage&) {
                            return std::count_if(h.log.entries().begin(), h.log.entries().end(),
                                                 [](const auto& e) { return e.what == "SAW A FOUL TOILET"; }) == 1;
                        }}};
     }},
    {"toilet-parched", "a parched human with no water drinks from the toilet it saw clean, not the nearer foul one", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto fronts = foulAndCleanToilet(s, h);
         if (!fronts) return Checks{{"floor for two toilets by the spawn", [](const Stage&) { return false; }}};
         h.bottles.clear();
         h.thirst = 0.9f;
         h.hunger = h.bladder = 0.1f;
         auto at = stoodAt(s, h, Human::State::Drinking, 60);
         auto [foul, clean] = *fronts;
         return Checks{logged({"DRANK FROM A TOILET"}),
                       {"drank at the clean toilet, not the foul one", [at, foul, clean](const Stage&) {
                            return std::count(at.begin(), at.end(), clean) && !std::count(at.begin(), at.end(), foul);
                        }}};
     }},
    {"foul-parched", "a parched human with only foul water it knows drinks it, knowingly", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         foulAll(s.world, h, waterBody(s.world, *b), 0.5f);
         h.bottles.clear();
         h.thirst = 0.9f;
         h.hunger = 0.1f;
         s.run(60);
         return Checks{logged({"DRANK FOUL WATER AGAIN"})};
     }},
    {"chat", "two calm acquaintances together start chatting", 99,
     [](Stage& s) {
         Human& a = s.first();
         Human& b = s.add(seedWhere(kindly, 9000));
         P::setTrust(a, b, 0.3f);
         P::setTrust(b, a, 0.3f);
         P::setMet(a, b, 0.0f);
         P::setMet(b, a, 0.0f);
         for (Human* h : {&a, &b}) h->hunger = h->thirst = h->tiredness = 0.05f;
         s.run(180);
         return Checks{{"a chat started", [](const Stage& st) {
                            for (Human* h : st.humans)
                                if (h->chats > 0) return true;
                            return false;
                        }}};
     }},
    {"shut-in-key", "a hearer with no key gets one later, out of earshot, and goes back to let them out", 99,
     [](Stage& s) {
         Human& caller = shutInFirst(s);
         Human& hearer = s.add(seedWhere(caring, 11000), 5);
         calm(hearer);
         P::setTrust(hearer, caller, 0.6f);
         hearCry(s, caller, {&hearer});
         // Walked off out of earshot, knowing the room.
         P::place(s.world, hearer, s.farFrom(caller.cell, 20));
         P::knowChunk(s.world, hearer);
         hearer.keys = 2;
         s.run(120);
         return Checks{logged({"HEARD @ CALLING FOR HELP"}), logged({"LET @ OUT OF A SLAM ROOM"}),
                       logged({"@ LET ME OUT"})};
     }},
    {"shut-in-told", "a hearer tells a stranger with keys, who never heard the cry, and they let them out", 99,
     [](Stage& s) {
         Human& caller = shutInFirst(s);
         Human& teller = s.add(seedWhere(caring, 12000), 5);
         calm(teller);
         hearCry(s, caller, {&teller});
         Human& listener = s.add(seedWhere(caring, 13000), 2);
         P::place(s.world, teller, s.farFrom(caller.cell, 18));
         P::place(s.world, listener, s.freeNear(teller.cell, 2));
         calm(listener);
         listener.keys = 2;
         P::setTrust(listener, caller, 0.6f);
         s.run(120);
         return Checks{logged({"TOLD @ SOMEONE IS SHUT IN"}), logged({"@ TOLD ME SOMEONE IS SHUT IN"}),
                       logged({"LET @ OUT OF A SLAM ROOM"})};
     }},
    {"parched-lost", "two parched humans boxed in with no water ask each other once, then lie by a door calling",
     seedWhere([](const Traits& t) { return t.sociable >= 0.75f && t.brave >= 0.3f; }, 17000),
     [](Stage& s) {
         Human& leader = s.first();
         Human& follower = s.add(seedWhere([](const Traits& t) { return t.sociable >= 0.75f; }, 17500), 2);
         lostPair(s, leader, follower);
         Check lying = runSeeingLying(s, 120);
         return Checks{logged({"@ KNEW OF NO WATER EITHER"}), askedOnce(),
                       logged({"NO WATER ANYWHERE. LYING BY A LOCKED DOOR, CALLING FOR HELP"}), lying,
                       unlogged("LEFT @. THEY KNOW NO MORE THAN I DO")};
     }},
    {"parched-door", "a caring human with a key, beyond the door the parched lie calling through, opens it",
     seedWhere([](const Traits& t) { return t.sociable >= 0.75f && t.brave >= 0.3f; }, 17696),
     [](Stage& s) {
         Human& leader = s.first();
         Human& follower = s.add(seedWhere([](const Traits& t) { return t.sociable >= 0.75f; }, 17500), 2);
         lostPair(s, leader, follower);
         // Just beyond the room's locked door nearest the leader.
         std::optional<Vec2i> beyond;
         int best = 0;
         Vec2i slot = World::slotOf(leader.cell);
         for (int y = 0; y < CHUNK; y++)
             for (int x = 0; x < CHUNK; x++) {
                 Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
                 if (s.world.tile(c) != Tile::DoorLocked || (beyond && cheb(c, leader.cell) >= best)) continue;
                 for (Vec2i d : DIRS4) {
                     if (World::slotOf(c + d) == slot) continue;
                     // Through the neighbour's wall too, where the door goes on.
                     Vec2i out = c + d;
                     s.world.ensureGenerated(out);
                     while (s.world.tile(out) == Tile::DoorLocked) out = out + d;
                     if (s.world.tile(out) == Tile::Floor && !s.world.occupant(out)) {
                         beyond = out;
                         best = cheb(c, leader.cell);
                     }
                 }
             }
         if (!beyond) return Checks{{"a locked door out of the room", [](const Stage&) { return false; }}};
         Human& keeper = s.add(
             seedWhere([](const Traits& t) { return caring(t) && t.sociable >= 0.75f && t.brave >= 0.6f && t.greedy <= 0.25f; },
                       20000),
             6);
         P::place(s.world, keeper, *beyond);
         calm(keeper);
         keeper.keys = 1;
         P::knowChunk(s.world, keeper);
         P::setTrust(keeper, leader, 0.6f);
         // Given up on a way out, it has no use of its own for the door.
         P::settle(keeper, keeper.cell);
         s.run(160);
         return Checks{logged({"NO WATER ANYWHERE. LYING BY A LOCKED DOOR, CALLING FOR HELP"}),
                       logged({"OPENED THE DOOR @ WAS CALLING THROUGH"}), logged({"@ LET ME OUT"})};
     }},
    {"parched-leave", "a bold, barely sociable parched human leaves a companion who knows of no water either",
     seedWhere([](const Traits& t) { return t.sociable >= 0.75f; }, 18000),
     [](Stage& s) {
         Human& leader = s.first();
         Human& follower =
             s.add(seedWhere([](const Traits& t) { return t.sociable >= 0.4f && t.sociable < 0.5f && t.brave >= 0.7f; },
                             18500),
                   2);
         lostPair(s, leader, follower);
         s.run(60);
         return Checks{logged({"@ KNEW OF NO WATER EITHER"}), logged({"LEFT @. THEY KNOW NO MORE THAN I DO"})};
     }},
    {"bottle-rage", "an erratic, parched human not hungry goes for a stranger's empty bottle and takes it", 99,
     [](Stage& s) {
         Human& holder = s.first();
         P::clearChunk(s.world, holder.cell);
         Human& mad = s.add(seedWhere([](const Traits& t) { return t.brave >= 0.6f; }, 19000), 3);
         calm(holder);
         holder.bottles = {Bottle{}};
         mad.hunger = mad.tiredness = 0.1f;
         mad.thirst = 0.85f;
         mad.bottles.clear();
         P::unhinge(mad);
         s.run(40);
         return Checks{logged({"WENT FOR @'S BOTTLE, FULL OR NOT"}), logged({"HIT @ AND TOOK THEIR BOTTLE"})};
     }},
    {"shut-in-greedy", "a greedy hearer with keys neither goes to let them out nor tells anyone", 99,
     [](Stage& s) {
         Human& caller = shutInFirst(s);
         Human& hearer = s.add(seedWhere([](const Traits& t) { return t.greedy >= 0.72f; }, 14000), 5);
         calm(hearer);
         hearer.keys = 2;
         P::setTrust(hearer, caller, 0.6f);
         hearCry(s, caller, {&hearer});
         Human& other = s.add(seedWhere(caring, 15000), 2);
         P::place(s.world, hearer, s.farFrom(caller.cell, 18));
         P::place(s.world, other, s.freeNear(hearer.cell, 2));
         calm(other);
         s.run(90);
         return Checks{logged({"HEARD @ CALLING FOR HELP"}), notLogged({"GOING TO LET @ OUT", "GOING TO CALL BACK TO @"}),
                       notLogged({"TOLD @ SOMEONE IS SHUT IN"}), logged({"MET @", "CAUGHT UP WITH @"})};
      }},
    {"lesson-again", "going parched with no water: learnt anew, again, or for good, as the lesson was held", 1,
     [](Stage& s) {
         // Far from the spawn basin, knowing no water, all about to go
         // parched: one never taught, one half taught, one fully.
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         Human& fresh = s.first();
         P::place(s.world, fresh, s.farFrom(*b, 25));
         Human& half = s.add(seedWhere(plain, 11000));
         Human& full = s.add(seedWhere(plain, 12000));
         half.lessons.nearWater = 0.6f;
         full.lessons.nearWater = 1.0f;
         for (Human* h : {&fresh, &half, &full}) {
             h->thirst = 0.799f; // just short of PARCHED
             h->hunger = h->tiredness = 0.1f;
         }
         s.run(5);
         return Checks{loggedBy(fresh, "LEARNT TO KEEP NEAR WATER"),
                       loggedBy(half, "LEARNT FOR GOOD TO KEEP NEAR WATER"),
                       loggedBy(half, "LEARNT TO KEEP NEAR WATER", false),
                       loggedBy(full, "LEARNT AGAIN TO KEEP NEAR WATER"),
                       loggedBy(full, "LEARNT TO KEEP NEAR WATER", false)};
     }},
    {"quenched", "the first drink after thirst hurt it is a narrow escape; a plain thirsty drink is not", 1,
     [](Stage& s) {
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         Human& dying = s.first();
         Human& dry = s.add(seedWhere(plain, 13000));
         dying.thirst = 1.0f;
         dying.health = 0.8f;
         dry.thirst = 0.7f;
         for (Human* h : {&dying, &dry}) h->hunger = h->tiredness = 0.1f;
         s.run(60);
         return Checks{loggedBy(dying, "DRANK, AFTER NEARLY DYING OF THIRST"), loggedBy(dry, "DRANK WATER"),
                       loggedBy(dry, "DRANK, AFTER NEARLY DYING OF THIRST", false)};
     }},
    {"vend-cake", "hungry with three keys and no cake about, it buys one from a machine and eats it",
     seedWhere(plain, 14000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         P::standMachine(s.world, Vend::Cake, h.cell + Vec2i{4, -3}, {0, 1}, 3);
         h.hunger = 0.6f;
         h.thirst = h.tiredness = 0.1f;
         h.keys = MACHINE_PRICE;
         s.run(30);
         return Checks{loggedBy(h, "PAID 3 KEYS FOR A CAKE FROM A MACHINE"), loggedBy(h, "ATE A CAKE"),
                       {"its keys spent", [&h](const Stage&) { return h.keys == 0; }}};
     }},
    {"vend-bottle", "thirsty with no bottle and three keys, it buys an empty one from a machine side on",
     seedWhere(plain, 15000), [](Stage& s) {
         Human& h = s.first();
         P::standMachine(s.world, Vend::Bottle, h.cell + Vec2i{5, -1}, {-1, 0}, 2);
         h.bottles.clear();
         P::setDrankYet(h);
         h.thirst = 0.6f;
         h.hunger = h.tiredness = 0.1f;
         h.keys = MACHINE_PRICE;
         s.run(30);
         return Checks{loggedBy(h, "PAID 3 KEYS FOR A BOTTLE FROM A MACHINE"),
                       {"a bottle in hand", [&h](const Stage&) { return h.bottles.size() == 1; }}};
     }},
    {"vend-empty", "two humans work one machine with one cake left at once: one gets it, one finds it empty",
     seedWhere(plain, 18000), [](Stage& s) {
         Human& a = s.first();
         P::clearChunk(s.world, a.cell);
         const Machine& m = P::standMachine(s.world, Vend::Cake, a.cell + Vec2i{4, -3}, {0, 1}, 1);
         Human& b = s.add(seedWhere(plain, 19000), 3);
         for (Human* h : {&a, &b}) calm(*h);
         s.run(8);
         P::place(s.world, a, m.at + m.facing);
         P::place(s.world, b, m.at + m.along() + m.facing);
         for (Human* h : {&a, &b}) {
             h->keys = MACHINE_PRICE;
             P::knowMachine(*h, m);
         }
         bool started = P::startVending(s.world, a, Vend::Cake) && P::startVending(s.world, b, Vend::Cake);
         s.run(5);
         return Checks{{"both started", [started](const Stage&) { return started; }},
                       logged({"PAID 3 KEYS FOR A CAKE FROM A MACHINE"}), logged({"THE MACHINE WAS EMPTY"})};
     }},
    {"sell-vending", "a grasping stranger sells word of a bottle machine to one with no bottle and cakes to spare",
     seedWhere([](const Traits& t) { return t.greedy >= 0.55f && t.brave < 0.35f; }, 20000), [](Stage& s) {
         Human& seller = s.first();
         P::clearChunk(s.world, seller.cell);
         const Machine& m = P::standMachine(s.world, Vend::Bottle, seller.cell + Vec2i{0, -24}, {0, 1}, 4);
         Human& buyer = s.add(seedWhere([](const Traits& t) { return t.greedy < 0.6f && t.sociable >= 0.4f; }, 21000), 3);
         P::knowChunk(s.world, seller);
         P::knowMachine(seller, m);
         calm(seller);
         buyer.hunger = buyer.thirst = buyer.tiredness = 0.1f;
         buyer.bottles.clear();
         P::setDrankYet(buyer);
         buyer.keys = MACHINE_PRICE;
         buyer.cakes = 3;
         s.run(60);
         return Checks{loggedBy(seller, "TOLD @ WHERE A MACHINE IS"), loggedBy(buyer, "PAID @ A CAKE FOR NEWS")};
     }},
    {"tell-vending", "a friend tells a hungry friend with keys of a machine out of sight, who goes and buys",
     seedWhere(kindly, 16000), [](Stage& s) {
         Human& teller = s.first();
         P::clearChunk(s.world, teller.cell);
         const Machine& m = P::standMachine(s.world, Vend::Cake, teller.cell + Vec2i{0, -24}, {0, 1}, 4);
         Human& hungry = s.add(seedWhere(kindly, 17000), 3);
         P::knowChunk(s.world, teller);
         P::knowMachine(teller, m);
         P::setTrust(teller, hungry, 0.6f);
         P::setTrust(hungry, teller, 0.6f);
         calm(teller);
         hungry.hunger = 0.6f;
         hungry.thirst = hungry.tiredness = 0.1f;
         hungry.keys = MACHINE_PRICE;
         s.run(90);
         return Checks{loggedBy(teller, "TOLD @ WHERE A MACHINE IS"), loggedBy(hungry, "@ TOLD ME WHERE A MACHINE IS"),
                       loggedBy(hungry, "PAID 3 KEYS FOR A CAKE FROM A MACHINE")};
     }},
    {"dirty-wash", "turning dirty, the modest wash it off and the shameless don't", seedWhere(modest, 12500),
     [](Stage& s) {
         return washOrNot(s, [](Human& h) { P::setGrime(h, 0.97f); }, [](const Human& prim, const Human& slob) {
             return Checks{own(prim, "TURNED DIRTY"), own(prim, "WASHED OFF DIRT"), own(prim, "WASHED CLEAN"),
                           own(slob, "TURNED DIRTY"), own(slob, "WASHED OFF DIRT", false),
                           {"#0 done washing", [&prim](const Stage&) { return prim.state != Human::State::Washing; }}};
         });
     }},
    {"dry-blood", "with dried blood on, the modest wash it off and the shameless don't", seedWhere(modest, 12500),
     [](Stage& s) {
         return washOrNot(s, [](Human& h) { P::bloody(h), P::dry(h); }, [](const Human& prim, const Human& slob) {
             return Checks{own(prim, "WASHED OFF BLOOD"), own(slob, "WASHED OFF BLOOD", false)};
         });
     }},
    {"fresh-blood", "with fresh blood on, even the shameless wash it off", seedWhere(modest, 12500),
     [](Stage& s) {
         return washOrNot(
             s, [](Human& h) { P::bloody(h); },
             [](const Human& prim, const Human& slob) {
                 return Checks{own(prim, "WASHED OFF BLOOD"), own(slob, "WASHED OFF BLOOD")};
             },
             shamelessMeek);
     }},
    {"stink", "dirty long enough, a human starts to smell", seedWhere(shameless, 12000),
     [](Stage& s) {
         Human& h = s.first();
         calm(h);
         P::setGrime(h, 1.45f);
         s.run(60);
         return Checks{own(h, "TURNED DIRTY"), own(h, "STARTED TO SMELL")};
     }},
    {"put-off", "hungry beside someone who smells, a human walks off to eat", seedWhere(modest, 12500),
     [](Stage& s) {
         Human& eater = s.first();
         Human& smelly = s.add(seedWhere(shameless, 12000), 1);
         calm(eater);
         calm(smelly);
         eater.hunger = 0.75f;
         eater.cakes = 1;
         P::setGrime(smelly, 2.8f);
         s.run(40);
         return Checks{own(smelly, "STARTED TO SMELL"), own(eater, "MOVED AWAY FROM A SMELL TO EAT"),
                       own(eater, "ATE A CAKE")};
     }},
    {"smell-death", "a body walled in out of sight is smelt through the wall", 1,
     [](Stage& s) {
         Human& nose = s.first();
         Human& victim = s.add(seedWhere(plain, 9900));
         calm(nose);
         Vec2i at = nose.cell + Vec2i{3, 0};
         P::wallIn(s.world, at);
         P::place(s.world, victim, at);
         P::kill(s.world, victim, 1200.0f);
         s.run(20);
         return Checks{own(nose, "SMELT DEATH"), unlogged("FOUND @ DEAD")};
     }},
    {"follow-stench", "an erratic human hungry for flesh follows a smell of death to the body",
     seedWhere([](const Traits& t) { return t.brave >= 0.6f; }, 13000),
     [](Stage& s) {
         Human& nose = s.first();
         Human& victim = s.add(seedWhere(plain, 9900));
         calm(nose);
         Vec2i at = nose.cell + Vec2i{3, 0};
         // A way in on the far side.
         P::wallIn(s.world, at, Vec2i{2, 0});
         P::place(s.world, victim, at);
         P::kill(s.world, victim, 1200.0f);
         P::craze(nose, 1.0f);
         nose.hunger = 0.8f;
         nose.cakes = 0;
         s.run(60);
         return Checks{own(nose, "SMELT DEATH"), own(nose, "FOLLOWED THE SMELL OF DEATH"),
                       logged({"ATE FROM @'S ROTTEN BODY", "ATE FROM @'S BODY"})};
     }},
    {"waking-alone", "coming to alone, it mutters with its eyes shut, sits up, and steadies itself getting up",
     seedWhere(plain, 9720),
     [](Stage& s) {
         // The fog's remarks, not discoveries looked at.
         P::seenItAll(s.first());
         s.run(40);
         return Checks{saidAround(s.first(), "WOKE UP, REMEMBERING NOTHING", false),
                       loggedBy(s.first(), "GOT UP, STILL IN A FOG"),
                       saidAround(s.first(), "GOT UP, STILL IN A FOG", true),
                       remarked(s.first(), talk::Moment::FirstTable), remarked(s.first(), talk::Moment::EmptyRoom),
                       loggedBy(s.first(), "TRIED TO REMEMBER ANYTHING FROM BEFORE. NOTHING")};
     },
     nullptr, true},
    {"quiet", "alone and finding nothing new for a while in its first minutes, it remarks on the quiet",
     seedWhere(plain, 9850),
     [](Stage& s) {
         calm(s.first());
         s.run(180);
         return Checks{loggedBy(s.first(), "ALONE IN THE QUIET")};
     },
     nullptr, true},
    {"it-goes-on", "leaving the place it woke in, it sees there is more", seedWhere(plain, 9850),
     [](Stage& s) {
         calm(s.first());
         s.run(150);
         return Checks{loggedBy(s.first(), "LEFT THE PLACE I WOKE IN. THERE'S MORE OF IT")};
     },
     nullptr, true},
    {"never-ends", "five places on, the next one like the last lifts its hope no more", seedWhere(plain, 9850),
     [](Stage& s) {
         calm(s.first());
         P::beenIn(s.first(), 5);
         s.run(150);
         return Checks{loggedBy(s.first(), "ANOTHER PLACE LIKE THE LAST. IT NEVER ENDS"),
                       loggedBy(s.first(), "LEFT THE PLACE I WOKE IN. THERE'S MORE OF IT", false)};
     },
     nullptr, true},
    {"call-answered", "stepping out alone, it calls; a sociable human out of sight calls back, and it hears",
     seedWhere([](const Traits& t) { return t.brave >= 0.5f; }, 9950),
     [](Stage& s) {
         return callStage(s, [](const Traits& t) { return t.sociable >= 0.7f && t.brave >= 0.4f; }, 20000,
                          [](Human& caller, Human& heard) {
                              return Checks{loggedBy(caller, "CALLED OUT INTO THE MALL"),
                                            loggedBy(heard, "CALLED BACK"),
                                            loggedBy(caller, "MY CALL WAS ANSWERED")};
                          });
     }},
    {"call-follow", "a curious, unsociable human hears a call from out of sight and goes toward the voice",
     seedWhere([](const Traits& t) { return t.brave >= 0.5f; }, 9950),
     [](Stage& s) {
         return callStage(s, [](const Traits& t) { return t.curious >= 0.7f && t.sociable < 0.5f && t.brave >= 0.4f; },
                          20000, [](Human&, Human& heard) {
                              return Checks{loggedBy(heard, "HEARD SOMEONE CALL OUT"),
                                            loggedBy(heard, "WENT TOWARD THE VOICE")};
                          });
     }},
    {"call-timid", "a timid human hears a call from out of sight, hushes, and doesn't go to it",
     seedWhere([](const Traits& t) { return t.brave >= 0.5f; }, 9950),
     [](Stage& s) {
         return callStage(s, [](const Traits& t) { return t.brave < 0.25f; }, 20000, [](Human&, Human& heard) {
             return Checks{loggedBy(heard, "HEARD SOMEONE CALL OUT"), loggedBy(heard, "WENT TOWARD THE VOICE", false),
                           loggedBy(heard, "CALLED BACK", false)};
         });
     }},
    {"lead-drink", "a friend that knows water leads a parched friend with none there",
     seedWhere([](const Traits& t) { return t.sociable >= 0.6f; }, 9900),
     [](Stage& s) { return leadToDrink(s, false, {loggedBy(s.first(), "TOOK @ TO WATER TO DRINK")}); }, nullptr},
    {"lead-sleeper", "a friend that knows water lets a parched friend sleep rather than lead it",
     seedWhere([](const Traits& t) { return t.sociable >= 0.6f; }, 9900),
     [](Stage& s) { return leadToDrink(s, true, {loggedBy(s.first(), "TOOK @ TO WATER TO DRINK", false)}); },
     nullptr},
    {"waking-statue", "still dazed, it remarks on a giant statue in the room it woke in", seedWhere(plain, 9800),
     [](Stage& s) {
         Human& h = s.first();
         bool stood = false;
         for (Vec2i d : {Vec2i{4, 0}, Vec2i{-4, 0}, Vec2i{0, 4}, Vec2i{0, -4}, Vec2i{4, 4}, Vec2i{-4, -4}})
             if ((stood = P::placeStatue(s.world, h.cell + d, h, StatueKind::Clock))) break;
         if (!stood) return Checks{{"room for a statue in the spawn", [](const Stage&) { return false; }}};
         s.run(30);
         return Checks{remarked(h, talk::Moment::FirstStatue)};
     },
     nullptr, true},
    {"hum-sleep", "tired with two keys and a server room it knows, it opens both doors and sleeps by the machines",
     seedWhere(plain, 20000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         P::knowServerRoom(h, r);
         h.tiredness = 0.6f;
         h.hunger = h.thirst = 0.1f;
         h.keys = 2;
         s.run(60);
         return Checks{loggedBy(h, "WENT TO SLEEP AMONG THE HUMMING MACHINES"),
                       loggedBy(h, "UNLOCKED A DOOR TO THE HUMMING MACHINES"),
                       loggedBy(h, "THE MACHINES HUMMED IT TO SLEEP"),
                       {"both keys spent", [&h](const Stage&) { return h.keys == 0; }},
                       {"asleep inside", [&h, &r](const Stage&) {
                            return h.state == Human::State::Sleeping && r.contains(h.cell);
                        }}};
     }},
    {"hum-out", "inside a server room with no keys, it remarks on the machines, pushes its way out to drink, "
                "and the doors lock behind it",
     seedWhere(plain, 21000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const Vec2i outside = h.cell;
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -1});
         P::addWater(s.world, outside);
         h.memory.see(outside, Tile::Basin);
         P::fillDispenser(s.world, r, 0.0f);
         h.bottles.clear();
         h.keys = 0;
         h.thirst = 0.6f;
         h.hunger = h.tiredness = 0.1f;
         s.run(60);
         return Checks{loggedBy(h, "FOUND A ROOM OF HUMMING MACHINES"), loggedBy(h, "DRANK WATER"),
                       {"out of the room", [&h, &r](const Stage&) { return !r.encloses(h.cell); }},
                       {"both doors locked again", [&r](const Stage& st) {
                            return st.world.tile(r.inner) == Tile::DoorLocked &&
                                   st.world.tile(r.outer) == Tile::DoorLocked;
                        }}};
     }},
    {"hum-lobby", "with one key it gets through the outer door, finds the inner one locked, and pushes back out",
     seedWhere(plain, 22000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.outer + Vec2i{0, 1});
         h.keys = 1;
         h.memory.see(r.outer, Tile::DoorLocked);
         h.memory.airlocks[r.outer] = {0, -1};
         bool opened = s.world.unlock(r.outer);
         h.hunger = h.thirst = h.tiredness = 0.1f;
         s.run(60);
         return Checks{{"the outer door opened", [opened](const Stage&) { return opened; }},
                       {"never inside the room", [&h, &r](const Stage&) { return !r.contains(h.cell); }},
                       {"not stuck in the lobby", [&h, &r](const Stage&) { return !r.encloses(h.cell); }}};
     }},
    {"dark-sight", "in the dark it makes out only what is close round it, but a cake under a light across the room",
     seedWhere(plain, 26000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         // Past the waking fog, which narrows sight too.
         s.run(45);
         P::darkenChunk(s.world, h.cell);
         const Vec2i near = s.freeNear(h.cell, 3), dark = s.freeNear(h.cell, 9), lit = s.farFrom(h.cell, 14);
         for (Vec2i c : {near, dark, lit}) P::addCake(s.world, c);
         P::hangLight(s.world, lit);
         s.run(0.5f);
         return Checks{{"sees the cake beside it", [&h, near](const Stage&) { return h.memory.cakes.count(near) > 0; }},
                       {"not the one in the dark", [&h, dark](const Stage&) { return h.memory.cakes.count(dark) == 0; }},
                       {"sees the lit one", [&h, lit](const Stage&) { return h.memory.cakes.count(lit) > 0; }}};
     }},
    {"dark-fear", "the timid, put in the dark, marks it and grows uneasy; the brave hardly",
     seedWhere([](const Traits& t) { return t.brave < 0.2f; }, 27000), [](Stage& s) {
         Human& timid = s.first();
         Human& brave = s.add(seedWhere([](const Traits& t) { return t.brave > 0.8f; }, 27500), 3);
         P::clearChunk(s.world, timid.cell);
         calm(timid);
         calm(brave);
         s.run(45);
         P::setDread(timid, 0.0f);
         P::setDread(brave, 0.0f);
         P::darkenChunk(s.world, timid.cell);
         s.run(20);
         return Checks{loggedBy(timid, "STOOD IN THE DARK"),
                       {"the timid uneasy", [&timid](const Stage&) { return timid.dread() > 0.15f; }},
                       {"the brave less so", [&timid, &brave](const Stage&) { return brave.dread() < timid.dread() / 2; }}};
     }},
    {"toward-light", "timid in the dark, it makes for a light it sees rather than feel its way on",
     seedWhere([](const Traits& t) { return t.brave < 0.2f; }, 28000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         P::darkenChunk(s.world, h.cell);
         P::forget(h);
         P::hangLight(s.world, s.farFrom(h.cell, 12));
         s.run(20);
         return Checks{loggedBy(h, "MADE FOR THE LIGHT")};
     }},
    {"sleep-dark", "tired and bold, it lies down in the dark half of the room, out of sight",
     seedWhere([](const Traits& t) { return t.brave > 0.85f; }, 29000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         // The light from the lit half reaches a couple of cells past it, so dark lies close by.
         const int edge = h.cell.x + 5;
         P::putOutLights(s.world, [edge](Vec2i c) { return c.x < edge; });
         P::forget(h);
         h.tiredness = 0.6f;
         s.run(30);
         return Checks{loggedBy(h, "LAY DOWN IN THE DARK, OUT OF SIGHT")};
     }},
    {"sleep-lit", "tired and timid, it lies down in the lit half of the room, where it can see",
     seedWhere([](const Traits& t) { return t.brave < 0.15f; }, 29500), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         const int edge = h.cell.x;
         P::putOutLights(s.world, [edge](Vec2i c) { return c.x < edge; });
         P::forget(h);
         h.tiredness = 0.6f;
         s.run(30);
         return Checks{loggedBy(h, "LAY DOWN WHERE THE LIGHT IS")};
     }},
    {"relief-dark", "modest, needing to go, it goes in the dark half of the room, where nobody would see",
     seedWhere(modest, 30000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         const int edge = h.cell.x + 5;
         P::putOutLights(s.world, [edge](Vec2i c) { return c.x < edge; });
         P::forget(h);
         h.bladder = 0.95f;
         s.run(30);
         return Checks{loggedBy(h, "WENT WHERE THE DARK HID ME")};
     }},
    {"stash-dark", "a grasping loner with full pockets hides its stash in the dark half of the room",
     seedWhere([](const Traits& t) { return t.greedy >= 0.8f && t.sociable <= 0.3f; }, 30500), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         const int edge = h.cell.x + 5;
         P::putOutLights(s.world, [edge](Vec2i c) { return c.x < edge; });
         P::forget(h);
         h.cakes = h.carryMax();
         // More to be had in plain sight, on the lit side, than it can carry.
         P::addCake(s.world, s.freeNear(h.cell + Vec2i{8, 0}, 0));
         s.run(120);
         return Checks{loggedBy(h, "HID IT IN THE DARK")};
     }},
    {"stepped-dark", "hungry in the dark, walking out of a box to a lit cake, it steps in feces it never saw",
     seedWhere(plain, 31000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         s.run(45);
         P::darkenChunk(s.world, h.cell);
         P::wallIn(s.world, h.cell, Vec2i{2, 0});
         P::dropFeces(s.world, h.cell + Vec2i{2, 0});
         const Vec2i cake = h.cell + Vec2i{12, 0};
         P::addCake(s.world, cake);
         P::hangLight(s.world, cake);
         h.hunger = 0.7f;
         s.run(20);
         return Checks{loggedBy(h, "STEPPED IN SHIT IN THE DARK")};
     }},
    {"closing-bell", "at midnight the bell rings and the ceiling lights go out; awake, it is uneasy",
     seedWhere([](const Traits& t) { return t.brave < 0.3f; }, 32000), [](Stage& s) {
         Human& h = s.first();
         calm(h);
         s.run(45);
         const Vec2i at = h.cell;
         const bool litBefore = s.world.light(at) > 0;
         P::setDread(h, 0.0f);
         P::setTime(s.world, (MALL_HOURS - OPENING) * MALL_HOUR - 2.0f);
         s.run(4);
         return Checks{loggedBy(h, "THE BELL RANG AND THE LIGHTS WENT OUT"),
                       {"lit before, closed now", [&s, litBefore](const Stage&) { return litBefore && s.world.closed(); }},
                       {"uneasy", [&h](const Stage&) { return h.dread() > 0.05f; }}};
     }},
    {"opening-bell", "asleep in the night, not worn through, it wakes to the opening bell",
     seedWhere(plain, 32500), [](Stage& s) {
         Human& h = s.first();
         calm(h);
         s.run(45);
         P::setTime(s.world, MALL_DAY - 3.0f);
         s.run(1);
         h.tiredness = 0.45f;
         P::sleepNow(s.world, h);
         s.run(6);
         return Checks{loggedBy(h, "WOKE TO THE BELL"), {"open again", [&s](const Stage&) { return !s.world.closed(); }}};
     }},
    {"closing-sleep", "a little tired, it would go on; once the mall closes, it lies down",
     seedWhere(plain, 33000), [](Stage& s) {
         Human& h = s.first();
         calm(h);
         s.run(45);
         h.tiredness = 0.42f;
         s.run(10);
         const bool sleptOpen = h.state == Human::State::Sleeping;
         P::setTime(s.world, (MALL_HOURS - OPENING) * MALL_HOUR - 1.0f);
         s.run(40);
         return Checks{{"awake while open", [sleptOpen](const Stage&) { return !sleptOpen; }},
                       loggedBy(h, "FELL ASLEEP")};
     }},
    {"clock-lesson", "the second time the lights go out at the bell, it knows the mall keeps hours",
     seedWhere(plain, 33500), [](Stage& s) {
         Human& h = s.first();
         calm(h);
         s.run(45);
         for (int night = 1; night <= 2; night++) {
             P::setTime(s.world, night * MALL_DAY - OPENING * MALL_HOUR - 1.0f);
             s.run(3);
             calm(h);
         }
         return Checks{loggedBy(h, "THE LIGHTS GO OUT AT THE BELL. EVERY TIME")};
     }},
    {"airlock-surprise", "with keys and nowhere else to go, it opens a server room's outer door and finds another behind",
     seedWhere(plain, 23000), [](Stage& s) {
         Human& h = s.first();
         const ServerRoom& r = airlockPocket(s, h, 0);
         s.run(40);
         return Checks{loggedBy(h, "ANOTHER LOCKED DOOR BEHIND THE FIRST"), notLogged({"LEARNT THE DOUBLE DOORS EAT KEYS"}),
                       {"a key spent on it", [&h](const Stage&) { return h.keys < 3; }},
                       {"not stuck in the lobby", [&h, &r](const Stage&) { return !r.encloses(h.cell) || r.contains(h.cell); }}};
     }},
    {"airlock-learn", "the second time it finds a locked door behind the one it opened, it learns the double doors eat keys",
     seedWhere(plain, 24000), [](Stage& s) {
         Human& h = s.first();
         airlockPocket(s, h, 1);
         s.run(40);
         return Checks{loggedBy(h, "LEARNT THE DOUBLE DOORS EAT KEYS"),
                       {"knows it now", [&h](const Stage&) { return P::keyWise(h); }}};
     }},
    {"airlock-spare", "key-wise and incurious, with nowhere else to go, it lets the double doors be and keeps its keys",
     seedWhere([](const Traits& t) { return t.curious < 0.4f; }, 25000), [](Stage& s) {
         Human& h = s.first();
         const ServerRoom& r = airlockPocket(s, h, 2);
         bool entered = false;
         for (int t = 0; t < 40; t++) {
             s.run(1);
             entered |= r.encloses(h.cell);
         }
         return Checks{loggedBy(h, "LET THE DOUBLE DOORS BE, KEYS KEPT"),
                       {"keys kept", [&h](const Stage&) { return h.keys == 3; }},
                       {"never went through", [entered](const Stage&) { return !entered; }}};
     }},
    {"airlock-dare", "key-wise but curious, with nowhere else to go, it opens the double doors anyway",
     seedWhere([](const Traits& t) { return t.curious >= 0.75f; }, 26000), [](Stage& s) {
         Human& h = s.first();
         airlockPocket(s, h, 2);
         s.run(40);
         return Checks{loggedBy(h, "OPENED THE DOUBLE DOORS ANYWAY")};
     }},
    {"airlock-tell", "a key-wise human tells a friend with keys what the double doors cost",
     seedWhere(kindly, 27000), [](Stage& s) {
         Human& teller = s.first();
         Human& told = s.add(seedWhere(kindly, 27500), 2);
         P::setTrust(teller, told, 0.6f);
         P::setTrust(told, teller, 0.6f);
         calm(teller);
         calm(told);
         P::metAirlock(teller, 2);
         told.keys = 2;
         s.run(40);
         return Checks{loggedBy(teller, "TOLD @ THE DOUBLE DOORS EAT KEYS"),
                       loggedBy(told, "@ TOLD ME THE DOUBLE DOORS EAT KEYS"),
                       {"the told knows it", [&told](const Stage&) { return P::keyWise(told); }}};
     }},
    {"hum-stay", "inside a server room the first time, drowsy, it feels the hum and sleeps there rather than leave",
     seedWhere(plain, 28000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -1});
         h.keys = 2;
         h.tiredness = 0.4f;
         h.hunger = h.thirst = 0.1f;
         s.run(60);
         return Checks{loggedBy(h, "THE HUM IN HERE IS MADE FOR SLEEP"),
                       loggedBy(h, "SLEEPING HERE RATHER THAN PAY TO COME BACK"),
                       {"asleep inside", [&h, &r](const Stage&) {
                            return h.state == Human::State::Sleeping && r.contains(h.cell);
                        }}};
     }},
    {"cooler-drink", "thirsty in a server room, it drinks from the water dispenser there before a basin just outside",
     seedWhere(plain, 23000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const Vec2i outside = h.cell;
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -1});
         P::addWater(s.world, outside);
         h.memory.see(outside, Tile::Basin);
         const Dispenser& d = P::fillDispenser(s.world, r, DISPENSER_HOLDS);
         h.bottles.clear();
         h.thirst = 0.6f;
         h.hunger = h.tiredness = 0.1f;
         s.run(30);
         return Checks{loggedBy(h, "FOUND A WATER DISPENSER"), loggedBy(h, "DRANK FROM A WATER DISPENSER"),
                       loggedBy(h, "DRANK WATER", false),
                       {"the dispenser drunk from", [&d](const Stage&) { return d.water < DISPENSER_HOLDS; }}};
     }},
    {"cooler-dry", "parched at a dispenser with little left, it drinks it dry, finds it so, and goes out to a basin",
     seedWhere(plain, 24000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const Vec2i outside = h.cell;
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -1});
         P::addWater(s.world, outside);
         h.memory.see(outside, Tile::Basin);
         const Dispenser& d = P::fillDispenser(s.world, r, 0.4f);
         h.bottles.clear();
         h.thirst = 0.85f;
         h.hunger = h.tiredness = 0.1f;
         s.run(60);
         return Checks{loggedBy(h, "DRANK FROM A WATER DISPENSER"), loggedBy(h, "FOUND THE WATER DISPENSER DRY"),
                       loggedBy(h, "DRANK WATER"),
                       {"the dispenser dry", [&d](const Stage&) { return d.water <= 0.0f; }}};
     }},
    {"cooler-fill", "not thirsty with an empty bottle in a server room, it fills it at the dispenser, draining it",
     seedWhere(plain, 25000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -1});
         P::seenItAll(h);
         const Dispenser& d = P::fillDispenser(s.world, r, 2.0f);
         h.bottles.assign(1, Bottle{});
         s.run(40);
         return Checks{loggedBy(h, "FILLED MY BOTTLE AT A WATER DISPENSER"),
                       {"the bottle full", [&h](const Stage&) { return !h.bottles.empty() && h.bottles[0].full(); }},
                       {"a bar gone from the dispenser", [&d](const Stage&) { return d.water <= 1.05f; }}};
     }},
    {"cooler-keep", "having learnt water is scarce, it sleeps among the machines, then pours its bottle into the "
                    "dispenser there",
     seedWhere(plain, 26000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -2});
         P::knowServerRoom(h, r);
         const Dispenser& d = P::fillDispenser(s.world, r, 1.0f);
         P::knowDispenser(h, d);
         P::sawScarce(h);
         h.bottles.assign(1, Bottle{1.0f});
         h.tiredness = 0.75f;
         h.hunger = h.thirst = 0.05f;
         s.run(400);
         return Checks{loggedBy(h, "THE MACHINES HUMMED IT TO SLEEP"),
                       loggedBy(h, "MEANS TO KEEP WATER HERE, BEHIND THE LOCKED DOORS"),
                       loggedBy(h, "POURED MY BOTTLE INTO THE WATER DISPENSER"),
                       {"the dispenser fuller", [&d](const Stage&) { return d.water > 1.5f; }}};
     }},
    {"cooler-home", "settled by a dispenser, it pours its bottle in to keep its home water",
     seedWhere(plain, 27000), [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         const ServerRoom& r = P::buildServerRoom(s.world, h.cell + Vec2i{-5, -17}, 10, 8);
         calm(h);
         s.run(8);
         P::place(s.world, h, r.inner + Vec2i{0, -2});
         P::knowServerRoom(h, r);
         const Dispenser& d = P::fillDispenser(s.world, r, 0.5f);
         P::knowDispenser(h, d);
         P::settle(h, d.at + d.facing);
         h.bottles.assign(1, Bottle{0.8f});
         s.run(40);
         return Checks{loggedBy(h, "POURED MY BOTTLE INTO THE WATER DISPENSER"),
                       {"the bottle poured in", [&d](const Stage&) { return d.water > 1.2f; }}};
     }},
    {"inspect", "a curious human walks up to the first cake it sees, stands, and remarks on it there",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         Vec2i cake = s.freeNear(h.cell, 5);
         P::addCake(s.world, cake);
         calm(h);
         s.run(40);
         return Checks{loggedBy(h, "WENT CLOSER TO LOOK AT A CAKE"), saidAround(h, "WENT CLOSER TO LOOK AT A CAKE", true),
                       {"nothing left to look at", [&h](const Stage&) { return P::findsKept(h) == 0; }}};
     }},
    {"computer-terminal", "a curious human walks up to the first terminal it sees, types at it and finds a password",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         computerNear(s, Rig::Terminal, 4);
         calm(h);
         s.run(40);
         return Checks{loggedBy(h, "STEPPED UP TO A TERMINAL TO TYPE"),
                       loggedBy(h, "TRIED A TERMINAL. LOCKED BY A PASSWORD"),
                       saidAround(h, "TRIED A TERMINAL. LOCKED BY A PASSWORD", true)};
     }},
    {"computer-desk", "a curious human takes the chair at the first console desk it sees and finds it locked too",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9100),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         Vec2i at = computerNear(s, Rig::Desk, 3);
         calm(h);
         auto chair = std::make_shared<bool>(false);
         s.run(40, [&] { *chair |= h.typingAt() == at && ServerPiece{Rig::Desk, at}.usedFrom(h.cell); });
         return Checks{loggedBy(h, "TOOK THE CHAIR AT THE CONSOLE DESK TO TYPE"),
                       loggedBy(h, "TRIED THE CONSOLE DESK. LOCKED BY A PASSWORD"),
                       saidAround(h, "TRIED THE CONSOLE DESK. LOCKED BY A PASSWORD", true),
                       {"typed from the chair", [chair](const Stage&) { return *chair; }}};
     }},
    {"computer-hungry", "a human typing at a terminal leaves it mid-try once hunger comes on, and never comes back to it",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         computerNear(s, Rig::Terminal, 4);
         calm(h);
         s.run(40, [&] {
             if (h.typingAt() && h.hunger < 0.5f) {
                 h.hunger = 0.7f;
                 h.cakes = 1;
             }
         });
         return Checks{loggedBy(h, "LEFT THE COMPUTER, SOMETHING MORE PRESSING"),
                       loggedBy(h, "TRIED A TERMINAL. LOCKED BY A PASSWORD", false)};
     }},
    {"computer-incurious", "an incurious human leaves the terminal it sees alone",
     seedWhere([](const Traits& t) { return t.curious < 0.4f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         computerNear(s, Rig::Terminal, 4);
         calm(h);
         s.run(40);
         return Checks{loggedBy(h, "STEPPED UP TO A TERMINAL TO TYPE", false)};
     }},
    {"computer-never-same", "a curious human never tries again a terminal it has tried before",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9200),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         Vec2i at = computerNear(s, Rig::Terminal, 3);
         P::triedComputer(h, at, Rig::Terminal);
         calm(h);
         s.run(40);
         return Checks{loggedBy(h, "STEPPED UP TO A TERMINAL TO TYPE", false)};
     }},
    {"computer-again", "turned away by one terminal before, it tries two more it meant to, finding a password "
                       "each time and saying something different each time",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9300),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         P::triedComputer(h, {-1000, -1000}, Rig::Terminal);
         P::meanToTry(h, computerNear(s, Rig::Terminal, 3));
         P::meanToTry(h, computerNear(s, Rig::Terminal, 6));
         calm(h);
         s.run(60);
         return Checks{loggedBy(h, "TRIED ANOTHER TERMINAL. A PASSWORD AGAIN"),
                       saidDifferently(h, "TRIED ANOTHER TERMINAL. A PASSWORD AGAIN", 2)};
     }},
    {"computer-watch", "a curious onlooker remarks on someone typing at a terminal",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         computerNear(s, Rig::Terminal, 4);
         Human& o = s.add(seedWhere([](const Traits& t) { return t.curious >= 0.6f && t.sociable < 0.4f; }, 9400), 2);
         P::seenItAll(o);
         calm(h);
         calm(o);
         s.run(40);
         return Checks{loggedBy(h, "TRIED A TERMINAL. LOCKED BY A PASSWORD"), loggedBy(o, "WATCHED @ TRY A COMPUTER")};
     }},
    {"inspect-hungry", "a hungry human goes for the cake it sees without stopping to look at anything",
     seedWhere([](const Traits& t) { return t.curious >= 0.7f; }, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         P::addCake(s.world, s.freeNear(h.cell, 5));
         calm(h);
         h.hunger = 0.7f;
         h.cakes = 0;
         s.run(40);
         return Checks{loggedBy(h, "PICKED UP A CAKE"), {"never stopped to look", [&h](const Stage&) {
                           for (const auto& e : h.log.entries())
                               if (e.what.find("TO LOOK AT") != std::string::npos) return false;
                           return true;
                       }}};
     }},
    {"inspect-body-timid", "a timid, incurious human looks at the first body it finds from where it stands",
     seedWhere([](const Traits& t) { return t.brave < 0.4f && t.curious < 0.4f; }, 9000),
     [](Stage& s) {
         Human& finder = s.first();
         Human& victim = s.add(seedWhere(plain, 9900), 4);
         P::kill(s.world, victim);
         calm(finder);
         s.run(30);
         return Checks{loggedBy(finder, "STOPPED TO LOOK AT A BODY"), loggedBy(finder, "WENT CLOSER TO LOOK AT A BODY", false),
                       loggedBy(finder, "FOUND @ DEAD")};
     }},
    {"inspect-body-brave", "a brave human walks up to the first body it finds to look at it",
     seedWhere([](const Traits& t) { return t.brave >= 0.7f && t.curious >= 0.4f; }, 9000),
     [](Stage& s) {
         Human& finder = s.first();
         Human& victim = s.add(seedWhere(plain, 9900), 4);
         P::kill(s.world, victim);
         calm(finder);
         s.run(30);
         return Checks{loggedBy(finder, "WENT CLOSER TO LOOK AT A BODY"), loggedBy(finder, "FOUND @ DEAD")};
     }},
    {"repeat-milestones", "three cakes and three bodies in, a human eats its 4th cake and finds its 4th body",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         Human& victim = s.add(seedWhere(plain, 9900), 4);
         calm(h);
         h.hunger = 0.7f;
         h.cakes = 2;
         P::setRepeats(h, P::Repeat::Cakes, 3);
         P::setRepeats(h, P::Repeat::Bodies, 3);
         P::kill(s.world, victim);
         s.run(40);
         return Checks{loggedBy(h, "ATE MY 4TH CAKE"), loggedBy(h, "FOUND MY 4TH BODY"),
                       {"counted 4 bodies", [&h](const Stage&) { return P::repeats(h, P::Repeat::Bodies) == 4; }}};
     }},
    {"repeat-harden", "a brave loner dreads a body less once it has found several",
     seedWhere([](const Traits& t) { return t.brave >= 0.7f && t.sociable <= 0.3f; }, 9000),
     [](Stage& s) {
         auto [first, later] = bodyDreads(s, 5);
         return Checks{{"found both", [first, later](const Stage&) { return first > 0.0f && later > 0.0f; }},
                       {"the later body dreaded less", [first, later](const Stage&) { return later < 0.9f * first; }}};
     }},
    {"repeat-grieve", "a timid, sociable human dreads a body more once it has found several",
     seedWhere([](const Traits& t) { return t.brave <= 0.3f && t.sociable >= 0.7f; }, 9000),
     [](Stage& s) {
         auto [first, later] = bodyDreads(s, 5);
         return Checks{{"found both", [first, later](const Stage&) { return first > 0.0f && later > 0.0f; }},
                       {"the later body dreaded more", [first, later](const Stage&) { return later > 1.1f * first; }}};
     }},
    // Planning for needs (AI/needs_planning_plan.md).
    {"top-up", "having learnt to keep near water, a human not yet thirsty drinks at a basin a few steps off",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         if (!s.basin()) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         calm(h);
         h.bottles.clear();
         h.lessons.nearWater = 1.0f;
         h.thirst = 0.4f;
         s.run(20);
         return Checks{loggedBy(h, "DRANK WHILE I COULD"), loggedBy(h, "DRANK WATER")};
     }},
    {"top-up-unlearned", "not having learnt to keep near water, a human not yet thirsty leaves the basin be",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         if (!s.basin()) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         calm(h);
         h.bottles.clear();
         h.thirst = 0.4f;
         s.run(20);
         return Checks{loggedBy(h, "DRANK WHILE I COULD", false), loggedBy(h, "DRANK WATER", false)};
     }},
    {"turn-back", "having learnt to keep near water, a human far from where it last drank turns back to drink",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         calm(h);
         h.bottles.clear();
         h.lessons.nearWater = 1.0f;
         // It has looked at the water and gone on before it thinks of it.
         s.run(25);
         // As if it last drank far off, where it came from.
         P::setLastWater(h, h.cell + Vec2i{500, 0});
         s.run(30);
         return Checks{loggedBy(h, "TOO FAR FROM WATER. TURNING BACK"), loggedBy(h, "DRANK WATER")};
     }},
    {"while-here", "just drunk, a human with no cake fetches one from a table close by",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         P::clearChunk(s.world, h.cell);
         P::addCake(s.world, s.freeNear(*b, 4));
         calm(h);
         h.bottles.clear();
         h.cakes = 0;
         h.thirst = 0.7f;
         h.hunger = 0.3f;
         s.run(50);
         return Checks{loggedBy(h, "DRANK WATER"), loggedBy(h, "FETCHING A CAKE WHILE I'M HERE"),
                       loggedBy(h, "PICKED UP A CAKE")};
     }},
    {"seek-water", "thirsty and knowing no water, a human goes looking for some",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         calm(h);
         h.bottles.clear();
         h.thirst = 0.6f;
         s.run(20);
         return Checks{loggedBy(h, "LOOKING FOR WATER")};
     }},
    {"seek-food", "hungry and knowing no cake, a human goes looking for some",
     seedWhere(plain, 9000),
     [](Stage& s) {
         Human& h = s.first();
         P::clearChunk(s.world, h.cell);
         calm(h);
         h.cakes = 0;
         h.hunger = 0.6f;
         s.run(20);
         return Checks{loggedBy(h, "LOOKING FOR FOOD")};
     }},
};

} // namespace

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int failed = 0, known = 0, ran = 0;
    std::map<std::string, int> lines;
    for (const Scenario& sc : SCENARIOS) {
        if (only && std::strcmp(only, sc.name)) continue;
        ran++;
        Stage stage(7919, sc.first, sc.asInPlay);
        Checks expect = sc.play(stage);
        for (const auto& h : stage.world.humans())
            for (const auto& e : h->log.entries()) lines[e.what] += e.times;
        std::vector<std::string> missing;
        for (const Check& c : expect)
            if (!c.ok(stage)) missing.push_back(c.label);
        bool ok = missing.empty() && !expect.empty();
        const char* verdict = ok ? "PASS" : sc.known ? "KNOWN" : "FAIL";
        failed += !ok && !sc.known;
        known += !ok && sc.known;
        std::printf("%-5s %-14s %s\n", verdict, sc.name, sc.what);
        if (sc.known) std::printf(ok ? "      now passes: drop its known note (%s)\n" : "      known: %s\n", sc.known);
        for (const auto& m : missing) std::printf("     didn't happen: %s\n", m.c_str());
        if (only || (!ok && !sc.known))
            for (Human* h : stage.humans)
                for (const auto& e : h->log.entries())
                    std::printf("     #%d %6.1f %s%s\n", h->id, e.time, e.what.c_str(),
                                e.times > 1 ? (" X" + std::to_string(e.times)).c_str() : "");
    }
    if (!ran) {
        std::printf("no scenario named %s\n", only ? only : "");
        return 1;
    }
    if (const char* path = std::getenv("LOGLINES")) {
        if (FILE* f = std::fopen(path, "w")) {
            for (auto& [what, n] : lines) std::fprintf(f, "%d\t%s\n", n, what.c_str());
            std::fclose(f);
        }
    }
    std::printf("scenarios: %d of %d passed, %d known not to yet, %d failed\n", ran - failed - known, ran, known, failed);
    return failed ? 1 : 0;
}
