#include "world/gen.h"
#include "world/gen_tuning.h"
#include <cmath>
#include <deque>

namespace gen {

namespace {

/// Floor cells kept between a pool and anything that is not floor.
tune::Int POOL_RING{"water", "POOL_RING"};
/// Tries per pool at a shape, size, turn and place that fits.
tune::Int POOL_TRIES{"water", "POOL_TRIES"};
/// Half a pool's extent. L, T and C need room for their arms and hollow.
tune::Float POOL_SIZE_MIN{"water", "POOL_SIZE_MIN"}, POOL_SIZE_MAX{"water", "POOL_SIZE_MAX"};
tune::Float ARMED_SIZE_MIN{"water", "ARMED_SIZE_MIN"};
/// Arm thickness; the C's hollow keeps at least this radius.
tune::Float ARM_MIN{"water", "ARM_MIN"}, HOLLOW_MIN{"water", "HOLLOW_MIN"};
/// The oval's narrow half axis, so a small one is not a ragged sliver.
tune::Float MINOR_MIN{"water", "MINOR_MIN"};
tune::Int CANAL_WIDTH_MIN{"water", "CANAL_WIDTH_MIN"}, CANAL_WIDTH_MAX{"water", "CANAL_WIDTH_MAX"};
tune::Int QUAY_WIDTH_MIN{"water", "QUAY_WIDTH_MIN"}, QUAY_WIDTH_MAX{"water", "QUAY_WIDTH_MAX"};
/// Least room between a canal and the walls it runs along.
tune::Int CANAL_MARGIN{"water", "CANAL_MARGIN"};

enum class Shape { Round, Oval, Square, L, T, C, Count };

/// A pool's outline about its centre, before it is turned.
struct Outline {
    Shape shape;
    /// Half the extent; the oval's other half axis; the arms' thickness.
    float size, minor, arm;

    bool covers(float x, float y) const {
        bool box = std::fabs(x) <= size && std::fabs(y) <= size;
        switch (shape) {
        case Shape::Round: return x * x + y * y <= size * size;
        case Shape::Oval: return x * x / (size * size) + y * y / (minor * minor) <= 1.0f;
        case Shape::Square: return box;
        case Shape::L: return box && (y >= size - arm || x <= -size + arm);
        case Shape::T: return box && (y <= -size + arm || std::fabs(x) <= arm / 2);
        case Shape::C: {
            // A ring with a straight slot cut out, opening toward +x.
            float r2 = x * x + y * y, in = size - arm;
            return r2 <= size * size && r2 >= in * in && !(x > 0 && std::fabs(y) < in * 0.7f);
        }
        case Shape::Count: break;
        }
        return false;
    }
};

Outline randomOutline(Rng& rng) {
    Outline o{Shape(rng.range(0, int(Shape::Count) - 1)), 0, 0, 0};
    bool armed = o.shape == Shape::L || o.shape == Shape::T || o.shape == Shape::C;
    // Cell centres sit on whole numbers, so half-cell sizes keep curves
    // from ending in one-cell spikes.
    auto half = [](float v) { return std::floor(v) + 0.5f; };
    o.size = half(rng.range(armed ? ARMED_SIZE_MIN : POOL_SIZE_MIN, POOL_SIZE_MAX));
    o.minor = half(std::max<float>(MINOR_MIN, o.size * rng.range(0.4f, 0.7f)));
    float armMax = o.shape == Shape::C ? o.size - HOLLOW_MIN : o.size * 0.75f;
    o.arm = rng.range(ARM_MIN, std::max<float>(ARM_MIN, armMax));
    return o;
}

/// For each cell, how many steps (8 ways) to the nearest cell that is not floor.
std::vector<int> clearance(const Chunk& c) {
    std::vector<int> dist(size_t(c.w * c.h), -1);
    std::deque<Vec2i> open;
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++)
            if (c.get({x, y}) != Tile::Floor) {
                dist[size_t(y * c.w + x)] = 0;
                open.push_back({x, y});
            }
    while (!open.empty()) {
        Vec2i p = open.front();
        open.pop_front();
        for (Vec2i d : DIRS8) {
            Vec2i n = p + d;
            if (!c.inside(n) || dist[size_t(n.y * c.w + n.x)] != -1) continue;
            dist[size_t(n.y * c.w + n.x)] = dist[size_t(p.y * c.w + p.x)] + 1;
            open.push_back(n);
        }
    }
    return dist;
}

/// Brings clearance up to date once the cells given stop being floor. New
/// water only brings cells nearer to something, so it's enough to spread
/// out from the new cells while that shortens the way.
void lower(const Chunk& c, std::vector<int>& dist, const std::vector<Vec2i>& cells) {
    std::deque<Vec2i> open;
    for (Vec2i p : cells) {
        dist[size_t(p.y * c.w + p.x)] = 0;
        open.push_back(p);
    }
    while (!open.empty()) {
        Vec2i p = open.front();
        open.pop_front();
        const int next = dist[size_t(p.y * c.w + p.x)] + 1;
        for (Vec2i d : DIRS8) {
            Vec2i n = p + d;
            if (!c.inside(n) || dist[size_t(n.y * c.w + n.x)] <= next) continue;
            dist[size_t(n.y * c.w + n.x)] = next;
            open.push_back(n);
        }
    }
}

} // namespace

