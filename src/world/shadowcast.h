#pragma once
#include "core/vec.h"
#include "world/chunk.h"

/// Recursive shadowcasting from eye out to radius: calls mark(cell) for the
/// eye and every cell in line of sight within the radius, the opaque ones
/// that stop it included. tileAt(cell) says what stands on a cell. Sight
/// looks this way, and light shines this way from its sources.
template <class TileAt, class Mark>
void shadowcast(Vec2i eye, int radius, const TileAt& tileAt, const Mark& mark);

namespace detail {

// Scans one octant row by row, narrowing the lit slopes [end, start] past
// each run of opaque cells and recursing for the light beyond them.
template <class TileAt, class Mark>
void castOctant(Vec2i eye, int radius, const TileAt& tileAt, const Mark& mark, int row, float start, float end, int xx,
                int xy, int yx, int yy) {
    if (start < end) return;
    float newStart = 0.0f;
    for (int i = row; i <= radius; i++) {
        bool blocked = false;
        for (int dx = -i, dy = -i; dx <= 0; dx++) {
            float l = (dx - 0.5f) / (dy + 0.5f);
            float r = (dx + 0.5f) / (dy - 0.5f);
            if (start < r) continue;
            if (end > l) break;
            Vec2i c = eye + Vec2i{dx * xx + dy * xy, dx * yx + dy * yy};
            if (dx * dx + dy * dy <= radius * radius) mark(c);
            bool wall = opaque(tileAt(c));
            if (blocked) {
                if (wall) {
                    newStart = r;
                    continue;
                }
                blocked = false;
                start = newStart;
            } else if (wall && i < radius) {
                blocked = true;
                castOctant(eye, radius, tileAt, mark, i + 1, start, l, xx, xy, yx, yy);
                newStart = r;
            }
        }
        if (blocked) break;
    }
}

} // namespace detail

template <class TileAt, class Mark>
void shadowcast(Vec2i eye, int radius, const TileAt& tileAt, const Mark& mark) {
    mark(eye);
    // Turns octant 0 into each of the eight.
    static constexpr int M[4][8] = {{1, 0, 0, -1, -1, 0, 0, 1},
                                    {0, 1, -1, 0, 0, -1, 1, 0},
                                    {0, 1, 1, 0, 0, -1, -1, 0},
                                    {1, 0, 0, 1, -1, 0, 0, -1}};
    for (int o = 0; o < 8; o++)
        detail::castOctant(eye, radius, tileAt, mark, 1, 1.0f, 0.0f, M[0][o], M[1][o], M[2][o], M[3][o]);
}
