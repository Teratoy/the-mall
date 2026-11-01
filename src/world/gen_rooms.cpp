#include "world/gen.h"
#include "world/gen_tuning.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>

namespace gen {

namespace {

/// Rooms wanted per slot.
tune::Int ROOMS_MIN{"rooms", "ROOMS_MIN"}, ROOMS_MAX{"rooms", "ROOMS_MAX"};
tune::Int ROOM_MIN{"rooms", "ROOM_MIN"};
/// The biggest room side is rolled per chunk in this range, so some
/// networks are many small rooms and others a few big halls.
tune::Int ROOM_MAX_LO{"rooms", "ROOM_MAX_LO"}, ROOM_MAX_HI{"rooms", "ROOM_MAX_HI"};
/// Least wall between two rooms' boxes.
tune::Int ROOM_GAP{"rooms", "ROOM_GAP"};
tune::Int CORRIDOR_MIN{"rooms", "CORRIDOR_MIN"}, CORRIDOR_MAX{"rooms", "CORRIDOR_MAX"};
/// Chance each room gets one more corridor beyond the spanning tree.
tune::Float LOOP_CHANCE{"rooms", "LOOP_CHANCE"};
/// Each chunk rolls this many floors and gives each room one of them, so
/// rooms repeat each other's floors like shops along one wing.
tune::Int ROOM_FLOORS_MIN{"rooms", "ROOM_FLOORS_MIN"}, ROOM_FLOORS_MAX{"rooms", "ROOM_FLOORS_MAX"};
/// The same for walls. Walls further from a room than WALL_REACH keep the
/// chunk's own style, and so do those along corridors.
tune::Int ROOM_WALLS_MIN{"rooms", "ROOM_WALLS_MIN"}, ROOM_WALLS_MAX{"rooms", "ROOM_WALLS_MAX"};
tune::Int WALL_REACH{"rooms", "WALL_REACH"};
tune::Float CANAL_CHANCE{"rooms", "CANAL_CHANCE"};
/// Pools tried per slot; they only fit in rooms roomy enough.
tune::Int POOLS_MAX{"rooms", "POOLS_MAX"};
/// Rooms at least this big each way may get a grid of pillars, this often.
tune::Int PILLAR_ROOM_MIN{"rooms", "PILLAR_ROOM_MIN"};
tune::Float PILLAR_CHANCE{"rooms", "PILLAR_CHANCE"};
/// Pillars are 1 or 2 cells across, with this much floor between them,
/// keep this far in from the room's box, and leave its centre clear.
tune::Int PILLAR_GAP_MIN{"rooms", "PILLAR_GAP_MIN"}, PILLAR_GAP_MAX{"rooms", "PILLAR_GAP_MAX"};
tune::Int PILLAR_MARGIN{"rooms", "PILLAR_MARGIN"};
tune::Int PILLAR_CLEAR{"rooms", "PILLAR_CLEAR"};
/// Rectangular rooms this long may get a wall across them, this often,
/// with one or two gaps of this width.
tune::Int PARTITION_ROOM_MIN{"rooms", "PARTITION_ROOM_MIN"};
tune::Float PARTITION_CHANCE{"rooms", "PARTITION_CHANCE"};
tune::Int PARTITION_GAP{"rooms", "PARTITION_GAP"};

/// Donut is a ring round a solid core; U and T are bars, turned any way.
enum class Shape { Rect, L, Cross, Disk, Octagon, Donut, U, T, Rounded, Diamond, Count };

using Room = RoomBox;

/// What a room was carved as, kept for furnishing it once corridors are dug.
struct Plan {
    Shape shape;
    WallStyle wall;
    /// Donut: its core, to be walled up again.
    int cx = 0, cy = 0, cw = 0, ch = 0;
};

int dist2(Vec2i a, Vec2i b) {
    Vec2i d = a - b;
    return d.x * d.x + d.y * d.y;
}

Vec2i carveShape(Chunk& c, Rng& rng, Plan& plan, int x, int y, int w, int h) {
    Vec2i mid{x + w / 2, y + h / 2};
    switch (plan.shape) {
    case Shape::Rect:
    case Shape::Count:
        carveRect(c, x, y, w, h);
        return mid;
    case Shape::Donut: {
        // Too small for a core with a ring round it wide enough to walk: a plain room.
        int t = std::max(3, std::min(w, h) / 4);
        carveRect(c, x, y, w, h);
        if (w - 2 * t < 3 || h - 2 * t < 3) {
            plan.shape = Shape::Rect;
            return mid;
        }
        plan.cx = x + t, plan.cy = y + t, plan.cw = w - 2 * t, plan.ch = h - 2 * t;
        return {x + t / 2, y + h / 2};
    }
    case Shape::U: {
        int t = std::max(4, std::min(w, h) / 3);
        switch (rng.range(0, 3)) {
        case 0: // open at the top
            carveRect(c, x, y, t, h), carveRect(c, x + w - t, y, t, h), carveRect(c, x, y + h - t, w, t);
            return {x + w / 2, y + h - 1 - t / 2};
        case 1: // at the bottom
            carveRect(c, x, y, t, h), carveRect(c, x + w - t, y, t, h), carveRect(c, x, y, w, t);
            return {x + w / 2, y + t / 2};
        case 2: // on the left
            carveRect(c, x, y, w, t), carveRect(c, x, y + h - t, w, t), carveRect(c, x + w - t, y, t, h);
            return {x + w - 1 - t / 2, y + h / 2};
        default: // on the right
            carveRect(c, x, y, w, t), carveRect(c, x, y + h - t, w, t), carveRect(c, x, y, t, h);
            return {x + t / 2, y + h / 2};
        }
    }
    case Shape::T: {
        int t = std::max(4, std::min(w, h) / 3);
        switch (rng.range(0, 3)) {
        case 0: // the bar along the top
            carveRect(c, x, y, w, t), carveRect(c, x + (w - t) / 2, y, t, h);
            return {x + w / 2, y + t / 2};
        case 1: // along the bottom
            carveRect(c, x, y + h - t, w, t), carveRect(c, x + (w - t) / 2, y, t, h);
            return {x + w / 2, y + h - 1 - t / 2};
        case 2: // down the left
            carveRect(c, x, y, t, h), carveRect(c, x, y + (h - t) / 2, w, t);
            return {x + t / 2, y + h / 2};
        default: // down the right
            carveRect(c, x + w - t, y, t, h), carveRect(c, x, y + (h - t) / 2, w, t);
            return {x + w - 1 - t / 2, y + h / 2};
        }
    }
    case Shape::Rounded: {
        float r = std::max(2, std::min(w, h) / 3);
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) {
                float px = xx + 0.5f, py = yy + 0.5f;
                float dx = px - std::clamp(px, x + r, x + w - r), dy = py - std::clamp(py, y + r, y + h - r);
                if (dx * dx + dy * dy <= r * r) carve(c, {xx, yy});
            }
        return mid;
    }
    case Shape::Diamond:
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) {
                float dx = std::fabs(xx + 0.5f - (x + w / 2.0f)) / (w / 2.0f);
                float dy = std::fabs(yy + 0.5f - (y + h / 2.0f)) / (h / 2.0f);
                if (dx + dy <= 1.0f) carve(c, {xx, yy});
            }
        return mid;
    case Shape::L: {
        int t = std::max(4, std::min(w, h) / 2);
        int by = rng.chance(0.5f) ? y : y + h - t;
        int bx = rng.chance(0.5f) ? x : x + w - t;
        carveRect(c, x, by, w, t);
        carveRect(c, bx, y, t, h);
        return {bx + t / 2, by + t / 2};
    }
    case Shape::Cross: {
        int t = std::max(4, std::min(w, h) / 3);
        carveRect(c, x, y + (h - t) / 2, w, t);
        carveRect(c, x + (w - t) / 2, y, t, h);
        return mid;
    }
    case Shape::Disk: {
        float r = w / 2.0f;
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) {
                float dx = xx + 0.5f - (x + r), dy = yy + 0.5f - (y + r);
                if (dx * dx + dy * dy <= r * r) carve(c, {xx, yy});
            }
        return mid;
    }
    case Shape::Octagon: {
        int cut = std::min(w, h) / 4;
        for (int yy = y; yy < y + h; yy++) {
            int inset = std::max(0, cut - std::min(yy - y, y + h - 1 - yy));
            carveRect(c, x + inset, yy, w - 2 * inset, 1);
        }
        return mid;
    }
    }
    return mid;
}