// Each pool is a whole outline or nothing: one that would come too close
// to anything is tried again elsewhere, at another size or turn.
// Clearance is worked out once and lowered round each new pool, as working
// it out afresh each time is slow on big chunks.
void pools(Chunk& c, Rng& rng, int count) {
    std::vector<int> clear = clearance(c);
    for (int i = 0; i < count; i++) {
        auto open = [&](Vec2i p) { return c.inside(p) && clear[size_t(p.y * c.w + p.x)] > POOL_RING; };
        std::vector<Vec2i> seeds;
        for (int y = 1; y < c.h - 1; y++)
            for (int x = 1; x < c.w - 1; x++)
                if (open({x, y})) seeds.push_back({x, y});
        if (seeds.empty()) return;

        for (int t = 0; t < POOL_TRIES; t++) {
            const Outline o = randomOutline(rng);
            // Quarter turns only, so straight edges stay straight.
            constexpr int COS[] = {1, 0, -1, 0}, SIN[] = {0, 1, 0, -1};
            const int turn = rng.range(0, 3), cs = COS[turn], sn = SIN[turn];
            const bool mirror = rng.chance(0.5f);
            const Vec2i mid = rng.pick(seeds);
            const int reach = int(std::ceil(o.size));
            std::vector<Vec2i> cells;
            bool fits = true;
            for (int dy = -reach; dy <= reach && fits; dy++)
                for (int dx = -reach; dx <= reach && fits; dx++) {
                    float lx = float(dx * cs + dy * sn), ly = float(-dx * sn + dy * cs);
                    if (!o.covers(mirror ? -lx : lx, ly)) continue;
                    Vec2i p = mid + Vec2i{dx, dy};
                    fits = open(p);
                    cells.push_back(p);
                }
            if (!fits) continue;
            for (Vec2i p : cells) c.set(p, Tile::Water);
            lower(c, clear, cells);
            break;
        }
    }
}

// Worked out along u, the way it runs, and v, across it.
void canal(Chunk& c, Rng& rng) {
    const int water = rng.range(CANAL_WIDTH_MIN, CANAL_WIDTH_MAX);
    const int quay = rng.range(QUAY_WIDTH_MIN, QUAY_WIDTH_MAX);
    const int span = water + 2 * quay;
    const bool alongX = rng.chance(0.5f);
    const int len = alongX ? c.w : c.h, breadth = alongX ? c.h : c.w;
    auto at = [&](int u, int v) { return alongX ? Vec2i{u, v} : Vec2i{v, u}; };

    std::vector<int> starts;
    for (int v = CANAL_MARGIN; v + span <= breadth - CANAL_MARGIN; v++) {
        // Its ends meet the border where there is no door, nor one beside,
        // and all of it between lies inside the footprint.
        bool ok = true;
        for (int k = v - 1; k <= v + span && ok; k++)
            ok = c.get(at(0, k)) == Tile::Wall && c.get(at(len - 1, k)) == Tile::Wall;
        Vec2i lo = at(1, v - 1), hi = at(len - 2, v + span);
        ok = ok && c.interiorRect(lo.x, lo.y, hi.x - lo.x + 1, hi.y - lo.y + 1);
        if (ok) starts.push_back(v);
    }
    if (starts.empty()) return;
    const int v0 = rng.pick(starts);

    const Floor quayFloor = randomFloor(rng);
    for (int u = 1; u < len - 1; u++)
        for (int k = 0; k < span; k++) {
            Vec2i p = at(u, v0 + k);
            if (k >= quay && k < quay + water) {
                c.set(p, Tile::Water);
            } else {
                c.set(p, Tile::Floor);
                c.setFloor(p, quayFloor);
            }
        }
}

void flood(Chunk& c) {
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++)
            if (c.interior({x, y}) && c.get({x, y}) == Tile::Wall) c.set({x, y}, Tile::Water);
}

} // namespace gen
