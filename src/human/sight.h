#pragma once
#include "core/vec.h"
#include <vector>

class World;

/// What a human can see right now: every cell in line of sight within
/// RADIUS, all round. Walls and locked doors block sight but are seen.
class Sight {
public:
    static constexpr int RADIUS = 20;

    Sight() : seen_(SIDE * SIDE, false) {}

    /// Recomputes the view from the eye cell, by recursive shadowcasting,
    /// out to radius (at most RADIUS): less for the dazed.
    void look(const World& world, Vec2i eye, int radius = RADIUS);
    bool sees(Vec2i cell) const;
    /// The cell it last looked from, and how far it saw from there.
    Vec2i eye() const { return eye_; }
    int radius() const { return radius_; }
    /// Eyes shut: sees nothing until it next looks.
    void blind() { seen_.assign(seen_.size(), false); }

    template <class F>
    void forEach(F f) const {
        for (int y = 0; y < SIDE; y++)
            for (int x = 0; x < SIDE; x++)
                if (seen_[size_t(y * SIDE + x)]) f(eye_ + Vec2i{x - RADIUS, y - RADIUS});
    }

private:
    static constexpr int SIDE = RADIUS * 2 + 1;
    Vec2i eye_;
    int radius_ = RADIUS;
    std::vector<bool> seen_;

    void mark(Vec2i cell);
};
