#include "render/light_view.h"
#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <vector>

namespace {

/// A colour to multiply by, each channel 0 to 1.
struct Shade {
    float r, g, b;
};

/// Unlit: what is left of a colour in the dark.
constexpr Shade DARK = {0.33f, 0.34f, 0.48f};
/// How far each light level goes from the dark to the source's colour.
constexpr float LEVEL_MIX[LIGHT_FULL + 1] = {0.0f, 0.6f, 0.9f, 1.0f};
/// Remembered: as bright as the light it was seen in, greyed, at this.
/// Art out of sight is already drawn this dim, so the layer only adds the
/// light it was seen in.
constexpr float MEMORY_DIM = 0.45f;
/// Remembered dark is never darker than this much of remembered light, so
/// the way through a dark place stays readable.
constexpr float MEMORY_DARK = 0.6f;
/// Eyes get used to the dark: right round the viewer it makes out this
/// much, fading to nothing a cell past where it sees in the dark.
constexpr Shade EYE_GLOW = {0.58f, 0.59f, 0.7f};
constexpr float EYE_GLOW_CELLS = float(Sight::DARK_SIGHT + 1);
/// The last two rings of sight fade toward how memory looks, by these.
constexpr float SIGHT_FADE_INNER = 0.25f, SIGHT_FADE_OUTER = 0.75f;
/// Shades are stepped into this many steps from black to full, and each
/// pixel between two steps takes the upper one by the pattern.
constexpr int SHADE_STEPS = 8;
/// Each pattern as the order, 0 first, in which the pixels of a 4x4 block
/// go up to the next step as the shade rises toward it.
constexpr int PATTERNS[DITHER_COUNT][4][4] = {
    // Ordered (Bayer): scattered dots.
    {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}},
    // Hatched: rising diagonals, every other one first.
    {{0, 8, 4, 12}, {9, 5, 13, 1}, {6, 14, 2, 10}, {15, 3, 11, 7}},
    // Crossed: a rising diagonal, then the falling one over it.
    {{0, 10, 7, 15}, {8, 4, 13, 3}, {6, 14, 2, 11}, {12, 1, 9, 5}},
    // Lines: level rows, every other one first.
    {{0, 2, 1, 3}, {8, 10, 9, 11}, {4, 6, 5, 7}, {12, 14, 13, 15}},
};
Dither pattern = Dither::Ordered;
/// The pattern breathes: each dot's threshold swings by this much, at its
/// own phase, once every PULSE_SECONDS, so the pixels on the edge of a step
/// come and go one by one.
constexpr float PULSE = 0.04f;
constexpr float PULSE_SECONDS = 2.4f;

/// Each kind of source's colour in full light: strip lights a cool
/// off-white, vending machines warm, screens and racks their own glow.
Shade tintOf(LightKind k) {
    switch (k) {
    case LightKind::Fitting: return {0.94f, 0.96f, 0.93f};
    case LightKind::NightLight: return {0.67f, 0.75f, 1.0f};
    case LightKind::Vending: return {1.0f, 0.84f, 0.63f};
    case LightKind::Screen: return {0.67f, 0.9f, 1.0f};
    case LightKind::Rack: return {0.67f, 1.0f, 0.75f};
    case LightKind::Dispenser: return {0.75f, 0.86f, 1.0f};
    case LightKind::Statue: return {1.0f, 0.86f, 0.67f};
    case LightKind::Torch: return {1.0f, 0.78f, 0.55f};
    case LightKind::None: break;
    }
    return DARK;
}

Shade mix(Shade a, Shade b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }
Shade brighter(Shade a, Shade b) { return {std::max(a.r, b.r), std::max(a.g, b.g), std::max(a.b, b.b)}; }
Shade scaled(Shade a, float k) { return {a.r * k, a.g * k, a.b * k}; }
/// The light as memory keeps it: how bright, not what colour.
Shade greyed(Shade a) {
    float y = 0.3f * a.r + 0.59f * a.g + 0.11f * a.b;
    return {y, y, y};
}
/// How a cell lit so looks once out of sight.
Shade remembered(Shade a) { return scaled(brighter(greyed(a), {MEMORY_DARK, MEMORY_DARK, MEMORY_DARK}), MEMORY_DIM); }
Shade lit(int level, LightKind from) { return mix(DARK, tintOf(from), LEVEL_MIX[level]); }

/// One cell of the layer: the shade it should end up at, and how dim its
/// art is drawn already.
struct Cell {
    Shade want{1.0f, 1.0f, 1.0f};
    float drawn = 1.0f;
    bool wall = false;
    bool shown = false;
};

} // namespace

void setDither(Dither d) { pattern = d; }
Dither dither() { return pattern; }
const char* ditherName(Dither d) {
    static const char* NAMES[DITHER_COUNT] = {"ordered", "hatched", "crossed", "lines"};
    return NAMES[int(d)];
}

