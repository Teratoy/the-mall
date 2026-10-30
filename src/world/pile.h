#pragma once
#include "human/bottle.h"
#include <vector>

/// What lies on a floor cell, put down by someone: cakes, and bottles with
/// what water they hold. In plain sight, like cakes on a table; the
/// bottles are see-through.
struct Pile {
    int cakes = 0;
    std::vector<Bottle> bottles;

    bool empty() const { return cakes == 0 && bottles.empty(); }
    /// All the water in its bottles.
    float water() const {
        float sum = 0.0f;
        for (const Bottle& b : bottles) sum += b.water;
        return sum;
    }
};
