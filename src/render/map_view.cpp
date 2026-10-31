#include "render/map_view.h"
#include "human/human.h"
#include "render/sprites.h"
#include "ui/pixel_ui.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace {

constexpr Color BACKDROP = {0, 0, 0, 255};
/// Remembered cells, flat, by tile.
constexpr Color FLOOR = {66, 66, 66, 255};
constexpr Color WALL = {150, 150, 150, 255};
constexpr Color DOOR_LOCKED = {176, 96, 40, 255};
constexpr Color DOOR_OPEN = {104, 74, 46, 255};
constexpr Color WATER = {50, 90, 140, 255};
constexpr Color BASIN = {96, 146, 196, 255};
constexpr Color TABLE = {96, 70, 48, 255};
constexpr Color STATUE = {116, 116, 116, 255};
/// Cells tinted for what lay on them.
constexpr Color BLOOD = {150, 30, 34, 255};
constexpr Color MESS = {118, 84, 40, 255};
/// The symbols, each on a dark outline so it reads on any tile.
constexpr Color OUTLINE = {0, 0, 0, 255};
/// What it only has on someone's word: places dimmed to this share of
/// their colour, with question marks over them and beside told symbols.
constexpr float HEARD_SHADE = 0.45f;
/// Told symbols are dimmed to this share, and their ? sits up and right.
constexpr float TOLD_SHADE = 0.6f;
constexpr int TOLD_MARK_DX = 4, TOLD_MARK_DY = -4;
/// Over places heard of, one ? to each block about this many canvas
/// pixels across, on the heard cell nearest its middle.
constexpr int HEARD_SPACING_PX = 24, HEARD_SPACING_MIN = 4;

/// A symbol is GLYPH x GLYPH pixels, centred on its cell, drawn from
/// data/sprites.txt in its ink #: every letter but '.' is set.
constexpr int GLYPH = 5;
using Glyph = Sprite;

const Glyph& symbol(std::string_view name) { return sprites::get(name, GLYPH, GLYPH); }

/// The symbol's colour, its ink #.
Color inkOf(const Glyph& g) {
    Rgba c = g.ink('#');
    return {c.r, c.g, c.b, c.a};
}

/// The legend: a symbol or swatch, a gap, its name; items ITEM_GAP apart,
/// on as many rows as they need.
constexpr int LEGEND_PAD = 3, LEGEND_ICON_GAP = 2, ITEM_GAP = 7;
/// The YOU symbol's centre blinks this often.
constexpr float BLINK = 0.4f;

Color tileColor(Tile t) {
    switch (t) {
    case Tile::Floor:
    case Tile::Toilet: return FLOOR;
    case Tile::Wall: return WALL;
    case Tile::DoorLocked: return DOOR_LOCKED;
    case Tile::DoorOpen: return DOOR_OPEN;
    case Tile::Water: return WATER;
    case Tile::Basin: return BASIN;
    case Tile::Table: return TABLE;
    case Tile::Statue:
    case Tile::Vending:
    case Tile::Rack:
    case Tile::Console:
    case Tile::Dispenser: return STATUE;
    case Tile::Void: break;
    }
    return BLANK;
}

Color shade(Color c, float share) {
    return {uint8_t(c.r * share), uint8_t(c.g * share), uint8_t(c.b * share), c.a};
}

void drawGlyph(const Glyph& g, int x, int y, Color c) {
    for (int pass = 0; pass < 2; pass++)
        for (int r = 0; r < GLYPH; r++)
            for (int col = 0; col < GLYPH; col++) {
                if (g.letter(col, r) == '.') continue;
                if (pass == 0)
                    DrawRectangle(x + col - 1, y + r - 1, 3, 3, OUTLINE);
                else
                    DrawRectangle(x + col, y + r, 1, 1, c);
            }
}

/// Lays out where cells fall on the canvas, and draws what fits wholly
/// inside the map.
struct Frame {
    Rectangle map;
    int cellPx;
    Vector2 centre;

