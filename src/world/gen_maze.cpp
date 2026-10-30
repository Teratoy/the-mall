#include "world/gen.h"
#include <algorithm>
#include <bit>

namespace gen {

namespace {

/// Corridor width and wall thickness are rolled per chunk.
constexpr int CORRIDOR_MIN = 3, CORRIDOR_MAX = 5;
constexpr int WALL_MIN = 1, WALL_MAX = 3;
/// Chance a dead end is knocked through, so the maze has loops.
constexpr float BRAID_CHANCE = 0.25f;
/// Up to this many halls per slot: blocks of maze cells opened into one space.
constexpr int HALLS_PER_SLOT_MAX = 2;

} // namespace

// A grid of square maze cells, cw across with wt of wall between, carved
// by a depth-first backtracker and then braided.
void maze(Chunk& c, Rng& rng) {
    const int cw = rng.range(CORRIDOR_MIN, CORRIDOR_MAX);
    const int wt = rng.range(WALL_MIN, WALL_MAX);
    const int pitch = cw + wt;
    const int nx = (c.w - 2 - wt) / pitch;
    const int ny = (c.h - 2 - wt) / pitch;
    // Leftover cells are split between both sides.
    const int ox = 1 + wt + ((c.w - 2 - wt) - nx * pitch) / 2;
    const int oy = 1 + wt + ((c.h - 2 - wt) - ny * pitch) / 2;
    auto at = [&](Vec2i m) { return Vec2i{ox + m.x * pitch, oy + m.y * pitch}; };
    // A maze cell counts only where it lies wholly inside the footprint.
    auto in = [&](Vec2i m) {
        return m.x >= 0 && m.y >= 0 && m.x < nx && m.y < ny && c.interiorRect(at(m).x, at(m).y, cw, cw);
    };
    auto idx = [&](Vec2i m) { return size_t(m.y * nx + m.x); };
    c.paint = randomFloor(rng);
    const Floor corridorFloor = c.paint;

    // Bit d is set when the cell opens toward DIRS4[d].
    std::vector<uint8_t> open(size_t(nx * ny), 0);
    auto link = [&](Vec2i a, int d) {
        Vec2i b = a + DIRS4[d];
        open[idx(a)] |= uint8_t(1 << d);
        open[idx(b)] |= uint8_t(1 << ((d + 2) % 4));
        Vec2i pa = at(a), pb = at(b);
        Vec2i lo{std::min(pa.x, pb.x), std::min(pa.y, pb.y)};
        Vec2i hi{std::max(pa.x, pb.x) + cw, std::max(pa.y, pb.y) + cw};
        carveRect(c, lo.x, lo.y, hi.x - lo.x, hi.y - lo.y);
    };

    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++) {
            Vec2i p = at({x, y});
            if (in({x, y})) carveRect(c, p.x, p.y, cw, cw);
        }

    std::vector<bool> seen(size_t(nx * ny), false);
    Vec2i start{rng.range(0, nx - 1), rng.range(0, ny - 1)};
    while (!in(start)) start = {rng.range(0, nx - 1), rng.range(0, ny - 1)};
    std::vector<Vec2i> stack{start};
    seen[idx(stack.back())] = true;
    while (!stack.empty()) {
        Vec2i a = stack.back();
        std::vector<int> ways;
        for (int d = 0; d < 4; d++)
            if (in(a + DIRS4[d]) && !seen[idx(a + DIRS4[d])]) ways.push_back(d);
        if (ways.empty()) {
            stack.pop_back();
            continue;
        }
        int d = rng.pick(ways);
        link(a, d);
        seen[idx(a + DIRS4[d])] = true;
        stack.push_back(a + DIRS4[d]);
    }

    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++) {
            Vec2i a{x, y};
            if (std::popcount(open[idx(a)]) != 1 || !rng.chance(BRAID_CHANCE)) continue;
            std::vector<int> walls;
            for (int d = 0; d < 4; d++)
                if (!(open[idx(a)] & (1 << d)) && in(a + DIRS4[d])) walls.push_back(d);
            if (!walls.empty()) link(a, rng.pick(walls));
        }

    int halls = rng.range(0, HALLS_PER_SLOT_MAX) * c.slotCount();
    for (int i = 0; i < halls; i++) {
        int hw = std::min(rng.range(2, 4), nx), hh = std::min(rng.range(2, 4), ny);
        Vec2i a{rng.range(0, nx - hw), rng.range(0, ny - hh)};
        Vec2i lo = at(a), hi = at({a.x + hw - 1, a.y + hh - 1});
        // A hall that would reach out of the footprint is left out.
        if (!c.interiorRect(lo.x, lo.y, hi.x + cw - lo.x, hi.y + cw - lo.y)) continue;
        c.paint = randomFloor(rng);
        carveRect(c, lo.x, lo.y, hi.x + cw - lo.x, hi.y + cw - lo.y);
        paintRect(c, lo.x, lo.y, hi.x + cw - lo.x, hi.y + cw - lo.y);
    }
    c.paint = corridorFloor;
}

} // namespace gen
