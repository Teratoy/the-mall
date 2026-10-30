#pragma once
#include <algorithm>

/// A water bottle, carried in a back pocket. It holds up to a whole thirst
/// bar of water, as foul and as red as the water it was filled from, and
/// it is see-through: anyone who sees it sees how full and how dirty it is.
struct Bottle {
    /// 0 empty, 1 full: a full one takes thirst from 1 to 0.
    float water = 0.0f;
    /// How foul and how red what it holds is, as world water is (World::foul,
    /// Blood::tint).
    float foul = 0.0f, red = 0.0f;

    bool empty() const { return water <= 0.0f; }
    bool full() const { return water >= 1.0f; }
    /// Tops it up with this much water, this foul and red, as far as it
    /// holds: what is in it and what comes in mix by how much of each.
    /// Returns how much went in.
    float add(float amount, float addFoul, float addRed) {
        amount = std::min(amount, 1.0f - water);
        if (amount <= 0.0f) return 0.0f;
        float total = water + amount;
        foul = (foul * water + addFoul * amount) / total;
        red = (red * water + addRed * amount) / total;
        water = total;
        return amount;
    }
    /// Takes up to this much out. Returns how much came out.
    float take(float amount) {
        amount = std::min(amount, water);
        water -= amount;
        if (water <= 0.0f) *this = {};
        return amount;
    }
    /// Pours this much of it into other, mixed in as any topping up.
    /// Returns how much went over.
    float pourInto(Bottle& other, float amount) {
        float f = foul, r = red;
        float moved = other.add(std::min(amount, water), f, r);
        take(moved);
        return moved;
    }
};