    /// A cell's top-left corner on the canvas.
    Vector2 at(Vec2i c) const {
        return {std::floor(map.x + map.width / 2 + (c.x - centre.x) * cellPx),
                std::floor(map.y + map.height / 2 + (c.y - centre.y) * cellPx)};
    }
    /// A symbol centred on a cell, moved by dx, dy, if it fits.
    void glyph(Vec2i c, const Glyph& g, Color col, int dx = 0, int dy = 0) const {
        Vector2 p = at(c);
        int x = int(p.x) + cellPx / 2 - GLYPH / 2 + dx, y = int(p.y) + cellPx / 2 - GLYPH / 2 + dy;
        if (x - 1 < map.x || y - 1 < map.y || x + GLYPH + 1 > map.x + map.width || y + GLYPH + 1 > map.y + map.height)
            return;
        drawGlyph(g, x, y, col);
    }
    /// A symbol, dimmed with a ? by it if it is only on someone's word.
    void item(Vec2i c, const Glyph& g, Color col, bool told) const {
        glyph(c, g, told ? shade(col, TOLD_SHADE) : col);
        if (told) {
            const Glyph& q = symbol("map heard of");
            glyph(c, q, inkOf(q), TOLD_MARK_DX, TOLD_MARK_DY);
        }
    }
    /// A cell tinted, at least 2 pixels so it shows zoomed out.
    void tint(Vec2i c, Color col) const {
        Vector2 p = at(c);
        int s = std::max(cellPx, 2);
        if (p.x < map.x || p.y < map.y || p.x + s > map.x + map.width || p.y + s > map.y + map.height) return;
        DrawRectangle(int(p.x), int(p.y), s, s, col);
    }
};

struct LegendItem {
    const char* label;
    /// The symbol's sprite, or none for a swatch of color.
    const char* glyph;
    Color color;
};

constexpr LegendItem LEGEND[] = {
    {"YOU", "map you", {}},       {"KEY", "map key", {}},         {"CAKE", "map cake", {}},
    {"TRAP", "map trap", {}},     {"BODY", "map body", {}},       {"TOILET", "map toilet", {}},
    {"MACHINE", "map machine", {}}, {"COOLER", "map dispenser", {}},
    {"BLOOD", nullptr, BLOOD},    {"MESS", nullptr, MESS},        {"LOCKED", nullptr, DOOR_LOCKED},
    {"WATER", nullptr, WATER},    {"HEARD OF", "map heard of", {}},
};

// Items flow left to right and wrap onto the next row. Laid out without
// drawing, it gives how many rows they take.
int layLegend(Rectangle area, bool draw) {
    int left = int(area.x) + LEGEND_PAD, right = int(area.x + area.width) - LEGEND_PAD;
    int x = left, y = draw ? int(area.y + area.height) - mapLegendHeight(area.width) + LEGEND_PAD : 0, rows = 1;
    if (draw) DrawRectangle(int(area.x), y - LEGEND_PAD, int(area.width), 1, ui::GROUND);
    for (const LegendItem& item : LEGEND) {
        int w = GLYPH + LEGEND_ICON_GAP + ui::textWidth(item.label);
        if (x + w > right && x > left) x = left, y += ui::ROW, rows++;
        if (draw) {
            if (item.glyph) {
                const Glyph& g = symbol(item.glyph);
                drawGlyph(g, x, y + 2, inkOf(g));
            } else
                DrawRectangle(x, y + 2, GLYPH, GLYPH, item.color);
            ui::text(item.label, x + GLYPH + LEGEND_ICON_GAP, y, ui::DIM);
        }
        x += w + ITEM_GAP;
    }
    return rows;
}

} // namespace

int mapLegendHeight(float width) { return layLegend({0, 0, width, 0}, false) * ui::ROW + 2 * LEGEND_PAD; }