/// Walls up floor, in the room's style.
void wallUp(Chunk& c, Vec2i p, WallStyle style) {
    if (!c.interior(p) || c.get(p) != Tile::Floor) return;
    c.set(p, Tile::Wall);
    c.setWall(p, style);
}

// A grid of pillars, centred in the room's box. Each goes only where it
// and every cell round it are floor, so none touches another or a wall,
// and walking round them joins up whatever they stand in.
void pillars(Chunk& c, Rng& rng, const Room& r, WallStyle style) {
    const int size = rng.range(1, 2), pitch = size + rng.range(PILLAR_GAP_MIN, PILLAR_GAP_MAX);
    const int spanX = r.w - 2 * PILLAR_MARGIN - size, spanY = r.h - 2 * PILLAR_MARGIN - size;
    if (spanX < 0 || spanY < 0) return;
    const int ox = r.x + PILLAR_MARGIN + (spanX % pitch) / 2, oy = r.y + PILLAR_MARGIN + (spanY % pitch) / 2;
    for (int py = oy; py <= oy + spanY - spanY % pitch; py += pitch)
        for (int px = ox; px <= ox + spanX - spanX % pitch; px += pitch) {
            bool fits = std::abs(px + size / 2 - r.centre.x) > PILLAR_CLEAR ||
                        std::abs(py + size / 2 - r.centre.y) > PILLAR_CLEAR;
            for (int y = py - 1; y <= py + size && fits; y++)
                for (int x = px - 1; x <= px + size && fits; x++) fits = c.get({x, y}) == Tile::Floor;
            if (!fits) continue;
            for (int y = py; y < py + size; y++)
                for (int x = px; x < px + size; x++) wallUp(c, {x, y}, style);
        }
}

