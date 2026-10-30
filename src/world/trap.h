#pragma once
#include "core/vec.h"
#include <cstdint>

/// Every trap hides in a cell until it goes off. Spikes hide in the floor
/// and leave whoever steps on them nearly dead, however healthy, and
/// bleeding hard enough that few live; then they sink and wait for the next. Plates hide under a cake on a table, or in the floor where no
/// table fit, and go off when the cake is lifted or stepped on. A slam
/// plate shuts its room's gate for good, until a key opens it. A dart
/// plate shoots a volley from a wall down a straight line at whoever set
/// it off: darts hurt whoever they hit, but never kill.
enum class TrapKind : uint8_t { Spikes, Slam, Darts };

struct Trap {
    TrapKind kind;
    /// Slam: the gate it shuts. Darts: the wall cell they last flew from.
    Vec2i from;
    /// Darts: the way they fly.
    Vec2i dir;
    /// Counts down while it shows it went off; armed again at zero, unless spent.
    float sprung = 0.0f;
    /// Gone off for good.
    bool spent = false;
    /// Slam: someone stands in the gate, so it shuts once they're out of it.
    bool shutting = false;
    /// Darts still to fly in this volley, and the time to the next one.
    int volley = 0;
    float reload = 0.0f;

    bool armed() const { return sprung <= 0.0f && !spent; }
};

/// A dart in flight, down a row or column of cells.
struct Dart {
    Vec2i from, dir;
    /// Cells flown so far.
    float flown = 0.0f;

    Vec2i at() const { return from + dir * int(flown); }
};
