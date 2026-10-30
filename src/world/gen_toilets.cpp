#include "world/gen.h"
#include "world/world.h"

// Toilets are rare: one now and then in a room network, against a straight
// stretch of wall with floor all round the rest of it so it blocks nothing,
// and one now and then in a maze, filling the end of a dead end. A maze may
// also have a restroom: a tiled hall cut through it, its walls lined with
// toilets. A toilet plaza has them all round its walls and columns.

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
