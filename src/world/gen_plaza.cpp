#include "world/gen.h"
#include <algorithm>
#include <cmath>

namespace gen {

namespace {

/// Solid wall left between the hall and the chunk's border.
constexpr int MARGIN_MIN = 3, MARGIN_MAX = 7;
/// The inlaid band: this far in from the hall's edge, and this wide.
constexpr int BAND_IN_MIN = 2, BAND_IN_MAX = 4;
constexpr int BAND_WIDTH_MIN = 1, BAND_WIDTH_MAX = 2;
/// Columns 2 cells across, this far apart, this far in from the hall's edge.
constexpr float COLONNADE_CHANCE = 0.6f;
constexpr int COLUMN = 2;
constexpr int COLUMN_PITCH_MIN = 8, COLUMN_PITCH_MAX = 12;
constexpr int COLUMN_IN = 6;
/// The pool round the island is at least this wide per slot across, and
/// has a quay of the band's floor this wide round it.
constexpr int MOAT_MIN = 2, MOAT_MAX = 3;
constexpr int QUAY = 2;
constexpr float ROUND_POOL_CHANCE = 0.5f;

enum class Hall { Rect, Octagon, Oval };

/// The hall's outline, shrunk by an inset for the band and the columns.
struct Outline {
    Hall kind;
    int x0, y0, x1, y1;

    bool in(Vec2i p, int inset) const {
        float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
        float hw = (x1 - x0) / 2.0f - inset, hh = (y1 - y0) / 2.0f - inset;
        float dx = std::fabs(p.x + 0.5f - cx), dy = std::fabs(p.y + 0.5f - cy);
        if (hw <= 0 || hh <= 0) return false;
        switch (kind) {
        case Hall::Rect: return dx <= hw && dy <= hh;
        case Hall::Octagon: {
            float cut = std::min(x1 - x0, y1 - y0) / 4.0f;
            return dx <= hw && dy <= hh && dx + dy <= (x1 - x0 + y1 - y0) / 2.0f - cut - 1.414f * inset;
        }
        case Hall::Oval: return (dx / hw) * (dx / hw) + (dy / hh) * (dy / hh) <= 1.0f;
        }
        return false;
    }
};

} // namespace

// The hall first, then its band, then the fountain and its quay, which
// paint over the band, and last the columns, which keep clear of all of it.
Plaza plaza(Chunk& c, Rng& rng, Vec2i island) {
    const int m = rng.range(MARGIN_MIN, MARGIN_MAX);
    const Outline hall{Hall(rng.range(0, 2)), m, m, c.w - m, c.h - m};
    const Vec2i centre{c.w / 2, c.h / 2};
    c.paint = randomFloor(rng);
    const Floor band = randomFloor(rng);
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++)
            if (hall.in({x, y}, 0)) carve(c, {x, y});
    const int bandIn = rng.range(BAND_IN_MIN, BAND_IN_MAX), bandOut = bandIn + rng.range(BAND_WIDTH_MIN, BAND_WIDTH_MAX);
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++)
            if (c.get({x, y}) == Tile::Floor && hall.in({x, y}, bandIn) && !hall.in({x, y}, bandOut))
                c.setFloor({x, y}, band);

    Plaza p{centre, {}, {}, 0};
    int reach = 0;
    if (island.x > 0 && island.y > 0) {
        p.island = centre - Vec2i{island.x / 2, island.y / 2};
        p.islandSize = island;
        auto onIsland = [&](Vec2i q) {
            return q.x >= p.island.x && q.y >= p.island.y && q.x < p.island.x + island.x && q.y < p.island.y + island.y;
        };
        const int moat = rng.range(MOAT_MIN, MOAT_MAX) * c.slots.x;
        const bool round = rng.chance(ROUND_POOL_CHANCE);
        // A round pool reaches past the island's corners as well as its sides.
        const float rx = std::max(island.x / 2.0f * 1.42f + 1, island.x / 2.0f + moat);
        const float ry = std::max(island.y / 2.0f * 1.42f + 1, island.y / 2.0f + moat);
        auto inPool = [&](Vec2i q) {
            if (onIsland(q)) return false;
            if (!round)
                return q.x >= p.island.x - moat && q.y >= p.island.y - moat && q.x < p.island.x + island.x + moat &&
                       q.y < p.island.y + island.y + moat;
            float dx = (q.x + 0.5f - (p.island.x + island.x / 2.0f)) / rx;
            float dy = (q.y + 0.5f - (p.island.y + island.y / 2.0f)) / ry;
            return dx * dx + dy * dy <= 1.0f;
        };
        reach = int(round ? std::max(rx, ry) : std::max(island.x, island.y) / 2.0f + moat) + 1;
        for (int y = centre.y - reach - QUAY; y <= centre.y + reach + QUAY; y++)
            for (int x = centre.x - reach - QUAY; x <= centre.x + reach + QUAY; x++) {
                Vec2i q{x, y};
                if (!c.interior(q) || c.get(q) != Tile::Floor) continue;
                if (inPool(q)) {
                    c.set(q, Tile::Water);
                    continue;
                }
                bool quay = onIsland(q);
                for (int dy = -QUAY; dy <= QUAY && !quay; dy++)
                    for (int dx = -QUAY; dx <= QUAY && !quay; dx++) quay = inPool(q + Vec2i{dx, dy});
                if (quay) c.setFloor(q, band);
            }
    }

    // Columns stand on a grid through the centre, in a band near the edge as
    // wide as the grid's pitch, so each side gets one row of them.
    int inner = std::min(c.w, c.h) / 2 - m;
    if (rng.chance(COLONNADE_CHANCE)) {
        const int pitch = rng.range(COLUMN_PITCH_MIN, COLUMN_PITCH_MAX);
        for (int y = centre.y % pitch; y < c.h - COLUMN; y += pitch)
            for (int x = centre.x % pitch; x < c.w - COLUMN; x += pitch) {
                Vec2i q{x, y};
                if (!hall.in(q, COLUMN_IN) || !hall.in(q + Vec2i{COLUMN - 1, COLUMN - 1}, COLUMN_IN) ||
                    hall.in(q, COLUMN_IN + pitch))
                    continue;
                bool clear = true;
                for (int dy = -1; dy <= COLUMN && clear; dy++)
                    for (int dx = -1; dx <= COLUMN && clear; dx++) clear = c.get(q + Vec2i{dx, dy}) == Tile::Floor;
                if (!clear) continue;
                for (int dy = 0; dy < COLUMN; dy++)
                    for (int dx = 0; dx < COLUMN; dx++) c.set(q + Vec2i{dx, dy}, Tile::Wall);
            }
        inner -= COLUMN_IN + pitch;
    }
    // Halfway between the fountain's quay and the columns or the band.
    p.ring = (reach + QUAY + inner - bandOut) / 2;
    return p;
}

} // namespace gen
