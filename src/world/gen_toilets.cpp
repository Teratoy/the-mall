#include "world/gen.h"
#include "world/world.h"

// Toilets are rare: one now and then in a room network, against a straight
// stretch of wall with floor all round the rest of it so it blocks nothing,
// and one now and then in a maze, filling the end of a dead end. A maze may
// also have a restroom: a tiled hall cut through it, its walls lined with
// toilets. A toilet plaza has them all round its walls and columns. A data
// centre has a washroom or two: a little tiled room off a corridor, walled
// in, with a row of toilets along its back wall (user).

namespace {

/// Chance a room network has a toilet, or two in a big one; and a maze.
constexpr float ROOMS_TOILET_CHANCE = 0.2f;
constexpr float MAZE_TOILET_CHANCE = 0.15f;
/// Tries at a spot for each.
constexpr int TOILET_TRIES = 300;

constexpr Vec2i SIDES[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

/// Chance per slot a maze has a restroom, its size either way round, and
/// the cells between its toilets.
constexpr float RESTROOM_CHANCE = 0.4f;
constexpr int RESTROOM_LONG_MIN = 14, RESTROOM_LONG_MAX = 22;
constexpr int RESTROOM_SHORT_MIN = 11, RESTROOM_SHORT_MAX = 15;
constexpr int RESTROOM_SPACING = 2;
/// Floor kept between the middle partition's toilets and the hall's edge.
constexpr int RESTROOM_AISLE = 3;
/// Restrooms keep at least this far apart.
constexpr int RESTROOM_GAP = 4;
constexpr int RESTROOM_TRIES = 40;
/// Restroom floors are tiles in clean colours.
constexpr FloorPattern RESTROOM_PATTERNS[] = {FloorPattern::Grid, FloorPattern::BigGrid, FloorPattern::Chess,
                                              FloorPattern::ChessSmall};
constexpr uint8_t RESTROOM_PALETTES[] = {2, 5, 7};
/// Share of the spots that fit a toilet in a toilet plaza that get one.
constexpr float PLAZA_TOILET_FILL = 0.7f;
/// A data centre's washroom: this many toilets, a cell apart along its
/// back wall, and this many rows of floor in front of them.
constexpr int WASHROOM_TOILETS_MIN = 2, WASHROOM_TOILETS_MAX = 3;
constexpr int WASHROOM_DEPTH = 3;
constexpr int WASHROOM_TRIES = 200;

} // namespace

// Carving more floor into a maze never cuts anything off, so the hall can
// go anywhere it fits. Toilets stand along each wall a cell or so apart,
// each with wall behind it and behind both its neighbours, so none of them
// narrows a way in. Down the middle runs a partition with toilets back to
// back on both sides, floor all round it, as the walls are often more
// corridor than wall.
void World::placeRestrooms(Chunk& c) {
    struct Box {
        int x, y, w, h;
    };
    std::vector<Box> boxes;
    int want = 0;
    for (int i = 0; i < c.slotCount(); i++) want += rng_.chance(RESTROOM_CHANCE);
    for (int t = 0; t < RESTROOM_TRIES && want > 0; t++) {
        int a = rng_.range(RESTROOM_LONG_MIN, RESTROOM_LONG_MAX), b = rng_.range(RESTROOM_SHORT_MIN, RESTROOM_SHORT_MAX);
        Box r{0, 0, a, b};
        if (rng_.chance(0.5f)) std::swap(r.w, r.h);
        if (r.w > c.w - 2 || r.h > c.h - 2) continue;
        r.x = rng_.range(1, c.w - 1 - r.w);
        r.y = rng_.range(1, c.h - 1 - r.h);
        bool clear = c.interiorRect(r.x, r.y, r.w, r.h);
        for (const Box& o : boxes)
            clear = clear && (r.x >= o.x + o.w + RESTROOM_GAP || o.x >= r.x + r.w + RESTROOM_GAP ||
                              r.y >= o.y + o.h + RESTROOM_GAP || o.y >= r.y + r.h + RESTROOM_GAP);
        for (int y = r.y; y < r.y + r.h && clear; y++)
            for (int x = r.x; x < r.x + r.w && clear; x++)
                clear = !behindStatues_.count(c.origin() + Vec2i{x, y});
        if (!clear) continue;
        boxes.push_back(r);
        want--;

        Floor before = c.paint;
        c.paint = {rng_.pick(RESTROOM_PATTERNS), rng_.pick(RESTROOM_PALETTES)};
        gen::carveRect(c, r.x, r.y, r.w, r.h);
        gen::paintRect(c, r.x, r.y, r.w, r.h);
        c.paint = before;

        // Round each wall, corners left out, back being outward.
        auto line = [&](Vec2i from, Vec2i step, int n, Vec2i back) {
            int since = RESTROOM_SPACING;
            for (int i = 1; i < n - 1; i++, since++) {
                Vec2i l = from + step * i;
                if (since < RESTROOM_SPACING || c.get(l) != Tile::Floor) continue;
                bool fits = c.get(l + back) == Tile::Wall && c.get(l + back + step) == Tile::Wall &&
                            c.get(l + back - step) == Tile::Wall && c.get(l - back) == Tile::Floor;
                if (!fits) continue;
                c.set(l, Tile::Toilet);
                toilets_[c.origin() + l] = {c.origin() + l - back, 0.0f, uint32_t(rng_.next())};
                since = 0;
            }
        };
        line({r.x, r.y}, {1, 0}, r.w, {0, -1});
        line({r.x, r.y + r.h - 1}, {1, 0}, r.w, {0, 1});
        line({r.x, r.y}, {0, 1}, r.h, {-1, 0});
        line({r.x + r.w - 1, r.y}, {0, 1}, r.h, {1, 0});

        // The partition, along the long way.
        bool across = r.w >= r.h;
        Vec2i step = across ? Vec2i{1, 0} : Vec2i{0, 1}, out = across ? Vec2i{0, 1} : Vec2i{1, 0};
        int length = (across ? r.w : r.h) - 2 * RESTROOM_AISLE;
        Vec2i mid = across ? Vec2i{r.x + RESTROOM_AISLE, r.y + r.h / 2} : Vec2i{r.x + r.w / 2, r.y + RESTROOM_AISLE};
        for (int i = 0; i < length; i++) c.set(mid + step * i, Tile::Wall);
        // Its ends stay bare, so the line functions see wall past them.
        line(mid - out, step, length, out);
        line(mid + out, step, length, -out);
    }
}

// The washroom's front wall is the corridor's own, so it sits flush with
// it and the doorway in its middle opens straight out; the rest of it, its
// walls included, is cut from solid wall, so it opens nowhere else.
std::vector<gen::RoomBox> World::placeWashrooms(Chunk& c, Rng& dice, int count) {
    std::vector<gen::RoomBox> made;
    for (int t = 0; t < WASHROOM_TRIES * count && int(made.size()) < count; t++) {
        const int toilets = dice.range(WASHROOM_TOILETS_MIN, WASHROOM_TOILETS_MAX);
        // Its long side along the corridor: out is from the back wall to the doorway.
        const Vec2i out = SIDES[size_t(dice.range(0, 3))], along{out.y != 0 ? 1 : 0, out.x != 0 ? 1 : 0};
        const int len = 2 * toilets - 1, w = along.x ? len : WASHROOM_DEPTH, h = along.x ? WASHROOM_DEPTH : len;
        const int x = dice.range(2, c.w - 3 - w), y = dice.range(2, c.h - 3 - h);
        bool clear = true;
        for (int yy = y - 1; yy <= y + h && clear; yy++)
            for (int xx = x - 1; xx <= x + w && clear; xx++)
                clear = c.interior({xx, yy}) && c.get({xx, yy}) == Tile::Wall;
        if (!clear) continue;
        // The back row, its first cell, and the doorway, the middle of the front wall.
        const Vec2i back = out.x > 0 ? Vec2i{x, y} : out.x < 0 ? Vec2i{x + w - 1, y} : out.y > 0 ? Vec2i{x, y}
                                                                                                 : Vec2i{x, y + h - 1};
        const Vec2i door = back + along * (toilets - 1) + out * WASHROOM_DEPTH;
        // Corridor all along the front wall's outside, so it reads as one of its walls.
        for (int i = -1; i <= len && clear; i++) {
            Vec2i p = back + along * i + out * (WASHROOM_DEPTH + 1);
            clear = c.interior(p) && c.get(p) == Tile::Floor;
        }
        if (!clear) continue;

        Floor before = c.paint;
        c.paint = {dice.pick(RESTROOM_PATTERNS), dice.pick(RESTROOM_PALETTES)};
        gen::carveRect(c, x, y, w, h);
        gen::carve(c, door);
        c.paint = before;
        for (int i = 0; i < toilets; i++) {
            Vec2i l = back + along * (2 * i);
            c.set(l, Tile::Toilet);
            toilets_[c.origin() + l] = {c.origin() + l + out, 0.0f, uint32_t(dice.next())};
        }
        made.push_back({x, y, w, h, door});
    }
    return made;
}

// A toilet has wall behind it and floor on its sides, all three cells in
// front and the one past that, so walking round it from one side to the
// other is always open, it never stands beside another, and two never face
// each other across a narrow corridor. Spots are taken in a shuffled order.
void World::placePlazaToilets(Chunk& c) {
    std::vector<Vec2i> spots;
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++) spots.push_back({x, y});
    rng_.shuffle(spots);
    auto floorAt = [&](Vec2i l) { return c.interior(l) && c.get(l) == Tile::Floor && !behindStatues_.count(c.origin() + l); };
    for (Vec2i l : spots) {
        if (!floorAt(l)) continue;
        for (Vec2i back : SIDES) {
            Vec2i front = -back, side{back.y, back.x};
            if (c.get(l + back) != Tile::Wall) continue;
            bool fits = true;
            for (Vec2i d : {side, -side, front, front + side, front - side, front * 2}) fits = fits && floorAt(l + d);
            if (!fits) continue;
            if (rng_.chance(PLAZA_TOILET_FILL)) {
                c.set(l, Tile::Toilet);
                toilets_[c.origin() + l] = {c.origin() + l + front, 0.0f, uint32_t(rng_.next())};
            }
            break;
        }
    }
}