// A wall a third of the way across the long side, so it misses the centre
// corridors aim for, with one or two gaps to walk through.
void partition(Chunk& c, Rng& rng, const Room& r, WallStyle style) {
    const bool upright = r.w >= r.h;
    const int len = upright ? r.h : r.w;
    const int at = upright ? r.x + r.w * rng.range(1, 2) / 3 : r.y + r.h * rng.range(1, 2) / 3;
    std::vector<bool> gap(size_t(len), false);
    for (int g = rng.range(1, 2); g > 0; g--) {
        int from = rng.range(0, len - PARTITION_GAP);
        for (int k = from; k < from + PARTITION_GAP; k++) gap[size_t(k)] = true;
    }
    for (int k = 0; k < len; k++)
        if (!gap[size_t(k)]) wallUp(c, upright ? Vec2i{at, r.y + k} : Vec2i{r.x + k, at}, style);
}

void furnish(Chunk& c, Rng& rng, const Room& r, const Plan& plan) {
    if (plan.shape == Shape::Donut) {
        for (int y = plan.cy; y < plan.cy + plan.ch; y++)
            for (int x = plan.cx; x < plan.cx + plan.cw; x++) wallUp(c, {x, y}, plan.wall);
        return;
    }
    bool open = plan.shape == Shape::Rect || plan.shape == Shape::Rounded || plan.shape == Shape::Octagon ||
                plan.shape == Shape::Disk || plan.shape == Shape::Diamond;
    if (open && std::min(r.w, r.h) >= PILLAR_ROOM_MIN && rng.chance(PILLAR_CHANCE))
        pillars(c, rng, r, plan.wall);
    else if (plan.shape == Shape::Rect && std::max(r.w, r.h) >= PARTITION_ROOM_MIN && rng.chance(PARTITION_CHANCE))
        partition(c, rng, r, plan.wall);
}

/// Gives the walls near a room's floor the room's style. Only floor in the
/// room's box counts, so a neighbour's floor never claims them.
void styleWalls(Chunk& c, const Room& r, WallStyle style) {
    for (int y = r.y - WALL_REACH; y < r.y + r.h + WALL_REACH; y++)
        for (int x = r.x - WALL_REACH; x < r.x + r.w + WALL_REACH; x++) {
            if (!c.inside({x, y}) || c.get({x, y}) != Tile::Wall) continue;
            bool near = false;
            for (int dy = -WALL_REACH; dy <= WALL_REACH && !near; dy++)
                for (int dx = -WALL_REACH; dx <= WALL_REACH && !near; dx++) {
                    Vec2i q{x + dx, y + dy};
                    bool inBox = q.x >= r.x && q.y >= r.y && q.x < r.x + r.w && q.y < r.y + r.h;
                    near = inBox && c.get(q) == Tile::Floor;
                }
            if (near) c.setWall({x, y}, style);
        }
}

} // namespace

