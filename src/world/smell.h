#pragma once
#include "core/vec.h"
#include <cmath>
#include <cstdint>

/// What gives off a smell: a human gone unwashed too long, a body rotting
/// or bloody, fresh feces, or water fouled past sickening.
enum class SmellKind : uint8_t { Human, Body, Feces, Water };

/// A smell where it is now, as strong as it is, 0 to 1; who is the human
/// (living or dead) or the one whose feces it is, -1 for water.
struct Smell {
    SmellKind kind;
    Vec2i cell;
    float strength;
    int who = -1;
};

/// The strongest smells carry this many cells, walls or no walls; the
/// faintest SMELL_REACH_MIN.
inline constexpr int SMELL_REACH_MIN = 1, SMELL_REACH_MAX = 3;
/// How far a smell this strong carries, in cells.
inline int smellReach(float strength) {
    return SMELL_REACH_MIN + int(std::lround(strength * (SMELL_REACH_MAX - SMELL_REACH_MIN)));
}
