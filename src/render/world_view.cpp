#include "render/world_view.h"
#include "human/human.h"
#include "render/blood_art.h"
#include "render/cable_art.h"
#include "render/computer_art.h"
#include "render/dispenser_art.h"
#include "render/bottle_art.h"
#include "render/floor_tiles.h"
#include "render/human_sprites.h"
#include "render/item_art.h"
#include "render/mess_art.h"
#include "render/server_rack_art.h"
#include "render/statue_art.h"
#include "render/table_art.h"
#include "render/toilet_art.h"
#include "render/vending_art.h"
#include "render/water_art.h"
#include "world/world.h"
#include <rlgl.h>
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
constexpr Color DART = {226, 214, 172, 255};
/// The slot in the wall darts fly from, as dark as a spike trap's holes.
constexpr Color TRAP_HOLE = {0, 0, 0, 120};
/// Darts fly about chest high, this far above the middle of their row.
constexpr int DART_RISE_PX = 3;
/// Flies buzz over whatever smells, more the stronger it smells: one at the
/// faintest, FLIES_MAX at the strongest. Each loops about its smell this
/// many pixels either side, across and up and down, at its own speed.
constexpr Color FLY = {20, 18, 16, 255};
constexpr int FLIES_MAX = 5;
constexpr float FLY_RX = 4.0f, FLY_RY = 2.5f, FLY_SPEED_X = 5.0f, FLY_SPEED_Y = 7.3f;
/// They hover this many pixels over a standing human's head, a body lying
/// down, or the middle of a cell.
constexpr float FLY_OVER_HEAD = 2.0f, FLY_OVER_BODY = 4.0f, FLY_OVER_CELL = 3.0f;
/// A human just hit is drawn tinted with this.
constexpr Color HURT_TINT = {255, 90, 90, 255};
/// The viewer behind a statue shows through it, this faintly.
constexpr Color GHOST_TINT = {255, 255, 255, 110};
/// Remembered but out of sight: drawn at this brightness.
constexpr float MEMORY_DIM = 0.45f;
/// The edge of sight fades into memory over its last two rings of cells:
/// in each 2x2 block of pixels, the inner ring darkens one to how
/// remembered ones look, the outer ring three, in this order.
constexpr int SIGHT_FADE[2][2] = {{0, 2}, {3, 1}};
constexpr int SIGHT_FADE_INNER = 1, SIGHT_FADE_OUTER = 3;
/// Statues are looked for this many rows below the screen, as the tallest
/// rise into it from there.
constexpr int STATUE_REACH = 28;

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
    case Tile::Vending:
    case Tile::Rack:
    case Tile::Console:
    case Tile::Dispenser:
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

/// Where each cake put down goes, its top-left from the cell's: two on the
/// floor a pixel apart, two on those, one on top; the rest are under.
constexpr Vec2i HEAP[] = {{-2, 2}, {3, 2}, {0, -1}, {5, -1}, {2, -4}};
/// Bottles put down lie on their side in front of the cakes, end to end,
/// this many at most, the first's base here from the cell's top-left.
constexpr int HEAP_BOTTLES = 2;
constexpr int HEAP_BOTTLE_X = -3, HEAP_BOTTLE_Y = 7;

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
    // An arm out to the tap, standing at a dispenser.
    case Human::State::Filling: return h.atTap() ? Pose::Point : Pose::Fill;
    case Human::State::Pouring: return Pose::Pour;
    case Human::State::Dead: return h.diedAsleep() ? Pose::DiedAsleep : Pose::Fall;
    case Human::State::Waking: return Pose::Wake;
    case Human::State::Sitting: return h.lyingDown() ? Pose::Lie : Pose::Sit;
    case Human::State::Rising: return h.rousing() ? Pose::WakeUp : Pose::Rise;
    case Human::State::Sleeping: return Pose::Sleep;
    case Human::State::Drinking: return h.atTap() ? Pose::Point : Pose::Wash;
    case Human::State::Washing: return Pose::Wash;
    case Human::State::Relieving: return h.shitting() ? Pose::Squat : Pose::Piss;
    case Human::State::Looting:
    case Human::State::Stashing: return Pose::Search;
    case Human::State::Feeding: return Pose::Feed;
    // An arm out to the buttons.
    case Human::State::Vending: return Pose::Point;
    // Hands at the keys.
    case Human::State::Typing: return Pose::Type;
    case Human::State::Idle:
    case Human::State::Walking: break;
    }
    if (h.moving) return h.running() ? Pose::Run : Pose::Walk;
    return h.winded() ? Pose::Winded : Pose::Idle;
}

} // namespace

