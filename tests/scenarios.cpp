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

/// A fresh world with its spawn room, and room to place more humans.
struct Stage {
    World world;
    Rng rng;
    std::vector<Human*> humans;

    explicit Stage(uint64_t seed, uint64_t first) : world(seed), rng(seed) {
        humans.push_back(&world.makeSpawn(HumanLook::random(rng), first));
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
        humans.push_back(&h);
        return h;
    }
    /// The basin cell nearest the first human, if one is near.
    std::optional<Vec2i> basin() const {
        for (int r = 0; r < 30; r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++)
                    if (std::max(std::abs(x), std::abs(y)) == r &&
                        world.tile(first().cell + Vec2i{x, y}) == Tile::Basin)
                        return first().cell + Vec2i{x, y};
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
using Checks = std::vector<Check>;

struct Scenario {
    const char* name;
    const char* what;
    /// The seed of the human the world is made with (its traits).
    uint64_t first;
    /// Sets the stage, runs it, and says what should have happened.
    std::function<Checks(Stage&)> play;
    /// Known not to happen yet, and why: reported, but doesn't fail the run.
    const char* known = nullptr;
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

/// A stranger, thirsty and not yet parched, near a settler at home by
/// the basin, with a cake or not.
Checks guardedWater(Stage& s, bool strangerHasCake, Checks expect) {
    Human& settler = s.first();
    auto b = s.basin();
    if (!b) return {{"a basin near the spawn", [](const Stage&) { return false; }}};
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

const std::vector<Scenario> SCENARIOS = {
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
         // Far from the spawn basin, so the foe knows of no water; the liar
         // knows the whole chunk.
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
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
         // It knows the chunk and its cakes, and stands a few cells from
         // one; the rival is a stranger, neither friend nor foe.
         P::knowChunk(s.world, liar, true);
         std::optional<Vec2i> cake;
         for (Vec2i c : s.world.cakes())
             if (World::slotOf(c) == World::slotOf(liar.cell) && (!cake || c.x < cake->x || (c.x == cake->x && c.y < cake->y)))
                 cake = c;
         if (!cake) return Checks{{"a cake in the spawn chunk", [](const Stage&) { return false; }}};
         P::place(s.world, liar, s.freeNear(*cake, 12));
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
    {"bottle-spare", "a friend hands its spare bottle to a thirsty friend with none", seedWhere(kindly, 12500),
     [](Stage& s) {
         return askForWater(s, {logged({"GAVE @ A BOTTLE"}), logged({"@ GAVE ME A BOTTLE"})}, [](Human& holder, Human& asker) {
             P::setTrust(holder, asker, 0.6f);
             P::setTrust(asker, holder, 0.6f);
             // The spare under half full: not worth stashing.
             holder.bottles = {Bottle{1.0f}, Bottle{0.45f}};
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
         return Checks{logged({"PICKED UP A BOTTLE LYING THERE"}), logged({"DRANK FROM MY BOTTLE"})};
     }},
    {"put-down", "a human kneels and puts cakes and a bottle down", 99,
     [](Stage& s) {
         Human& h = s.first();
         h.cakes = 2;
         h.bottles = {Bottle{1.0f}, Bottle{0.5f}};
         h.hunger = h.thirst = 0.1f;
         s.run(8);
         P::putDown(h, 2, {1});
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
};

} // namespace

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int failed = 0, known = 0, ran = 0;
    std::map<std::string, int> lines;
    for (const Scenario& sc : SCENARIOS) {
        if (only && std::strcmp(only, sc.name)) continue;
        ran++;
        Stage stage(7919, sc.first);
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
