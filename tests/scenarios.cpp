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
    {"foul-memory", "a human who tasted foul water goes to clean water it knows instead, farther off", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto foul = foulAndClean(s, h);
         if (foul.empty()) return Checks{{"a basin near the spawn and floor for clean water", [](const Stage&) { return false; }}};
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
    {"foul-parched", "a parched human with only foul water it knows drinks it, knowingly", 99,
     [](Stage& s) {
         Human& h = s.first();
         auto b = s.basin();
         if (!b) return Checks{{"a basin near the spawn", [](const Stage&) { return false; }}};
         foulAll(s.world, h, waterBody(s.world, *b), 0.5f);
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
