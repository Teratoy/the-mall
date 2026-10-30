#pragma once
// What the tests may see and set of humans and the world that the game
// keeps to itself. SoakProbe is a friend of Human and World.
#include "human/human.h"
#include "world/world.h"
#include <deque>

struct SoakProbe {
    // Reading how walks go (stuck_soak.cpp).
    static const std::deque<Vec2i>& path(const Human& h) { return h.path_; }
    static Vec2i target(const Human& h) { return h.target_; }
    static int stuck(const Human& h) { return h.stuck_; }
    static bool rethink(const Human& h) { return h.rethink_; }
    static int partner(const Human& h) { return h.social_.partner; }
    static int leader(const Human& h) { return h.social_.leader; }
    static int foe(const Human& h) { return h.fight_.foe; }
    static bool ateFlesh(const Human& h) { return !h.mind_.ateFrom.empty(); }

    // Setting up a situation (scenarios.cpp).
    static Human& addHuman(World& w, const HumanLook& look, Vec2i cell, uint64_t seed) { return w.addHuman(look, cell, seed); }
    /// What h thinks of o, -1 to 1, as if it had seen it all happen.
    static void setTrust(Human& h, const Human& o, float t) { h.social_.trust[o.id] = t; }
    /// Has met o, so it doesn't walk over just to greet them.
    static void setMet(Human& h, const Human& o, float at) {
        h.met.insert(o.id);
        h.social_.greetedAt[o.id] = at;
    }
    /// Given up on a way out, at home on home.
    static void settle(Human& h, Vec2i home) {
        h.escape_.settled = true;
        h.escape_.home = home;
        h.escape_.hope = 0.0f;
    }
};
