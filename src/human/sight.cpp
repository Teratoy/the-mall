#include "human/sight.h"
#include "world/shadowcast.h"
#include "world/world.h"
#include <algorithm>

void Sight::look(const World& world, Vec2i eye, int radius) {
    eye_ = eye;
    radius_ = std::clamp(radius, 1, RADIUS);
    std::fill(seen_.begin(), seen_.end(), false);
    shadowcast(eye, radius_, [&](Vec2i c) { return world.tile(c); }, [&](Vec2i c) {
        Vec2i d = c - eye;
        if (d.x * d.x + d.y * d.y <= DARK_SIGHT * DARK_SIGHT || world.light(c) > 0) mark(c);
    });
}

bool Sight::sees(Vec2i cell) const {
    Vec2i l = cell - eye_ + Vec2i{RADIUS, RADIUS};
    return l.x >= 0 && l.y >= 0 && l.x < SIDE && l.y < SIDE && seen_[size_t(l.y * SIDE + l.x)];
}

void Sight::mark(Vec2i cell) {
    Vec2i l = cell - eye_ + Vec2i{RADIUS, RADIUS};
    if (l.x >= 0 && l.y >= 0 && l.x < SIDE && l.y < SIDE) seen_[size_t(l.y * SIDE + l.x)] = true;
}