// Each cell gets the shade it should end at. In sight, the light on it now,
// or what the viewer's eyes make out near it if that is more, fading over
// the last two rings into how it will be remembered. Out of sight, the
// light it was seen in, greyed and dimmed. Each pixel then blends the
// shades of the four cells whose middles are round it, divided by how dim
// its own art is drawn: so it can darken art toward a dark neighbour but
// never brighten memory, and the edge of sight fades on the side in sight.
// Never a wall's shade over floor, so light doesn't leak through one. The
// blend is stepped through the pattern, so it falls off in pixels.
void drawLight(const World& world, const Human& viewer, bool reveal, int x0, int y0, int x1, int y1, int left,
               int top) {
    const int cw = x1 - x0 + 3, ch = y1 - y0 + 3;
    std::vector<Cell> cells(size_t(cw * ch));
    const Vec2i eye = viewer.sight.eye();
    const float radius = float(viewer.sight.radius());
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++) {
            Vec2i c{x0 - 1 + x, y0 - 1 + y};
            Cell& cell = cells[size_t(y * cw + x)];
            Tile t = reveal ? world.tile(c) : viewer.memory.tile(c);
            if (t == Tile::Void) continue;
            cell.shown = true;
            cell.wall = opaque(t);
            if (reveal) {
                cell.want = lit(world.light(c), world.lightFrom(c));
                continue;
            }
            if (!viewer.sight.inView(c)) {
                auto seen = viewer.memory.lightSeen(c);
                cell.want = remembered(seen ? lit(seen->level, seen->from) : Shade{1.0f, 1.0f, 1.0f});
                cell.drawn = MEMORY_DIM;
                continue;
            }
            const float d = std::hypot(float(c.x - eye.x), float(c.y - eye.y));
            Shade now = lit(world.light(c), world.lightFrom(c));
            now = brighter(now, mix(DARK, EYE_GLOW, std::max(0.0f, 1.0f - d / EYE_GLOW_CELLS)));
            if (d > radius - 2.0f) {
                const float fade = d > radius - 1.0f ? SIGHT_FADE_OUTER : SIGHT_FADE_INNER;
                now = mix(now, remembered(now), fade);
            }
            cell.want = now;
        }

    static std::vector<Color> pixels;
    static Texture2D texture{};
    const int w = (x1 - x0 + 1) * CELL_PX, h = (y1 - y0 + 1) * CELL_PX;
    if (texture.id == 0 || texture.width != w || texture.height != h) {
        if (texture.id != 0) UnloadTexture(texture);
        Image blank = GenImageColor(w, h, WHITE);
        texture = LoadTextureFromImage(blank);
        UnloadImage(blank);
    }
    pixels.resize(size_t(w * h));
    // A shade over art drawn this dim, stepped to the next step up or down
    // by the pattern.
    auto channel = [](float want, float drawn, float t) {
        float x = std::min(1.0f, want / drawn) * float(SHADE_STEPS);
        float step = std::floor(x);
        if (x - step > t) step += 1.0f;
        return (unsigned char)std::lround(255.0f * std::min(1.0f, step / float(SHADE_STEPS)));
    };
    // Each place in the pattern its own phase, the golden angle apart, so
    // neighbouring dots swing out of step.
    float thresholds[16];
    const float swing = float(GetTime()) * 2.0f * PI / PULSE_SECONDS;
    for (int r = 0; r < 16; r++) thresholds[r] = (float(r) + 0.5f) / 16.0f + PULSE * std::sin(swing + float(r) * 2.4f);
    for (int py = 0; py < h; py++) {
        const int wy = y0 * CELL_PX + py;
        const int ownY = py / CELL_PX + 1;
        const float v = (float(py % CELL_PX) + 0.5f) / CELL_PX - 0.5f;
        const int ny = ownY + (v < 0 ? -1 : 1);
        for (int px = 0; px < w; px++) {
            const int wx = x0 * CELL_PX + px;
            const int ownX = px / CELL_PX + 1;
            const Cell& own = cells[size_t(ownY * cw + ownX)];
            if (!own.shown) {
                pixels[size_t(py * w + px)] = WHITE;
                continue;
            }
            const float u = (float(px % CELL_PX) + 0.5f) / CELL_PX - 0.5f;
            const int nx = ownX + (u < 0 ? -1 : 1);
            auto at = [&](int x, int y) {
                const Cell& c = cells[size_t(y * cw + x)];
                return !c.shown || (c.wall && !own.wall) ? own.want : c.want;
            };
            const float ku = std::abs(u), kv = std::abs(v);
            const Shade want = mix(mix(own.want, at(nx, ownY), ku), mix(at(ownX, ny), at(nx, ny), ku), kv);
            const float t = thresholds[PATTERNS[int(pattern)][wy & 3][wx & 3]];
            pixels[size_t(py * w + px)] = {channel(want.r, own.drawn, t), channel(want.g, own.drawn, t),
                                           channel(want.b, own.drawn, t), 255};
        }
    }
    UpdateTexture(texture, pixels.data());
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexture(texture, x0 * CELL_PX - left, y0 * CELL_PX - top, WHITE);
    EndBlendMode();
}
