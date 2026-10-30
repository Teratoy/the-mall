#include "render/world_view.h"
#include "human/human.h"
#include "render/blood_art.h"
#include "render/bottle_art.h"
#include "render/floor_tiles.h"
#include "render/human_sprites.h"
#include "render/mess_art.h"
#include "render/statue_art.h"
#include "render/table_art.h"
#include "render/toilet_art.h"
#include "render/water_art.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace {

/// Pointing the way, the arm goes up or down past this much north or south
/// in it (sin of the angle): about 22 degrees off level.
constexpr float POINT_TILT = 0.38f;

/// The south face of a wall, seen from the floor below it.
constexpr int WALL_FACE_PX = 2;
constexpr Color DOOR = {128, 80, 38, 255};
constexpr Color DOOR_EDGE = {84, 50, 22, 255};
constexpr Color DOOR_OPEN = {54, 54, 54, 255};
/// A basin's stone rim, lit along its top and left, shaded along the rest.
constexpr Color BASIN_RIM = {164, 160, 150, 255};
constexpr Color BASIN_RIM_SHADE = {104, 100, 94, 255};
/// Glint frames a second. Remembered water holds still.
constexpr float WATER_FPS = 3.0f;
constexpr Color KEY = {232, 197, 71, 255};
constexpr Color CAKE_CHERRY = {200, 48, 58, 255};
constexpr Color CAKE_FROSTING = {236, 180, 196, 255};
constexpr Color CAKE_SPONGE = {176, 124, 80, 255};
/// Traps are drawn see-through, so they sit on any floor.
constexpr Color TRAP_HOLE = {0, 0, 0, 120};
constexpr Color SPIKE_TIP = {214, 214, 208, 255};
constexpr Color SPIKE_SHAFT = {132, 132, 128, 255};
constexpr Color PLATE_EDGE = {255, 255, 255, 48};
constexpr Color PLATE_DOWN = {0, 0, 0, 90};
constexpr Color DART = {226, 214, 172, 255};
/// Darts fly about chest high, this far above the middle of their row.
constexpr int DART_RISE_PX = 3;
/// A human just hit is drawn tinted with this.
constexpr Color HURT_TINT = {255, 90, 90, 255};
/// The viewer behind a statue shows through it, this faintly.
constexpr Color GHOST_TINT = {255, 255, 255, 110};
/// Remembered but out of sight: drawn at this brightness.
constexpr float MEMORY_DIM = 0.45f;
/// Statues are looked for this many rows below the screen, as the tallest
/// rise into it from there.
constexpr int STATUE_REACH = 28;

constexpr const char* KEY_GLYPH[CELL_PX] = {
    "......",
    "##....",
    "#.####",
    "##.#.#",
    "......",
    "......",
};

/// c is the cherry, f frosting, s sponge.
constexpr const char* CAKE_GLYPH[CELL_PX] = {
    "..c...",
    ".ffff.",
    "fsfsff",
    "ssssss",
    "ffffff",
    "ssssss",
};

/// o is a hole in the floor; t a spike's tip, s its shaft.
constexpr const char* SPIKES_GLYPH[CELL_PX] = {
    "......",
    ".o..o.",
    "......",
    "......",
    ".o..o.",
    "......",
};
constexpr const char* SPIKES_UP_GLYPH[CELL_PX] = {
    ".t..t.",
    ".s..s.",
    "......",
    ".t..t.",
    ".s..s.",
    "......",
};
/// e is the plate's edge; d the plate pressed down.
constexpr const char* PLATE_GLYPH[CELL_PX] = {
    "......",
    ".eeee.",
    ".e..e.",
    ".e..e.",
    ".eeee.",
    "......",
};
constexpr const char* PLATE_DOWN_GLYPH[CELL_PX] = {
    "......",
    ".eeee.",
    ".edde.",
    ".edde.",
    ".eeee.",
    "......",
};

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

Color dim(Color c, bool lit) {
    if (lit) return c;
    return {(unsigned char)(c.r * MEMORY_DIM), (unsigned char)(c.g * MEMORY_DIM),
            (unsigned char)(c.b * MEMORY_DIM), c.a};
}

