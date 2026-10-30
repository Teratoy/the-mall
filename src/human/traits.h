#pragma once
#include "core/rng.h"

/// What one human is like, rolled once at birth and never changed. Each is
/// 0 to 1, with most humans somewhere in the middle and a few at either end.
/// The traits weigh what a human wants to do next (Human::choose) and move
/// the thresholds it acts on: when it runs, how much it keeps, how far it
/// strolls. They never make a human do what it couldn't do before.
struct Traits {
    /// 0 timid, 1 fearless: runs from fights later, forgets a fright sooner,
    /// goes after cake holders when merely hungry, washes off blood less.
    float brave = 0.5f;
    /// 0 generous, 1 grasping: shares less, carries more, preys on the weak
    /// sooner, and goes for cakes before anything else.
    float greedy = 0.5f;
    /// 0 stay-at-home, 1 restless: explores and checks doors before
    /// greeting or following, sits less, strolls farther.
    float curious = 0.5f;
    /// 0 loner, 1 friendly: greets, follows and escorts before exploring;
    /// a real loner never escorts at all.
    float sociable = 0.5f;

    /// Middling more often than not: the mean of two rolls. Rolled from
    /// the human's seed, apart from the dice it lives by.
    static Traits random(uint64_t seed) {
        Rng rng(seed ^ 0x7261697473ULL);
        auto roll = [&] { return (rng.unit() + rng.unit()) / 2.0f; };
        Traits t;
        t.brave = roll();
        t.greedy = roll();
        t.curious = roll();
        t.sociable = roll();
        return t;
    }
};
