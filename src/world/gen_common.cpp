#include "world/gen.h"
#include <algorithm>
#include <deque>

namespace gen {

namespace {

/// Breadth-first through the interior, walls included but not water, from
/// the start cells to the first cell that is a goal. Returns that path, or
/// nothing.
template <class Goal>
std::vector<Vec2i> dig(const Chunk& c, const std::vector<Vec2i>& starts, Goal goal) {
    std::vector<int> from(size_t(c.w * c.h), -2);
    std::deque<Vec2i> open;
    for (Vec2i s : starts) {
        from[size_t(s.y * c.w + s.x)] = -1;
        open.push_back(s);
    }
    while (!open.empty()) {
        Vec2i p = open.front();
        open.pop_front();
        if (goal(p)) {
            std::vector<Vec2i> path;
            for (int i = p.y * c.w + p.x; i != -1; i = from[size_t(i)])
                path.push_back({i % c.w, i / c.w});
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (Vec2i d : DIRS4) {
            Vec2i n = p + d;
            if (!c.interior(n) || c.get(n) == Tile::Water || c.get(n) == Tile::Statue ||
                from[size_t(n.y * c.w + n.x)] != -2)
                continue;

            from[size_t(n.y * c.w + n.x)] = p.y * c.w + p.x;
            open.push_back(n);
        }
    }
    return {};
}

void carveLine(Chunk& c, Vec2i a, Vec2i b, int width) {
    Vec2i d{(b.x > a.x) - (b.x < a.x), (b.y > a.y) - (b.y < a.y)};
    for (Vec2i p = a;; p = p + d) {
        carveBrush(c, p, width);
        if (p == b) break;
    }
}

} // namespace

Floor randomFloor(Rng& rng) {
    return {FloorPattern(rng.range(0, int(FloorPattern::Count) - 1)),
            uint8_t(rng.range(0, FLOOR_PALETTE_COUNT - 1))};
}

WallStyle randomWall(Rng& rng) {
    return {WallPattern(rng.range(0, int(WallPattern::Count) - 1)),
            uint8_t(rng.range(0, WALL_PALETTE_COUNT - 1))};
}

void paintRect(Chunk& c, int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (c.interior({xx, yy}) && c.get({xx, yy}) == Tile::Floor) c.setFloor({xx, yy}, c.paint);
}

void carve(Chunk& c, Vec2i p) {
    if (!c.interior(p) || c.get(p) != Tile::Wall) return;
    c.set(p, Tile::Floor);
    c.setFloor(p, c.paint);
}

void carveRect(Chunk& c, int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) carve(c, {xx, yy});
}

void carveBrush(Chunk& c, Vec2i p, int width) {
    int lo = -(width - 1) / 2;
    carveRect(c, p.x + lo, p.y + lo, width, width);
}

void carveCorridor(Chunk& c, Rng& rng, Vec2i a, Vec2i b, int width) {
    Vec2i corner = rng.chance(0.5f) ? Vec2i{b.x, a.y} : Vec2i{a.x, b.y};
    carveLine(c, a, corner, width);
    carveLine(c, corner, b, width);
}

void connectDoor(Chunk& c, Vec2i inner, int width) {
    if (!c.interior(inner) || c.get(inner) == Tile::Water) return;
    auto path = dig(c, {inner}, [&](Vec2i p) { return c.get(p) == Tile::Floor; });
    carveBrush(c, inner, width);
    for (Vec2i p : path) carveBrush(c, p, width);
}

void connectRegions(Chunk& c, int width) {
    const std::vector<int> side = sides(c);
    const int sideCount = *std::max_element(side.begin(), side.end()) + 1;
    // Each pass joins the biggest area that still has another on its side
    // of the water to its nearest neighbour there.
    for (int pass = 0; pass < 1000; pass++) {
        std::vector<int> label(size_t(c.w * c.h), -1);
        std::vector<int> sizes, regionSide;
        for (int y = 1; y < c.h - 1; y++)
            for (int x = 1; x < c.w - 1; x++) {
                if (c.get({x, y}) != Tile::Floor || label[size_t(y * c.w + x)] != -1) continue;
                int id = int(sizes.size());
                sizes.push_back(0);
                regionSide.push_back(side[size_t(y * c.w + x)]);
                std::vector<Vec2i> stack{{x, y}};
                label[size_t(y * c.w + x)] = id;
                while (!stack.empty()) {
                    Vec2i p = stack.back();
                    stack.pop_back();
                    sizes[size_t(id)]++;
                    for (Vec2i d : DIRS4) {
                        Vec2i n = p + d;
                        if (!c.interior(n) || c.get(n) != Tile::Floor) continue;
                        if (label[size_t(n.y * c.w + n.x)] != -1) continue;
                        label[size_t(n.y * c.w + n.x)] = id;
                        stack.push_back(n);
                    }
                }
            }
        std::vector<int> perSide(size_t(std::max(sideCount, 0)), 0);
        for (int s : regionSide) perSide[size_t(s)]++;
        int main = -1;
        for (int id = 0; id < int(sizes.size()); id++)
            if (perSide[size_t(regionSide[size_t(id)])] > 1 && (main < 0 || sizes[size_t(id)] > sizes[size_t(main)]))
                main = id;
        if (main < 0) return;
        std::vector<Vec2i> starts;
        for (int y = 1; y < c.h - 1; y++)
            for (int x = 1; x < c.w - 1; x++)
                if (label[size_t(y * c.w + x)] == main) starts.push_back({x, y});
        auto path = dig(c, starts, [&](Vec2i p) {
            int l = label[size_t(p.y * c.w + p.x)];
            return l != -1 && l != main;
        });
        if (path.empty()) return;
        for (Vec2i p : path) carveBrush(c, p, width);
    }
}

std::vector<int> sides(const Chunk& c) {
    std::vector<int> side(size_t(c.w * c.h), -1);
    int next = 0;
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++) {
            if (c.get({x, y}) == Tile::Water || side[size_t(y * c.w + x)] != -1) continue;
            std::vector<Vec2i> stack{{x, y}};
            side[size_t(y * c.w + x)] = next;
            while (!stack.empty()) {
                Vec2i p = stack.back();
                stack.pop_back();
                for (Vec2i d : DIRS4) {
                    Vec2i n = p + d;
                    if (!c.interior(n) || c.get(n) == Tile::Water || side[size_t(n.y * c.w + n.x)] != -1) continue;
                    side[size_t(n.y * c.w + n.x)] = next;
                    stack.push_back(n);
                }
            }
            next++;
        }
    return side;
}

void squareRoom(Chunk& c) {
    carveRect(c, 1, 1, c.w - 2, c.h - 2);
}

} // namespace gen