/// The floor's and wall's patterns are drawn after, in a pass of their own,
/// so the pattern texture is not swapped in and out cell by cell.
void drawCell(int x, int y, Tile t, Tile above, Tile below, Floor floor, WallStyle wall, bool lit) {
    switch (t) {
    case Tile::Void:
        break;
    case Tile::Floor:
    case Tile::Table:
    case Tile::Statue:
    case Tile::Basin:
    case Tile::Toilet:
        DrawRectangle(x, y, CELL_PX, CELL_PX, dim(toColor(FLOOR_PALETTES[floor.palette].base), lit));
        break;
    case Tile::Wall:
        DrawRectangle(x, y, CELL_PX, CELL_PX, dim(toColor(WALL_PALETTES[wall.palette].top), lit));
        if (!opaque(below))
            DrawRectangle(x, y + CELL_PX - WALL_FACE_PX, CELL_PX, WALL_FACE_PX,
                          dim(toColor(WALL_PALETTES[wall.palette].face), lit));
        break;
    case Tile::DoorLocked:
        DrawRectangle(x, y, CELL_PX, CELL_PX, dim(DOOR_EDGE, lit));
        DrawRectangle(x + 1, y + 1, CELL_PX - 2, CELL_PX - 2, dim(DOOR, lit));
        break;
    case Tile::DoorOpen:
        DrawRectangle(x, y, CELL_PX, CELL_PX, dim(DOOR_OPEN, lit));
        break;
    case Tile::Water:
        DrawRectangle(x, y, CELL_PX, CELL_PX, dim(WATER, lit));
        if (above != Tile::Water) DrawRectangle(x, y, CELL_PX, 1, dim(WATER_EDGE, lit));
        break;
    }
}

/// One cell of a basin: water, rimmed in stone on each side that is not
/// more basin, and clouded red and brown as the water is.
void drawBasin(int x, int y, bool up, bool down, bool leftSide, bool rightSide, float tint, float foul, bool lit) {
    DrawRectangle(x, y, CELL_PX, CELL_PX, dim(WATER, lit));
    if (tint > 0.0f) drawWaterTint(x, y, tint);
    if (foul > 0.0f) drawWaterFoul(x, y, foul);
    if (up) DrawRectangle(x, y, CELL_PX, 1, dim(BASIN_RIM, lit));
    if (leftSide) DrawRectangle(x, y, 1, CELL_PX, dim(BASIN_RIM, lit));
    if (rightSide) DrawRectangle(x + CELL_PX - 1, y, 1, CELL_PX, dim(BASIN_RIM_SHADE, lit));
    if (down) DrawRectangle(x, y + CELL_PX - 1, CELL_PX, 1, dim(BASIN_RIM_SHADE, lit));
    // The water's own edge under the lit rim, as pools have.
    if (up) DrawRectangle(x + (leftSide ? 1 : 0), y + 1, CELL_PX - (leftSide ? 1 : 0) - (rightSide ? 1 : 0), 1,
                          dim(WATER_EDGE, lit));
}

void drawKey(int x, int y, bool lit) {
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++)
            if (KEY_GLYPH[gy][gx] == '#') DrawPixel(x + gx, y + gy, dim(KEY, lit));
}

void drawCake(int x, int y, bool lit) {
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++) {
            char g = CAKE_GLYPH[gy][gx];
            if (g == '.') continue;
            Color c = g == 'c' ? CAKE_CHERRY : g == 'f' ? CAKE_FROSTING : CAKE_SPONGE;
            DrawPixel(x + gx, y + gy, dim(c, lit));
        }
}

/// A cake put down, smaller than one on a table so a few heap up on a cell:
/// c the cherry, f frosting, s sponge.
constexpr int HEAP_CAKE_W = 4, HEAP_CAKE_H = 3;
constexpr const char* HEAP_CAKE_GLYPH[HEAP_CAKE_H] = {
    ".c..",
    "ffff",
    "ssss",
};
/// Where each cake put down goes, its top-left from the cell's: two on the
/// floor a pixel apart, two on those, one on top; the rest are under.
constexpr Vec2i HEAP[] = {{-2, 2}, {3, 2}, {0, -1}, {5, -1}, {2, -4}};
/// Bottles put down lie on their side in front of the cakes, end to end,
/// this many at most, the first's base here from the cell's top-left.
constexpr int HEAP_BOTTLES = 2;
constexpr int HEAP_BOTTLE_X = -3, HEAP_BOTTLE_Y = 7;

void drawHeapCake(int x, int y, bool lit) {
    for (int gy = 0; gy < HEAP_CAKE_H; gy++)
        for (int gx = 0; gx < HEAP_CAKE_W; gx++) {
            char g = HEAP_CAKE_GLYPH[gy][gx];
            if (g == '.') continue;
            Color c = g == 'c' ? CAKE_CHERRY : g == 'f' ? CAKE_FROSTING : CAKE_SPONGE;
            DrawPixel(x + gx, y + gy, dim(c, lit));
        }
}