void World::placeToilets(Chunk& c) {
    if (c.kind == ChunkKind::Maze) placeRestrooms(c);
    float chance = c.kind == ChunkKind::Rooms ? ROOMS_TOILET_CHANCE : c.kind == ChunkKind::Maze ? MAZE_TOILET_CHANCE : 0.0f;
    int want = 0;
    for (int i = 0; i < c.slotCount(); i++) want += rng_.chance(chance);
    auto floorAt = [&](Vec2i l) { return c.interior(l) && c.get(l) == Tile::Floor; };
    for (int t = 0; t < TOILET_TRIES && want > 0; t++) {
        Vec2i l{rng_.range(1, c.w - 2), rng_.range(1, c.h - 2)};
        if (!floorAt(l) || behindStatues_.count(c.origin() + l)) continue;
        for (Vec2i back : SIDES) {
            Vec2i front = -back, side{back.y, back.x};
            if (c.get(l + back) != Tile::Wall || !floorAt(l + front)) continue;
            bool fits;
            if (c.kind == ChunkKind::Maze) {
                // A dead end: walled on both sides as well.
                fits = c.get(l + side) == Tile::Wall && c.get(l - side) == Tile::Wall;
            } else {
                // A straight wall behind, floor round the rest.
                fits = c.get(l + back + side) == Tile::Wall && c.get(l + back - side) == Tile::Wall;
                for (Vec2i d : {side, -side, front + side, front - side}) fits = fits && floorAt(l + d);
            }
            if (!fits) continue;
            c.set(l, Tile::Toilet);
            toilets_[c.origin() + l] = {c.origin() + l + front, 0.0f, uint32_t(rng_.next())};
            want--;
            break;
        }
    }
}
