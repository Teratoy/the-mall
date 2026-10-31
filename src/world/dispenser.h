#pragma once
#include "core/vec.h"
#include <algorithm>

/// Water a dispenser holds when full: three thirst bars, as a full bottle
/// holds one (user).
inline constexpr float DISPENSER_HOLDS = 3.0f;

/// A water dispenser stands on one cell of a server room against a wall,
/// an upturned jug on a stand, and blocks walking but not sight. Its water
/// is always clean, never comes back on its own, and goes back in only
/// when someone pours a bottle into it (user). The jug is see-through, so
/// how full it is shows.
struct Dispenser {
    Vec2i at;
    /// Away from its wall: down toward the viewer against a north wall
    /// (seen from the front), east or west against a side wall (side on).
    Vec2i facing;
    /// How much water is left in it, 0 to DISPENSER_HOLDS.
    float water = 0.0f;

    bool side() const { return facing.x != 0; }
    /// Takes up to this much out. Returns how much came out.
    float take(float amount) {
        amount = std::min(amount, water);
        water -= amount;
        return amount;
    }
    /// Pours up to this much in, as far as it holds. Returns how much went in.
    float add(float amount) {
        amount = std::min(amount, DISPENSER_HOLDS - water);
        if (amount <= 0.0f) return 0.0f;
        water += amount;
        return amount;
    }
};