/// What lies put down on a cell with its top-left at (x, y): the cakes
/// heaped, and the bottles on their side in front, each as full and as
/// dirty as it is.
void drawPile(int x, int y, const Pile& p, bool lit) {
    int shown = std::min(p.cakes, int(std::size(HEAP)));
    for (int i = 0; i < shown; i++) drawHeapCake(x + HEAP[i].x, y + HEAP[i].y, lit);
    int bottles = std::min(int(p.bottles.size()), HEAP_BOTTLES);
    for (int i = 0; i < bottles; i++)
        drawBottle({float(x), float(y)}, HEAP_BOTTLE_X + i * (BOTTLE_LONG + 1), HEAP_BOTTLE_Y, BottleTurn::Ahead, false,
                   p.bottles[size_t(i)], 1, dim(WHITE, lit));
}

// Spikes up and plates down show a trap that just went off, or a spent one.
void drawTrap(int x, int y, TrapKind kind, bool sprung, bool lit) {
    const char* const* glyph = kind == TrapKind::Spikes ? (sprung ? SPIKES_UP_GLYPH : SPIKES_GLYPH)
                                                        : (sprung ? PLATE_DOWN_GLYPH : PLATE_GLYPH);
    for (int gy = 0; gy < CELL_PX; gy++)
        for (int gx = 0; gx < CELL_PX; gx++) {
            char g = glyph[gy][gx];
            if (g == '.') continue;
            Color c = g == 'o' ? TRAP_HOLE : g == 't' ? SPIKE_TIP : g == 's' ? SPIKE_SHAFT : g == 'e' ? PLATE_EDGE : PLATE_DOWN;
            DrawPixel(x + gx, y + gy, dim(c, lit));
        }
}

/// The slot in the wall that darts fly from.
void drawShooter(int x, int y, bool lit) {
    DrawRectangle(x + 2, y + 2, 2, 2, dim(TRAP_HOLE, lit));
}

Pose poseOf(const Human& h) {
    // Pointing the way out, arm held out: up for a way mostly north, down
    // for one mostly south.
    if (const Human::Speech* s = h.speaking(); s && s->points && !h.dead() && !h.moving) {
        float down = std::sin(*s->points);
        return down < -POINT_TILT ? Pose::PointUp : down > POINT_TILT ? Pose::PointDown : Pose::Point;
    }
    switch (h.state) {
    case Human::State::Greeting: return Pose::Wave;
    case Human::State::Eating: return Pose::Eat;
    case Human::State::Punching: return h.punchLanded ? Pose::Punch : Pose::Guard;
    case Human::State::Giving: return h.givingWater() ? Pose::OfferBottle : Pose::Offer;
    case Human::State::Sipping: return Pose::Sip;
    case Human::State::Filling: return Pose::Fill;
    case Human::State::Pouring: return Pose::Pour;
    case Human::State::Dead: return h.diedAsleep() ? Pose::DiedAsleep : Pose::Fall;
    case Human::State::Waking: return Pose::Wake;
    case Human::State::Sitting: return Pose::Sit;
    case Human::State::Rising: return h.rousing() ? Pose::WakeUp : Pose::Rise;
    case Human::State::Sleeping: return Pose::Sleep;
    case Human::State::Washing:
    case Human::State::Drinking: return Pose::Wash;
    case Human::State::Relieving: return h.shitting() ? Pose::Squat : Pose::Piss;
    case Human::State::Looting:
    case Human::State::Stashing: return Pose::Search;
    case Human::State::Feeding: return Pose::Feed;
    case Human::State::Idle:
    case Human::State::Walking: break;
    }
    return h.moving ? Pose::Walk : Pose::Idle;
}

} // namespace

void drawKeyIcon(int x, int y) { drawKey(x, y, true); }

void drawCakeIcon(int x, int y) { drawCake(x, y, true); }

Vector2 feetOf(const Human& h) {
    float t = h.stepProgress();
    return {(h.prev.x + (h.cell.x - h.prev.x) * t) * CELL_PX + CELL_PX / 2.0f,
            (h.prev.y + (h.cell.y - h.prev.y) * t) * CELL_PX + CELL_PX};
}

