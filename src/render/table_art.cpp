#include "render/table_art.h"
#include "core/color.h"
#include "world/chunk.h"
#include <algorithm>
#include <raylib.h>
#include <vector>

namespace {

/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;
/// Showing between the legs, under the table.
constexpr Color SHADOW = {0, 0, 0, 80};
/// The top sticks out past its cells this far each side, so a rim shows
/// round a cake that fills the cell it lies on.
constexpr int OVERHANG_PX = 1;

/// A material's colours: its top, the grain, veins or flecks on it, the
/// top's lit and shaded rims, the front face, and the legs. Glass is the
/// one see-through.
struct Look {
    Rgba top, grain, hi, edge, side, leg;
};

constexpr Rgba rgba(uint32_t rgb, uint8_t a) { return {uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb), a}; }

constexpr Look LOOKS[] = {
    {hex(0xb0844e), hex(0x9a7040), hex(0xc89a62), hex(0x6e4c2a), hex(0x7c5630), hex(0x5e4024)}, // oak
    {hex(0x6e4630), hex(0x5c3a28), hex(0x86583c), hex(0x3e2618), hex(0x4a2e1e), hex(0x38221a)}, // walnut
    {hex(0xd2b07a), hex(0xbc9864), hex(0xe2c48e), hex(0x8e7046), hex(0xa08050), hex(0x7e6440)}, // pine
    {hex(0x9aa2aa), hex(0x8a9298), hex(0xc4ccd2), hex(0x5c6268), hex(0x6c7278), hex(0x50555a)}, // steel
    {hex(0xe2ddd4), hex(0xa8a29a), hex(0xf4f0ea), hex(0x8e8a84), hex(0xb4afa6), hex(0x9a958c)}, // marble
    {hex(0x6a6a70), hex(0x4c4c52), hex(0x86868e), hex(0x3c3c42), hex(0x4a4a50), hex(0x3a3a40)}, // granite
    {rgba(0xaad6e2, 90), rgba(0xffffff, 150), rgba(0xdcf4fa, 170), rgba(0x78aaba, 190), rgba(0x6e96a2, 150),
     hex(0x6c7278)}, // glass, on steel legs
    {}, // plastic, coloured per table
};
static_assert(sizeof LOOKS / sizeof LOOKS[0] == size_t(TableMaterial::Count));

/// Hues plastic tables come in.
constexpr float PLASTIC_HUES[] = {0.0f, 0.07f, 0.14f, 0.33f, 0.5f, 0.6f, 0.8f};

uint32_t mix(uint32_t a) {
    a ^= a >> 16;
    a *= 0x7feb352du;
    a ^= a >> 15;
    a *= 0x846ca68bu;
    return a ^ (a >> 16);
}

uint32_t noise(uint32_t seed, int x, int y) { return mix(seed ^ mix(uint32_t(x) * 0x9e3779b1u ^ uint32_t(y) * 0x85ebca77u)); }

Look lookOf(const Table& t) {
    if (t.material != TableMaterial::Plastic) return LOOKS[size_t(t.material)];
    float h = PLASTIC_HUES[t.seed % (sizeof PLASTIC_HUES / sizeof PLASTIC_HUES[0])];
    return {hsv(h, 0.6f, 0.82f), hsv(h, 0.6f, 0.82f), hsv(h, 0.35f, 0.96f), hsv(h, 0.65f, 0.5f), hsv(h, 0.65f, 0.6f),
            hsv(h, 0.6f, 0.45f)};
}

Color toColor(Rgba c, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    return {(unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), c.a};
}

bool inEllipse(float x, float y, float w, float h) {
    float dx = (x + 0.5f - w / 2) / (w / 2), dy = (y + 0.5f - h / 2) / (h / 2);
    return dx * dx + dy * dy <= 1.0f;
}

/// The table seen straight from above, a pixel at a time: its whole
/// footprint, widened by the overhang, and rounded where the shape is.
struct Body {
    int w, h;
    std::vector<bool> on;

    bool at(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h && on[size_t(y * w + x)]; }
};

Body bodyOf(const Table& t) {
    Body b{t.size.x * CELL_PX + 2 * OVERHANG_PX, t.size.y * CELL_PX, {}};
    b.on.assign(size_t(b.w * b.h), false);
    // A pixel of the footprint itself, from its top-left corner.
    auto under = [&](int x, int y) { return x >= 0 && x < t.size.x * CELL_PX && t.coversLocal({x / CELL_PX, y / CELL_PX}); };
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++) {
            bool in = false;
            switch (t.shape) {
            case TableShape::Pedestal:
            case TableShape::Round:
                in = inEllipse(float(x), float(y), float(b.w), float(b.h));
                break;
            case TableShape::Oval: {
                // A stadium: within a radius of the line down its middle.
                bool wide = b.w >= b.h;
                float r = std::min(b.w, b.h) / 2.0f, cx = x + 0.5f, cy = y + 0.5f;
                float dx = cx - (wide ? std::clamp(cx, r, b.w - r) : b.w / 2.0f);
                float dy = cy - (wide ? b.h / 2.0f : std::clamp(cy, r, b.h - r));
                in = dx * dx + dy * dy <= r * r;
                break;
            }
            default:
                for (int d = -OVERHANG_PX; d <= OVERHANG_PX; d++) in |= under(x - OVERHANG_PX + d, y);
                break;
            }
            b.on[size_t(y * b.w + x)] = in;
        }
    if (t.shape != TableShape::Dining && t.shape != TableShape::Bench && t.shape != TableShape::Corner) return b;
    // The bigger square shapes get their outer corners rounded off by a
    // pixel; a side table stays square, or it would pass for a round one.
    Body square = b;
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++) {
            if (!square.at(x, y)) continue;
            bool l = square.at(x - 1, y), r = square.at(x + 1, y), u = square.at(x, y - 1), d = square.at(x, y + 1);
            if ((!l && !u) || (!u && !r) || (!r && !d) || (!d && !l)) b.on[size_t(y * b.w + x)] = false;
        }
    return b;
}