void drawKeyIcon(int x, int y) { drawKey(x, y, true); }

void drawCakeIcon(int x, int y) { drawCake(x, y, true); }

Vector2 feetOf(const Human& h) {
    float t = h.stepProgress();
    return {(h.prev.x + (h.cell.x - h.prev.x) * t) * CELL_PX + CELL_PX / 2.0f,
            (h.prev.y + (h.cell.y - h.prev.y) * t) * CELL_PX + CELL_PX};
}

Vector2 drawnFeet(const Human& h, float zoom) {
    Vector2 feet = feetOf(h);
    return {std::round(feet.x * zoom) / zoom, std::round(feet.y * zoom) / zoom};
}

// On the middle of the sprite, snapped to whole canvas pixels: zoomed out,
// a canvas pixel spans several world ones; zoomed in, a world pixel spans
// several canvas ones, and the camera moves by the canvas pixel.
Vector2 cameraFor(Vector2 focus, float zoom, Vector2 size) {
    auto snapped = [zoom](float v) { return std::round(v * zoom) / zoom; };
    return {snapped(focus.x - size.x / zoom / 2), snapped(focus.y - SPRITE_H / 2.0f - size.y / zoom / 2)};
}

bool shown(const Human& viewer, const Human& h, bool reveal) {
    return reveal || &h == &viewer || viewer.sight.sees(h.cell);
}