// On the middle of the sprite, snapped to whole canvas pixels: zoomed out,
// a canvas pixel spans several world ones.
Vector2 cameraFor(const Human& viewer, float zoom, Vector2 size) {
    Vector2 feet = feetOf(viewer);
    float snap = std::max(1.0f, 1.0f / zoom);
    auto snapped = [snap](float v) { return std::round(v / snap) * snap; };
    return {snapped(feet.x - size.x / zoom / 2), snapped(feet.y - SPRITE_H / 2.0f - size.y / zoom / 2)};
}

bool shown(const Human& viewer, const Human& h, bool reveal) {
    return reveal || &h == &viewer || viewer.sight.sees(h.cell);
}

void drawWorld(const World& world, const Human& viewer, const SpritesFor& sprites,
               const FloorTiles& floorTiles, const StatueArt& statueArt, bool reveal, float zoom, Rectangle area) {
    Vector2 cam = cameraFor(viewer, zoom, {area.width, area.height});
    int left = int(cam.x), top = int(cam.y);
    BeginScissorMode(int(area.x), int(area.y), int(area.width), int(area.height));
    BeginMode2D(Camera2D{{area.x, area.y}, {0, 0}, 0.0f, zoom});

    auto known = [&](Vec2i c) { return reveal ? world.tile(c) : viewer.memory.tile(c); };
    int x0 = floorDiv(left, CELL_PX), x1 = floorDiv(left + int(std::ceil(area.width / zoom)), CELL_PX);
    int y0 = floorDiv(top, CELL_PX), y1 = floorDiv(top + int(std::ceil(area.height / zoom)), CELL_PX);
    // Floors never change, so a remembered one looks as it really is.
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Vec2i c{x, y};
            Tile tile = known(c);
            if (tile == Tile::Void) continue;
            drawCell(x * CELL_PX - left, y * CELL_PX - top, tile, known(c + Vec2i{0, -1}),
                     known(c + Vec2i{0, 1}), world.floor(c), world.wall(c), reveal || viewer.sight.sees(c));
        }
    const int frame = int(GetTime() * WATER_FPS);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Vec2i c{x, y};
            Tile tile = known(c);
            bool lit = reveal || viewer.sight.sees(c);
            if (tile == Tile::Water) {
                // Blood clouds the water only while the viewer watches.
                if (lit) {
                    if (float tint = world.blood().tint(c); tint > 0.0f)
                        drawWaterTint(x * CELL_PX - left, y * CELL_PX - top, tint);
                    if (float foul = world.foul(c); foul > 0.0f)
                        drawWaterFoul(x * CELL_PX - left, y * CELL_PX - top, foul);
                }
                floorTiles.drawWater(x * CELL_PX - left, y * CELL_PX - top, c, lit ? frame : 0,
                                     dim(WATER_INK, lit));
                continue;
            }
            if (tile == Tile::Toilet) {
                Floor f = world.floor(c);
                floorTiles.draw(x * CELL_PX - left, y * CELL_PX - top, c, f.pattern,
                                dim(toColor(FLOOR_PALETTES[f.palette].ink), lit));
                if (const Toilet* t = world.toilet(c))
                    drawToilet(x * CELL_PX - left, y * CELL_PX - top, c - t->front, t->dirt, t->seed, lit);
                continue;
            }
            if (tile == Tile::Basin) {
                auto rim = [&](Vec2i d) { return known(c + d) != Tile::Basin; };
                // Blood clouds the water only while the viewer watches.
                drawBasin(x * CELL_PX - left, y * CELL_PX - top, rim({0, -1}), rim({0, 1}), rim({-1, 0}), rim({1, 0}),
                          lit ? world.blood().tint(c) : 0.0f, lit ? world.foul(c) : 0.0f, lit);
                continue;
            }
            if (tile == Tile::Wall) {
                WallStyle w = world.wall(c);
                int h = opaque(known(c + Vec2i{0, 1})) ? CELL_PX : CELL_PX - WALL_FACE_PX;
                floorTiles.drawWall(x * CELL_PX - left, y * CELL_PX - top, c, w.pattern, h,
                                    dim(toColor(WALL_PALETTES[w.palette].ink), lit));
                continue;
            }
            if (tile != Tile::Floor && tile != Tile::Table && tile != Tile::Statue) continue;
            Floor f = world.floor(c);
            floorTiles.draw(x * CELL_PX - left, y * CELL_PX - top, c, f.pattern,
                            dim(toColor(FLOOR_PALETTES[f.palette].ink), lit));
        }

    // Blood in sight as it is, elsewhere as the viewer last saw it. On
    // floor and walls now; on tables and statues once they are drawn.
    auto drawBlood = [&](Vec2i c, int rise) {
        bool lit = reveal || viewer.sight.sees(c);
        const Stain* s = world.blood().at(c);
        if (!lit) {
            auto it = viewer.memory.blood.find(c);
            s = it == viewer.memory.blood.end() ? nullptr : &it->second;
        }
        if (s) drawStain(c.x * CELL_PX - left, c.y * CELL_PX - top - rise, *s, lit ? 1.0f : MEMORY_DIM);
    };
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            Tile tile = known({x, y});
            if (tile != Tile::Void && tile != Tile::Table && tile != Tile::Statue && tile != Tile::Basin) drawBlood({x, y}, 0);
        }

    // Piss and feces in sight, and those the viewer saw before, dimmed.
    // Poured water and piss first, so a pile sits on its puddle.
    const auto& drops = world.mess().all();
    auto onFloor = [&](int px, int py) {
        Tile t = known({floorDiv(px, CELL_PX), floorDiv(py, CELL_PX)});
        return t == Tile::Floor || t == Tile::DoorOpen;
    };
    for (MessKind kind : {MessKind::Water, MessKind::Piss, MessKind::Print, MessKind::Feces})
        for (size_t i = 0; i < drops.size(); i++) {
            const Dropping& d = drops[i];
            if (d.kind != kind || d.cell.x < x0 - 1 || d.cell.x > x1 + 1 || d.cell.y < y0 - 1 || d.cell.y > y1 + 1)
                continue;
            bool lit = reveal || viewer.sight.sees(d.cell);
            if (!lit && !viewer.memory.mess.count(int(i))) continue;
            drawDropping(d, Mess::freshness(d, world.time()), world.time() - d.born, left, top,
                         lit ? 1.0f : MEMORY_DIM, onFloor);
        }

    // Tables never move, so a remembered one is drawn as it is, a cell at
    // a time as far as the viewer knows it.
    auto view = [&](Vec2i c) {
        if (known(c) != Tile::Table) return CellView::Unknown;
        return reveal || viewer.sight.sees(c) ? CellView::Lit : CellView::Remembered;
    };
    std::unordered_set<const Table*> tables;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (known({x, y}) == Tile::Table)
                if (const Table* t = world.table({x, y})) tables.insert(t);
    for (const Table* t : tables) drawTable(*t, t->at.x * CELL_PX - left, t->at.y * CELL_PX - top, view);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (known({x, y}) == Tile::Table) drawBlood({x, y}, TABLE_RISE_PX);
    // Whatever lies on a table is drawn up on its top.
    auto rise = [&](Vec2i c) { return world.table(c) ? TABLE_RISE_PX : 0; };

    // Traps as the viewer knows them, or all of them when revealing; seen
    // as they are now when in sight. A dart plate's wall shows its slot.
    auto drawTrapAt = [&](Vec2i c, TrapKind kind) {
        bool lit = reveal || viewer.sight.sees(c);
        const Trap* t = world.trap(c);
        drawTrap(c.x * CELL_PX - left, c.y * CELL_PX - top - rise(c), kind, lit && t && !t->armed(), lit);
        if (kind == TrapKind::Darts && t && known(t->from) != Tile::Void)
            drawShooter(t->from.x * CELL_PX - left, t->from.y * CELL_PX - top,
                        reveal || viewer.sight.sees(t->from));
    };
    if (reveal)
        for (const auto& [c, t] : world.traps()) drawTrapAt(c, t.kind);
    else
        for (const auto& [c, kind] : viewer.memory.traps) drawTrapAt(c, kind);

    if (reveal) {
        for (Vec2i k : world.keys()) drawKey(k.x * CELL_PX - left, k.y * CELL_PX - top - rise(k), true);
        for (Vec2i k : world.cakes()) drawCake(k.x * CELL_PX - left, k.y * CELL_PX - top - rise(k), true);
        for (const auto& [c, p] : world.piles()) drawPile(c.x * CELL_PX - left, c.y * CELL_PX - top, p, true);
    } else {
        for (const auto& [c, p] : viewer.memory.piles)
            drawPile(c.x * CELL_PX - left, c.y * CELL_PX - top, p, viewer.sight.sees(c));
        for (Vec2i k : viewer.memory.keys)
            drawKey(k.x * CELL_PX - left, k.y * CELL_PX - top - rise(k), viewer.sight.sees(k));
        for (Vec2i k : viewer.memory.cakes)
            drawCake(k.x * CELL_PX - left, k.y * CELL_PX - top - rise(k), viewer.sight.sees(k));
    }

    // Statues the viewer knows any of the base of, lit if they see any of it.
    std::unordered_set<int> statues;
    for (int y = y0; y <= y1 + STATUE_REACH; y++)
        for (int x = x0; x <= x1; x++)
            if (known({x, y}) == Tile::Statue)
                if (const Statue* s = world.statue({x, y})) statues.insert(int(s - world.statues().data()));

    // The dead lie under everything standing; otherwise back to front, so
    // whatever stands lower overlaps what stands above it. A statue stands
    // on the bottom edge of its base.
    struct Drawn {
        float y;
        const Human* human;
        int statue;
    };
    std::vector<Drawn> drawn;
    for (const auto& h : world.humans())
        if (shown(viewer, *h, reveal)) drawn.push_back({feetOf(*h).y, h.get(), -1});
    for (int id : statues) {
        const Statue& s = world.statues()[size_t(id)];
        drawn.push_back({float((s.at.y + s.size.y) * CELL_PX), nullptr, id});
    }
    std::sort(drawn.begin(), drawn.end(), [](const Drawn& a, const Drawn& b) {
        bool deadA = a.human && a.human->dead(), deadB = b.human && b.human->dead();
        if (deadA != deadB) return deadA;
        return a.y < b.y;
    });
    const Vector2 viewerFeet = feetOf(viewer);
    bool viewerDrawn = false, viewerHidden = false;
    auto drawHuman = [&](const Human& h, Color tint) {
        Vector2 feet = feetOf(h);
        sprites(h).draw({feet.x - left, feet.y - top}, poseOf(h), h.animTime, h.facingLeft, 1,
                        h.hurtTime > 0.0f ? HURT_TINT : tint, h.cakes, h.shownBottle(), h.bottleInHand());
        // The cakes a body still holds lie in plain sight on it, and its
        // bottles on their side at its feet.
        bool lit = reveal || viewer.sight.sees(h.cell);
        if (h.dead() && h.cakes > 0) drawCake(h.cell.x * CELL_PX - left, h.cell.y * CELL_PX - top, lit);
        for (size_t i = 0; h.dead() && i < h.bottles.size(); i++) {
            Vector2 origin{float(h.cell.x * CELL_PX - left), float(h.cell.y * CELL_PX - top)};
            drawBottle(origin, int(i) * (BOTTLE_LONG + 1) - 2, CELL_PX - 1 - int(i), BottleTurn::Ahead, false,
                       h.bottles[i], 1, dim(WHITE, lit));
        }
    };
    for (const Drawn& d : drawn) {
        if (!d.human) {
            const Statue& s = world.statues()[size_t(d.statue)];
            bool lit = reveal;
            for (int y = 0; y < s.size.y && !lit; y++)
                for (int x = 0; x < s.size.x && !lit; x++) lit = viewer.sight.sees(s.at + Vec2i{x, y});
            statueArt.draw(s, d.statue, s.at.x * CELL_PX - left, int(d.y) - top, lit);
            for (int y = 0; y < s.size.y; y++)
                for (int x = 0; x < s.size.x; x++)
                    if (known(s.at + Vec2i{x, y}) == Tile::Statue) drawBlood(s.at + Vec2i{x, y}, 0);
            float sx = float(s.at.x * CELL_PX);
            viewerHidden |= viewerDrawn && viewerFeet.x + SPRITE_W / 2.0f > sx &&
                            viewerFeet.x - SPRITE_W / 2.0f < sx + s.size.x * CELL_PX &&
                            viewerFeet.y > d.y - statueHeightPx(s);
            continue;
        }
        drawHuman(*d.human, WHITE);
        viewerDrawn |= d.human == &viewer;
    }
    if (viewerHidden) drawHuman(viewer, GHOST_TINT);

    // Darts over everyone, as a short streak trailing back toward their wall.
    for (const Dart& d : world.darts()) {
        if (d.flown < 1.0f || (!reveal && !viewer.sight.sees(d.at()))) continue;
        float px = (d.from.x + d.dir.x * d.flown + 0.5f) * CELL_PX - left;
        float py = (d.from.y + d.dir.y * d.flown + 0.5f) * CELL_PX - top - DART_RISE_PX;
        for (int i = 0; i < 3; i++) DrawPixel(int(px) - d.dir.x * i, int(py) - d.dir.y * i, DART);
    }
    EndMode2D();
    EndScissorMode();
}