std::vector<RoomBox> rooms(Chunk& c, Rng& rng) {
    const int want = rng.range(ROOMS_MIN, ROOMS_MAX) * c.slotCount();
    const int maxSide = rng.range(ROOM_MAX_LO, ROOM_MAX_HI);
    const int corridor = rng.range(CORRIDOR_MIN, CORRIDOR_MAX);
    std::vector<Floor> floors(size_t(rng.range(ROOM_FLOORS_MIN, ROOM_FLOORS_MAX)));
    for (Floor& f : floors) f = randomFloor(rng);
    const Floor corridorFloor = randomFloor(rng);
    std::vector<WallStyle> walls(size_t(rng.range(ROOM_WALLS_MIN, ROOM_WALLS_MAX)));
    for (WallStyle& w : walls) w = randomWall(rng);

    std::vector<Room> rooms;
    std::vector<Plan> plans;
    for (int tries = 0; tries < want * 40 && int(rooms.size()) < want; tries++) {
        Shape s = Shape(rng.range(0, int(Shape::Count) - 1));
        int w = rng.range(ROOM_MIN, maxSide);
        int h = s == Shape::Disk ? w : rng.range(ROOM_MIN, maxSide);
        int x = rng.range(2, c.w - 3 - w);
        int y = rng.range(2, c.h - 3 - h);
        bool clash = std::any_of(rooms.begin(), rooms.end(), [&](const Room& o) {
            return x < o.x + o.w + ROOM_GAP && o.x < x + w + ROOM_GAP &&
                   y < o.y + o.h + ROOM_GAP && o.y < y + h + ROOM_GAP;
        });
        // The room and the wall round it lie inside the footprint.
        if (clash || !c.interiorRect(x - 1, y - 1, w + 2, h + 2)) continue;
        c.paint = rng.pick(floors);
        Plan plan{s, rng.pick(walls)};
        rooms.push_back({x, y, w, h, carveShape(c, rng, plan, x, y, w, h)});
        plans.push_back(plan);
        styleWalls(c, rooms.back(), plan.wall);
    }
    // Pillars, partitions and cores go up once the corridors are dug, so no
    // corridor cuts through them; one that ran through a core goes round it.
    auto furnishAll = [&] {
        for (size_t i = 0; i < rooms.size(); i++) furnish(c, rng, rooms[i], plans[i]);
    };
    // Corridors, and whatever joins things up after, get the corridor floor.
    c.paint = corridorFloor;
    if (rooms.size() < 2) {
        furnishAll();
        return rooms;
    }

    // Spanning tree: keep joining the nearest room not yet joined.
    std::vector<bool> joined(rooms.size(), false);
    joined[0] = true;
    for (size_t k = 1; k < rooms.size(); k++) {
        int best = INT_MAX;
        size_t a = 0, b = 0;
        for (size_t i = 0; i < rooms.size(); i++) {
            if (!joined[i]) continue;
            for (size_t j = 0; j < rooms.size(); j++) {
                if (joined[j]) continue;
                int d = dist2(rooms[i].centre, rooms[j].centre);
                if (d < best) best = d, a = i, b = j;
            }
        }
        joined[b] = true;
        carveCorridor(c, rng, rooms[a].centre, rooms[b].centre, corridor);
    }

    // A few loops, each to one of the room's three nearest.
    for (size_t i = 0; i < rooms.size(); i++) {
        if (!rng.chance(LOOP_CHANCE)) continue;
        std::vector<size_t> near;
        for (size_t j = 0; j < rooms.size(); j++)
            if (j != i) near.push_back(j);
        std::sort(near.begin(), near.end(), [&](size_t p, size_t q) {
            return dist2(rooms[i].centre, rooms[p].centre) < dist2(rooms[i].centre, rooms[q].centre);
        });
        near.resize(std::min<size_t>(3, near.size()));
        carveCorridor(c, rng, rooms[i].centre, rooms[rng.pick(near)].centre, corridor);
    }

    furnishAll();
    if (rng.chance(CANAL_CHANCE)) canal(c, rng);

    pools(c, rng, rng.range(0, POOLS_MAX) * c.slotCount());
    return rooms;
}

} // namespace gen
