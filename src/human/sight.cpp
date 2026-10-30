#include "human/sight.h"
#include "world/world.h"
#include <algorithm>

void Sight::look(const World& world, Vec2i eye, int radius) {
    eye_ = eye;
    radius_ = std::clamp(radius, 1, RADIUS);
    std::fill(seen_.begin(), seen_.end(), false);
    mark(eye);
    // Turns octant 0 into each of the eight.
    static constexpr int M[4][8] = {{1, 0, 0, -1, -1, 0, 0, 1},
                                    {0, 1, -1, 0, 0, -1, 1, 0},
                                    {0, 1, 1, 0, 0, -1, -1, 0},
                                    {1, 0, 0, 1, -1, 0, 0, -1}};
    for (int o = 0; o < 8; o++) cast(world, 1, 1.0f, 0.0f, M[0][o], M[1][o], M[2][o], M[3][o]);
}

bool Sight::sees(Vec2i cell) const {
    Vec2i l = cell - eye_ + Vec2i{RADIUS, RADIUS};
    return l.x >= 0 && l.y >= 0 && l.x < SIDE && l.y < SIDE && seen_[size_t(l.y * SIDE + l.x)];
}

void Sight::mark(Vec2i cell) {
    Vec2i l = cell - eye_ + Vec2i{RADIUS, RADIUS};
    if (l.x >= 0 && l.y >= 0 && l.x < SIDE && l.y < SIDE) seen_[size_t(l.y * SIDE + l.x)] = true;
}

// Scans one octant row by row, narrowing the lit slopes [end, start] past
// each run of opaque cells and recursing for the light beyond them.
void Sight::cast(const World& world, int row, float start, float end, int xx, int xy, int yx, int yy) {
    if (start < end) return;
    float newStart = 0.0f;
    for (int i = row; i <= radius_; i++) {
        bool blocked = false;
        for (int dx = -i, dy = -i; dx <= 0; dx++) {
            float l = (dx - 0.5f) / (dy + 0.5f);
            float r = (dx + 0.5f) / (dy - 0.5f);
            if (start < r) continue;
            if (end > l) break;
            Vec2i c = eye_ + Vec2i{dx * xx + dy * xy, dx * yx + dy * yy};
            if (dx * dx + dy * dy <= radius_ * radius_) mark(c);
            bool wall = opaque(world.tile(c));
            if (blocked) {
                if (wall) {
                    newStart = r;
                    continue;
                }
                blocked = false;
                start = newStart;
            } else if (wall && i < radius_) {
                blocked = true;
                cast(world, i + 1, start, l, xx, xy, yx, yy);
                newStart = r;
            }
        }
        if (blocked) break;
    }
}