enum class Legs { Ends, Post, Posts, Plinth };

Legs legsOf(const Table& t) {
    switch (t.shape) {
    case TableShape::Pedestal:
    case TableShape::Round: return Legs::Post;
    case TableShape::Oval: return t.size.x > t.size.y ? Legs::Posts : Legs::Post;
    default: break;
    }
    return t.material == TableMaterial::Marble || t.material == TableMaterial::Granite ? Legs::Plinth : Legs::Ends;
}

/// Whether the column holds up the table, in a run of the bottom row from x0 to x1.
bool legAt(const Table& t, Legs legs, int w, int x, int x0, int x1) {
    bool thick = t.material != TableMaterial::Steel && t.material != TableMaterial::Glass;
    switch (legs) {
    case Legs::Ends: {
        int k = thick && x1 - x0 >= 11 ? 1 : 0;
        return x <= x0 + k || x >= x1 - k;
    }
    case Legs::Plinth: return x > x0 && x < x1;
    case Legs::Post: return x == w / 2 - 1 || x == w / 2;
    case Legs::Posts: {
        int r = t.size.y * CELL_PX / 2;
        return x == r - 1 || x == r || x == w - r - 1 || x == w - r;
    }
    }
    return false;
}

/// What is drawn on the top's inside: grain along wood, veins through
/// marble, flecks in granite, a sheen on steel, streaks of light on glass.
Rgba topPixel(const Table& t, const Look& k, int x, int y) {
    bool across = t.size.x >= t.size.y;
    int along = across ? x : y, row = across ? y : x;
    uint32_t n = noise(t.seed, x, y);
    switch (t.material) {
    case TableMaterial::Oak:
    case TableMaterial::Walnut:
    case TableMaterial::Pine: {
        uint32_t r = noise(t.seed, 0, row);
        return row % 2 == int(t.seed % 2) && (along + int(r % 7)) % 7 < 4 ? k.grain : k.top;
    }
    case TableMaterial::Steel: return (row + int(n % 6 == 0)) % 2 ? k.grain : k.top;
    case TableMaterial::Marble: {
        int wobble = int(noise(t.seed, 0, y) % 3);
        return (x + y + wobble + int(t.seed % 11)) % 11 == 0 ? k.grain : k.top;
    }
    case TableMaterial::Granite:
        if (n % 5 == 0) return k.grain;
        return n % 7 == 0 ? k.hi : k.top;
    case TableMaterial::Glass: {
        int d = x - y + int(t.seed % 9);
        return ((d % 9) + 9) % 9 == 0 || ((d % 9) + 9) % 9 == 2 ? k.grain : k.top;
    }
    case TableMaterial::Plastic:
    case TableMaterial::Count: break;
    }
    return k.top;
}

} // namespace

// The top is the footprint lifted by the rise; the strip of footprint it
// leaves bare below is the front face: a rim, then the legs, with shadow
// between them. Each pixel is shown as the cell under that bit of the
// table is.
void drawTable(const Table& t, int x, int y, const std::function<CellView(Vec2i)>& view) {
    const Body b = bodyOf(t);
    const Look k = lookOf(t);
    const Legs legs = legsOf(t);
    auto top = [&](int px, int py) { return b.at(px, py + TABLE_RISE_PX); };
    auto bottom = [&](int px, int py) {
        return b.at(px, py) && !top(px, py) && !b.at(px, py + 1) && !top(px, py - 1);
    };
    for (int py = -TABLE_RISE_PX; py < b.h; py++)
        for (int px = 0; px < b.w; px++) {
            bool onTop = top(px, py);
            if (!onTop && !b.at(px, py)) continue;
            int fy = onTop ? py + TABLE_RISE_PX : py;
            Vec2i cell{std::clamp((px - OVERHANG_PX) / CELL_PX, 0, t.size.x - 1), fy / CELL_PX};
            if (!t.coversLocal(cell)) cell = {std::clamp((px - 2 * OVERHANG_PX) / CELL_PX, 0, t.size.x - 1), fy / CELL_PX};
            if (!t.coversLocal(cell)) cell = {std::clamp(px / CELL_PX, 0, t.size.x - 1), fy / CELL_PX};
            CellView v = view(t.at + cell);
            if (v == CellView::Unknown) continue;
            bool lit = v == CellView::Lit;
            Rgba c;
            if (onTop) {
                bool rimHi = !top(px, py - 1) || !top(px - 1, py);
                bool rimLo = !top(px, py + 1) || !top(px + 1, py);
                c = rimLo ? k.edge : rimHi ? k.hi : topPixel(t, k, px, fy);
            } else if (!bottom(px, py)) {
                c = k.side;
            } else {
                // The bottom row: legs or shadow, along the run it is part of.
                int x0 = px, x1 = px;
                while (bottom(x0 - 1, py)) x0--;
                while (bottom(x1 + 1, py)) x1++;
                if (!legAt(t, legs, b.w, px, x0, x1)) {
                    Color s = SHADOW;
                    if (!lit) s.a = (unsigned char)(s.a * MEMORY_DIM);
                    DrawPixel(x - OVERHANG_PX + px, y + py, s);
                    continue;
                }
                c = k.leg;
            }
            DrawPixel(x - OVERHANG_PX + px, y + py, toColor(c, lit));
        }
}