void drawWorld(const World& world, const Human& viewer, const SpritesFor& sprites,
               const FloorTiles& floorTiles, const StatueArt& statueArt, bool reveal, float zoom, Rectangle area,
               Vector2 focus, bool humans) {
    // Everything is placed in whole world pixels from left and top; the
    // camera's part of a world pixel past them shifts it all at once.
    Vector2 cam = cameraFor(focus, zoom, {area.width, area.height});
    int left = int(std::floor(cam.x)), top = int(std::floor(cam.y));
    BeginScissorMode(int(area.x), int(area.y), int(area.width), int(area.height));
    BeginMode2D(Camera2D{{area.x, area.y}, {cam.x - float(left), cam.y - float(top)}, 0.0f, zoom});

    auto known = [&](Vec2i c) { return reveal ? world.tile(c) : viewer.memory.tile(c); };
    int x0 = floorDiv(left, CELL_PX), x1 = floorDiv(left + 1 + int(std::ceil(area.width / zoom)), CELL_PX);
    int y0 = floorDiv(top, CELL_PX), y1 = floorDiv(top + 1 + int(std::ceil(area.height / zoom)), CELL_PX);
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
                // Out of sight, as dirty as the viewer last saw it.
                if (const Toilet* t = world.toilet(c)) {
                    Human::SeenToilet seen{t->dirt, t->piss};
                    if (!lit) seen = viewer.seenToilet(c) ? *viewer.seenToilet(c) : Human::SeenToilet{};
                    drawToilet(x * CELL_PX - left, y * CELL_PX - top, c - t->front, seen.dirt, seen.piss, t->seed, lit);
                }
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
            if (tile != Tile::Floor && tile != Tile::Table && tile != Tile::Statue && tile != Tile::Vending &&
                tile != Tile::Rack && tile != Tile::Console && tile != Tile::Dispenser)
                continue;
            Floor f = world.floor(c);
            floorTiles.draw(x * CELL_PX - left, y * CELL_PX - top, c, f.pattern,
                            dim(toColor(FLOOR_PALETTES[f.palette].ink), lit));
        }

    // Cables on the floor never move, so one is drawn wherever its cell is known.
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (uint8_t k = world.cable({x, y}); k && known({x, y}) != Tile::Void)
                drawCable(x * CELL_PX - left, y * CELL_PX - top, uint8_t(k & 15), k >> 4,
                          reveal || viewer.sight.sees({x, y}));

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
            if (tile != Tile::Void && tile != Tile::Table && tile != Tile::Statue && tile != Tile::Basin &&
                tile != Tile::Vending && tile != Tile::Rack && tile != Tile::Console && tile != Tile::Dispenser)
                drawBlood({x, y}, 0);
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
        const Machine* machine = nullptr;
        const ServerPiece* piece = nullptr;
        const Dispenser* dispenser = nullptr;
    };
    std::vector<Drawn> drawn;
    for (const auto& h : world.humans())
        if (humans && shown(viewer, *h, reveal)) drawn.push_back({feetOf(*h).y, h.get(), -1});
    for (int id : statues) {
        const Statue& s = world.statues()[size_t(id)];
        drawn.push_back({float((s.at.y + s.size.y) * CELL_PX), nullptr, id});
    }
    // Machines the viewer knows either cell of, standing on the bottom edge
    // of their lower cell.
    for (const Machine& m : world.machines()) {
        Vec2i last = m.at + m.along();
        if (last.x < x0 - 1 || m.at.x > x1 + 1 || last.y < y0 || m.at.y > y1 + VENDING_H / CELL_PX + 1) continue;
        if (known(m.at) != Tile::Vending && known(last) != Tile::Vending) continue;
        drawn.push_back({float((last.y + 1) * CELL_PX), nullptr, -1, &m});
    }
    // Server room machines the viewer knows any cell of, standing on the
    // bottom edge of their foot row.
    auto pieceCells = [](const ServerPiece& p) {
        std::vector<Vec2i> cells;
        for (int dy = 0; dy < DESK_D; dy++)
            for (int dx = 0; dx < DESK_W; dx++)
                if (ServerPiece::covers(p.kind, {dx, dy})) cells.push_back(p.at + Vec2i{dx, dy});
        return cells;
    };
    for (const ServerPiece& p : world.pieces()) {
        if (p.at.x < x0 - DESK_W || p.at.x > x1 + 1 || p.at.y < y0 - DESK_D ||
            p.footRow() > y1 + SERVER_RACK_H / CELL_PX + 1)
            continue;
        bool knownAny = false;
        for (Vec2i c : pieceCells(p)) knownAny |= known(c) == Tile::Rack || known(c) == Tile::Console;
        if (knownAny) drawn.push_back({float((p.footRow() + 1) * CELL_PX), nullptr, -1, nullptr, &p});
    }
    // Water dispensers the viewer knows of, standing on the bottom edge of their cell.
    for (const Dispenser& d : world.dispensers()) {
        if (d.at.x < x0 - 1 || d.at.x > x1 + 1 || d.at.y < y0 || d.at.y > y1 + DISPENSER_H / CELL_PX + 1) continue;
        if (known(d.at) == Tile::Dispenser)
            drawn.push_back({float((d.at.y + 1) * CELL_PX), nullptr, -1, nullptr, nullptr, &d});
    }
    std::sort(drawn.begin(), drawn.end(), [](const Drawn& a, const Drawn& b) {
        bool deadA = a.human && a.human->dead(), deadB = b.human && b.human->dead();
        if (deadA != deadB) return deadA;
        // On the same row a machine goes behind: whoever sits in the desk's chair is drawn over it.
        if (a.y == b.y) return (a.piece != nullptr) > (b.piece != nullptr);
        return a.y < b.y;
    });
    const Vector2 viewerFeet = feetOf(viewer);
    bool viewerDrawn = false, viewerHidden = false;
    auto drawHuman = [&](const Human& h, Color tint) {
        // Its art is laid out in whole world pixels and then shifted by the
        // part of one its feet stand past, so it glides by the canvas pixel.
        Vector2 feet = drawnFeet(h, zoom), whole{std::round(feet.x), std::round(feet.y)};
        rlPushMatrix();
        rlTranslatef(feet.x - whole.x, feet.y - whole.y, 0.0f);
        sprites(h).draw({whole.x - left, whole.y - top}, poseOf(h), h.animTime, h.facingLeft, 1,
                        h.hurtTime > 0.0f ? HURT_TINT : tint, h.cakes, h.shownBottle(), h.bottleInHand());
        rlPopMatrix();
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
        if (d.piece) {
            // Each blinks to its own clock; remembered, it holds still.
            const ServerPiece& p = *d.piece;
            bool lit = reveal;
            for (Vec2i c : pieceCells(p)) lit |= viewer.sight.sees(c);
            const float seconds = float(GetTime()) + float(p.seed % 1000) / 100.0f;
            const int x = p.at.x * CELL_PX - left;
            // Someone at its keys, in sight: the screen asks them for a password.
            bool typed = false;
            for (const auto& o : world.humans())
                typed |= o->typingAt() == p.at && (reveal || viewer.sight.sees(o->cell));
            if (p.kind == Rig::Rack) drawServerRack(x, int(d.y) - top, lit, seconds);
            else if (p.kind == Rig::Terminal) drawTerminal(x, int(d.y) - top, lit, seconds, typed);
            else drawConsoleDesk(x, p.at.y * CELL_PX - top, lit, seconds, typed);
            continue;
        }
        if (d.dispenser) {
            // In sight it shows how full it is now; remembered, as full as the viewer last saw it, dimmed.
            const Dispenser& w = *d.dispenser;
            bool lit = reveal || viewer.sight.sees(w.at);
            auto it = viewer.memory.dispensers.find(w.at);
            float water = lit ? w.water : it == viewer.memory.dispensers.end() ? 0.0f : it->second;
            drawDispenser(w.at.x * CELL_PX - left, int(d.y) - top, water / DISPENSER_HOLDS, w.side(), w.facing.x < 0,
                          lit);
            continue;
        }
        if (d.machine) {
            // In sight it shows what it holds now; remembered, what the viewer saw in it, dimmed and still.
            const Machine& m = *d.machine;
            bool lit = reveal || viewer.sight.sees(m.at) || viewer.sight.sees(m.at + m.along());
            int x = m.at.x * CELL_PX - left, y = int(d.y) - top;
            if (!m.side()) {
                auto it = viewer.memory.machines.find(m.at);
                int stock = lit ? m.stock : it == viewer.memory.machines.end() ? 0 : it->second.stock;
                drawVendingMachine(m.kind, x, y, uint8_t((1u << stock) - 1), lit, float(GetTime()));
            } else {
                // Its back to the wall, its flank to the viewer.
                int sx = m.facing.x < 0 ? x + CELL_PX - VENDING_DEPTH : x;
                drawVendingMachineSide(m.kind, sx, y, lit, float(GetTime()), m.facing.x > 0);
            }
            continue;
        }
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

    // The edge of sight fades into memory, so the lit circle has no hard
    // rim. The pattern is fixed to the world, so it doesn't crawl as the
    // camera moves.
    if (!reveal) {
        const Color shade{0, 0, 0, (unsigned char)std::lround(255.0f * (1.0f - MEMORY_DIM))};
        const Vec2i eye = viewer.sight.eye();
        const float r = float(viewer.sight.radius());
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                float d = std::hypot(float(x - eye.x), float(y - eye.y));
                if (d <= r - 2.0f || !viewer.sight.sees({x, y})) continue;
                int level = d > r - 1.0f ? SIGHT_FADE_OUTER : SIGHT_FADE_INNER;
                for (int py = y * CELL_PX; py < (y + 1) * CELL_PX; py++)
                    for (int px = x * CELL_PX; px < (x + 1) * CELL_PX; px++)
                        if (SIGHT_FADE[py & 1][px & 1] < level) DrawPixel(px - left, py - top, shade);
            }
    }

    // Darts over everyone, as a short streak trailing back toward their wall.
    for (const Dart& d : world.darts()) {
        if (d.flown < 1.0f || (!reveal && !viewer.sight.sees(d.at()))) continue;
        float px = (d.from.x + d.dir.x * d.flown + 0.5f) * CELL_PX - left;
        float py = (d.from.y + d.dir.y * d.flown + 0.5f) * CELL_PX - top - DART_RISE_PX;
        for (int i = 0; i < 3; i++) DrawPixel(int(px) - d.dir.x * i, int(py) - d.dir.y * i, DART);
    }

    // Flies over every smell in sight; a human's go where it goes.
    float now = world.time();
    for (const Smell& s : world.smells()) {
        Vector2 at{(s.cell.x + 0.5f) * CELL_PX, (s.cell.y + 0.5f) * CELL_PX - FLY_OVER_CELL};
        if (s.kind == SmellKind::Human || s.kind == SmellKind::Body) {
            const Human& h = world.human(s.who);
            if (!shown(viewer, h, reveal)) continue;
            at = feetOf(h);
            at.y -= h.dead() ? SPRITE_W / 2.0f + FLY_OVER_BODY : SPRITE_H + FLY_OVER_HEAD;
        } else if (!reveal && !viewer.sight.sees(s.cell)) {
            continue;
        }
        int flies = 1 + int(std::lround(s.strength * (FLIES_MAX - 1)));
        // Each swarm its own, by where it is.
        unsigned hash = (unsigned(s.cell.x) * 73856093u) ^ (unsigned(s.cell.y) * 19349663u);
        float seed = float(hash & 0xffffu) / 100.0f + float(s.who);
        for (int i = 0; i < flies; i++) {
            float phase = seed + 2.4f * i;
            float x = at.x + std::sin(now * FLY_SPEED_X * (1.0f + 0.37f * i) + phase) * FLY_RX;
            float y = at.y + std::sin(now * FLY_SPEED_Y * (1.0f + 0.23f * i) + 1.7f * phase) * FLY_RY;
            DrawPixel(int(std::lround(x)) - left, int(std::lround(y)) - top, FLY);
        }
    }
    EndMode2D();
    EndScissorMode();
}