// Tiles go down as runs of one colour along each row, so a big map stays
// a few thousand rectangles; then tints, then symbols on top, the viewer last.
void drawMap(const World& world, const Human& viewer, Rectangle area, int cellPx, Vector2 centre, float time) {
    DrawRectangleRec(area, BACKDROP);
    Frame f{{area.x, area.y, area.width, area.height - float(mapLegendHeight(area.width))}, cellPx, centre};
    const Memory& memory = viewer.memory;
    int cols = int(f.map.width) / cellPx + 2, rows = int(f.map.height) / cellPx + 2;
    int x0 = int(std::floor(centre.x)) - cols / 2 - 1, y0 = int(std::floor(centre.y)) - rows / 2 - 1;
    int clipL = int(f.map.x), clipR = int(f.map.x + f.map.width);
    int clipT = int(f.map.y), clipB = int(f.map.y + f.map.height);
    std::vector<Vec2i> toilets;
    for (int cy = y0; cy <= y0 + rows + 1; cy++) {
        int top = int(f.at({0, cy}).y), bottom = top + cellPx;
        top = std::max(top, clipT), bottom = std::min(bottom, clipB);
        if (top >= bottom) continue;
        int runStart = 0;
        Color run = BLANK;
        auto flush = [&](int end) {
            if (run.a == 0) return;
            int l = std::max(runStart, clipL), r = std::min(end, clipR);
            if (l < r) DrawRectangle(l, top, r - l, bottom - top, run);
        };
        for (int cx = x0; cx <= x0 + cols + 1; cx++) {
            Tile t = memory.tile({cx, cy});
            if (t == Tile::Toilet) toilets.push_back({cx, cy});
            Color c = tileColor(t);
            if (c.a && !memory.hearsay.empty() && memory.hearsay.count({cx, cy})) c = shade(c, HEARD_SHADE);
            int px = int(f.at({cx, cy}).x);
            if (c.r != run.r || c.g != run.g || c.b != run.b || c.a != run.a) {
                flush(px);
                run = c;
                runStart = px;
            }
        }
        flush(int(f.at({x0 + cols + 2, cy}).x));
    }
    for (int i : memory.mess) f.tint(world.mess().all()[size_t(i)].cell, MESS);
    for (Vec2i c : memory.pools) f.tint(c, BLOOD);
    // Places heard of: one ? per block, on the heard floor nearest the
    // block's middle, so they sit still while the map moves; then any too
    // near one kept before it, block by block, go.
    int block = std::max(HEARD_SPACING_MIN, HEARD_SPACING_PX / cellPx);
    std::unordered_map<Vec2i, Vec2i, Vec2iHash> best;
    auto offMiddle = [&](Vec2i c) {
        Vec2i d = c - Vec2i{floorDiv(c.x, block), floorDiv(c.y, block)} * block - Vec2i{block / 2, block / 2};
        return d.x * d.x + d.y * d.y;
    };
    auto before = [](Vec2i a, Vec2i b) { return a.y < b.y || (a.y == b.y && a.x < b.x); };
    for (Vec2i c : memory.hearsay) {
        if (c.x < x0 || c.x > x0 + cols + 1 || c.y < y0 || c.y > y0 + rows + 1) continue;
        if (memory.tile(c) != Tile::Floor) continue;
        auto [it, fresh] = best.emplace(Vec2i{floorDiv(c.x, block), floorDiv(c.y, block)}, c);
        int was = offMiddle(it->second), now = offMiddle(c);
        if (!fresh && (now < was || (now == was && before(c, it->second)))) it->second = c;
    }
    std::vector<std::pair<Vec2i, Vec2i>> marks(best.begin(), best.end());
    std::sort(marks.begin(), marks.end(), [&](const auto& a, const auto& b) { return before(a.first, b.first); });
    std::vector<Vec2i> kept;
    const Glyph& question = symbol("map heard of");
    for (const auto& [key, c] : marks) {
        bool crowded = false;
        for (Vec2i k : kept) crowded |= cheb(c, k) < block * 3 / 4;
        if (crowded) continue;
        kept.push_back(c);
        f.glyph(c, question, inkOf(question));
    }
    auto told = [&](Vec2i c) { return memory.toldBy.count(c) > 0; };
    const Glyph &toilet = symbol("map toilet"), &trap = symbol("map trap"), &body = symbol("map body");
    const Glyph &key = symbol("map key"), &cake = symbol("map cake"), &you = symbol("map you");
    const Glyph &machine = symbol("map machine"), &cooler = symbol("map dispenser");
    // Brown once seen pissed or shat in.
    const Glyph& fouled = symbol("map toilet fouled");
    for (Vec2i c : toilets) {
        const Glyph& g = viewer.seenToiletDirt(c) >= Toilet::FOULED ? fouled : toilet;
        f.glyph(c, g, inkOf(g));
    }
    for (const auto& [c, kind] : memory.traps) f.item(c, trap, inkOf(trap), told(c));
    for (Vec2i c : viewer.bodyCells()) f.glyph(c, body, inkOf(body));
    for (Vec2i c : memory.keys) f.item(c, key, inkOf(key), told(c));
    for (Vec2i c : memory.cakes) f.item(c, cake, inkOf(cake), told(c));
    for (const auto& [c, seen] : memory.machines) f.item(c, machine, inkOf(machine), told(c));
    for (const auto& [c, water] : memory.dispensers) f.item(c, cooler, inkOf(cooler), told(c));
    f.glyph(viewer.cell, you, inkOf(you));
    if (std::fmod(time, 2 * BLINK) < BLINK) {
        Vector2 p = f.at(viewer.cell);
        int x = int(p.x) + cellPx / 2 - 1, y = int(p.y) + cellPx / 2 - 1;
        if (x >= clipL && y >= clipT && x + 3 <= clipR && y + 3 <= clipB) DrawRectangle(x, y, 3, 3, inkOf(you));
    }
    layLegend(area, true);
}
