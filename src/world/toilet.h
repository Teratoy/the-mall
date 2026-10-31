#pragma once
#include "core/vec.h"
#include <cstdint>

/// A toilet stands against a wall on a cell of its own, used from the cell
/// in front. Everyone who uses it leaves some of what they did in it, and
/// nothing ever cleans it.
struct Toilet {
    /// From this dirt it looks fouled to whoever sees it: a piss or more.
    static constexpr float FOULED = 0.05f;
    /// The floor cell it is used from.
    Vec2i front;
    /// How dirty it is: 0 clean, about 1 after a few uses, no top.
    float dirt = 0.0f;
    /// How much of that dirt is piss, which yellows its water.
    float piss = 0.0f;
    /// Shapes its stains its own.
    uint32_t seed = 0;
};
