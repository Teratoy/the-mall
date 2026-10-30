#pragma once
#include "core/vec.h"
#include <cstdint>
#include <vector>

/// Something a human went through or saw that it could talk about later:
/// what happened, when and where, who did it to whom, and how much it
/// struck it. Kept apart from the life log, which is words for the reader;
/// episodes are what chit-chat draws on (AI/chitchat_plan.md).
///
/// What happened is told the same whoever remembers it: a punch is a Fight
/// by who on whom for the one who threw it, the one hit and anyone who saw,
/// so two humans can tell they remember the same thing (Human::shares).
/// What only happened inside one human (waking, starving, snapping, hope,
/// coming to like or hate someone, finding out a lie) has it as who.
struct Episode {
    enum class What : uint8_t {
        Woke,      // came to here, remembering nothing
        Fight,     // who hit whom
        Kill,      // who killed whom
        Body,      // found whom dead
        Flesh,     // who ate from whom's body
        Gnawed,    // found whom's body half eaten
        Trap,      // a trap went off at cell (detail: TrapKind), on who if known
        Spotted,   // spotted a hidden trap at cell (detail: TrapKind)
        Gift,      // who gave whom a cake (detail: Human::Gift)
        Door,      // who unlocked a door at cell
        NewPlace,  // set foot in a chunk new to it (detail: the chunk id)
        Pool,      // a fresh pool of blood at cell, whose if its own (who)
        Escort,    // who led whom to water
        Snapped,   // its mind snapped, or came back
        Friend,    // came to think of whom as a friend
        Foe,       // came to think of whom as a foe
        Lied,      // found out whom lied to it
        Starving,  // went starving
        Parched,   // went parched
        Loot,      // who went through whom's body
        FoulWater, // drank foul water at cell
        Despair,   // lost hope of a way out (detail 1), or got it back (0)
        Settled,   // made a home at cell
        Freed,     // who let whom out of a slam room
        Count
    };
    What what;
    /// World seconds: when it began and when it last went on (a fight
    /// goes on for a while; most things are over at once).
    float since, when;
    Vec2i cell;
    int who = -1, whom = -1;
    int detail = 0;
    /// How much it struck it, 0 to 1; repeats of the same thing add up.
    float strength;
    /// The others it had in sight when it happened.
    std::vector<int> with;
    /// Those it has talked to about it since.
    std::vector<int> toldTo;
};
