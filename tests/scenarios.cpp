// Set-up situations for what random soaks seldom reach: each scenario
// places a few humans in a fresh spawn room, sets them up (traits by seed,
// needs, trust, memory), lets the world run, and checks that the thing
// happened, by the lines it leaves in their logs.
// Usage: build/scenarios [name]   (make scenarios; part of make check)
//   With a name, runs only that scenario and prints every log line.
//   LOGLINES=path writes every distinct log line written, as the soak does,
//   for tools/coverage.py.
// Exits non-zero if any scenario fails. Deterministic, like the soak.
#include "probe.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
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
        const float dt = 1.0f / 30.0f;
        for (int f = 0; f < int(seconds / dt); f++) world.update(dt);
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
using Checks = std::vector<Check>;
/// This line in h's log, or (said false) not in it.
Check loggedBy(const Human& h, const std::string& line, bool said = true) {
    return {(said ? "#" : "no #") + std::to_string(h.id) + " " + line, [&h, line, said](const Stage&) {
                for (const auto& e : h.log.entries())
                    if (e.what == line) return said;
                return !said;
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
    setUp(holder, asker);
    s.run(100);
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
/// once before (wary 0.5) and as hungry as asked. A second human, whom it
/// trusts as asked, told it of the cake if tipped.
Checks wareCake(Stage& s, float hunger, Checks expect, bool tipped = false) {
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
    P::setWary(h, 0.5f, 0.0f);
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

const std::vector<Scenario> SCENARIOS = {
    {"wary-darts", "darted once, a human not yet hungry leaves the next table cake alone", seedWhere(plain, 9000),
     [](Stage& s) {
         return wareCake(s, 0.3f, {logged({"LEFT A CAKE ALONE. IT COULD BE RIGGED"}), notLogged({"PICKED UP A CAKE"})});
     }},
    {"wary-starving", "darted once, a starving human still risks the table cake", seedWhere(plain, 9000),
     [](Stage& s) { return wareCake(s, 0.85f, {logged({"PICKED UP A CAKE"})}); }},
    {"wary-tip", "darted once, it takes a cake a friend told it of on their word", seedWhere(plain, 9000),
     [](Stage& s) { return wareCake(s, 0.3f, {logged({"PICKED UP A CAKE"})}, true); }},
    {"wary-slam", "shut in once, with no key it leaves the bait in a room with one door", seedWhere(plain, 9000),
     [](Stage& s) {
         return wareSlam(s, 0, {logged({"LEFT THE CAKE IN A ROOM WITH ONE DOOR. NOT WITHOUT A KEY"}),
                                notLogged({"SHUT IN A SLAM ROOM"})});
     }},
    {"wary-slam-key", "shut in once, with a key in hand it goes for the bait anyway", seedWhere(plain, 9000),
     [](Stage& s) { return wareSlam(s, 1, {logged({"PICKED UP A CAKE"})}); }},
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
     seedWhere([](const Traits& t) { return t.sociable >= 0.6f && t.greedy >= 0.5f && !cunning(t); }, 4000),
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
         P::knowChunk(s.world, liar);
         // A restless loner: it goes its own way, down the route, not with the liar.
         Human& victim = s.add(seedWhere([](const Traits& t) { return t.sociable < 0.3f && t.curious > 0.5f; }, 6000));
         setUpLie(s, liar, victim, TrapKind::Darts, 4);
         for (const auto& [c, kind] : liar.memory.traps) P::addTrap(s.world, c, kind);
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
         // Set up apart from how the spawn room came out: of its two tables
         // farthest apart, a cake on one and nothing on the other. The liar
         // knows the room and its cake, and stands about ten cells from the
         // cake on the side away from the bare table; the rival, a stranger,
         // wakes beside it, too far from the bare table ever to have seen it.
         std::vector<Vec2i> tables;
         for (const Table& t : s.world.tables())
             if (World::slotOf(t.at) == World::slotOf(liar.cell)) tables.push_back(t.at);
         std::optional<std::pair<Vec2i, Vec2i>> pair;
         for (Vec2i a : tables)
             for (Vec2i b : tables)
                 if (!pair || cheb(a, b) > cheb(pair->first, pair->second)) pair = {a, b};
         if (!pair || cheb(pair->first, pair->second) < 40)
             return Checks{{"two tables far apart in the spawn chunk", [](const Stage&) { return false; }}};
         auto [cake, bare] = *pair;
         P::clearChunk(s.world, cake);
         P::addCake(s.world, cake);
         P::knowChunk(s.world, liar, true);
         std::optional<Vec2i> stand;
         for (int y = -10; y <= 10; y++)
             for (int x = -10; x <= 10; x++) {
                 Vec2i c = cake + Vec2i{x, y};
                 if (cheb(c, cake) < 8 || s.world.tile(c) != Tile::Floor || s.world.occupant(c) || s.world.table(c) ||
                     s.world.statue(c))
                     continue;
                 if (!stand || cheb(c, bare) > cheb(*stand, bare)) stand = c;
             }
         if (!stand) return Checks{{"floor near the cake", [](const Stage&) { return false; }}};
         P::place(s.world, liar, *stand);
         Human& rival = s.add(seedWhere(plain, 9700));
         liar.hunger = 0.4f;
         liar.cakes = 0;
         rival.hunger = 0.68f;
         rival.cakes = 0;
         liar.thirst = rival.thirst = 0.1f;
         s.run(90);
         return Checks{logged({"SENT @ AWAY FROM A CAKE I WANT"}), logged({"@ TOLD ME WHERE A CAKE IS"})};
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
     seedWhere([](const Traits& t) { return t.greedy >= 0.55f && t.sociable < 0.4f && t.brave < 0.4f; }, 13000),
     [](Stage& s) {
         return askForWater(s, {logged({"SOLD @ SOME WATER FOR A CAKE"}), logged({"BOUGHT SOME WATER FROM @ FOR A CAKE"})},
                            [](Human&, Human& asker) { asker.cakes = 1; });
     }},
    {"bottle-lie", "a cunning stranger says its bottle is empty, and is seen through",
     seedWhere([](const Traits& t) { return cunning(t) && t.sociable < 0.4f; }, 14000),
     [](Stage& s) {
         return askForWater(s, {logged({"TOLD @ MY BOTTLE WAS EMPTY"}), logged({"@ SAID THEIR BOTTLE WAS EMPTY. IT WASN'T"})},
                            [](Human&, Human&) {});
     }},
    {"bottle-refuse", "a stingy stranger won't give any of its water",
     seedWhere([](const Traits& t) { return t.greedy < 0.35f && t.sociable < 0.4f && t.brave < 0.3f; }, 15000),
     [](Stage& s) {
         return askForWater(s, {logged({"WOULDN'T GIVE @ WATER"}), logged({"@ WOULDN'T GIVE ME WATER"})},
                            [](Human&, Human&) {});
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
     seedWhere([](const Traits& t) { return t.greedy >= 0.55f && t.sociable < 0.4f && t.brave < 0.4f; }, 13000),
     [](Stage& s) {
         return askForWater(s, {logged({"ASKED @ FOR WATER"}), notLogged({"SOLD @ SOME WATER FOR A CAKE"})},
                            [](Human& holder, Human& asker) {
                                asker.cakes = 1;
                                holder.lessons.nearWater = 1.0f;
                            });
     }},
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
         Human& other = P::addHuman(s.world, HumanLook::random(s.rng), s.freeNear(at, 1), seedWhere(plain, 18000));
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
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         // Out of sight of the basin, which it remembers foul; clean water
         // farther off the other way, both known.
         auto foul = waterBody(s.world, *b);
         foulAll(s.world, h, foul, 0.5f);
         P::place(s.world, h, s.farFrom(*b, Sight::RADIUS + 5));
         P::knowChunk(s.world, h);
         int near = cheb(h.cell, *b);
         Vec2i slot = World::slotOf(h.cell);
         std::optional<Vec2i> clean;
         for (int y = 0; y < CHUNK; y++)
             for (int x = 0; x < CHUNK; x++) {
                 Vec2i c{slot.x * CHUNK + x, slot.y * CHUNK + y};
                 int d = cheb(c, h.cell);
                 if (s.world.tile(c) != Tile::Floor || s.world.table(c) || s.world.statue(c) || d <= near + 5 ||
                     d > near + 20)
                     continue;
                 if (!clean || cheb(c, *b) > cheb(*clean, *b)) clean = c;
             }
         if (!clean) return Checks{{"floor for clean water", [](const Stage&) { return false; }}};
         P::addWater(s.world, *clean);
         h.memory.see(*clean, Tile::Basin);
         // No bottle to drink from or fill: it has to kneel at the water.
         h.bottles.clear();
         h.thirst = 0.7f;
         h.hunger = 0.1f;
         int closest = near;
         for (int t = 0; t < 120; t++) {
             s.run(1);
             for (Vec2i c : foul) closest = std::min(closest, cheb(h.cell, c));
         }
         return Checks{logged({"DRANK WATER"}),
                       {"no foul water drunk",
                        [](const Stage& st) { return !st.logged("DRANK FOUL WATER") && !st.logged("DRANK FOUL WATER AGAIN"); }},
                       {"kept off the foul basin", [closest](const Stage&) { return closest > 2; }}};
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
         Check lying = runSeeingLying(s, 80);
         return Checks{logged({"@ KNEW OF NO WATER EITHER"}), askedOnce(),
                       logged({"NO WATER ANYWHERE. LYING BY A LOCKED DOOR, CALLING FOR HELP"}), lying,
                       unlogged("LEFT @. THEY KNOW NO MORE THAN I DO")};
     }},
    {"parched-door", "a caring human with a key, beyond the door the parched lie calling through, opens it",
     seedWhere([](const Traits& t) { return t.sociable >= 0.75f && t.brave >= 0.3f; }, 17000),
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
         s.run(100);
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
