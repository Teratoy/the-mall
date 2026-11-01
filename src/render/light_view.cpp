#include "render/light_view.h"
#include "human/human.h"
#include "render/canvas.h"
#include "world/world.h"
#include <raylib.h>
#include <cmath>
#include <vector>

namespace {

/// Unlit: what is left of a colour in the dark.
constexpr Color DARK = {58, 62, 98, 255};
/// How far each light level goes from the dark to the source's colour.
constexpr float LEVEL_MIX[LIGHT_FULL + 1] = {0.0f, 0.6f, 0.9f, 1.0f};
/// The 4x4 ordered dither the light steps from cell to cell by.
constexpr int BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/// Each kind of source's colour in full light: strip lights a cold
/// white-green, vending machines warm, screens and racks their own glow.
Color tintOf(LightKind k) {
    switch (k) {
    case LightKind::Fitting: return {240, 246, 236, 255};
    case LightKind::NightLight: return {170, 190, 255, 255};
    case LightKind::Vending: return {255, 214, 160, 255};
    case LightKind::Screen: return {170, 230, 255, 255};
    case LightKind::Rack: return {170, 255, 190, 255};
    case LightKind::Dispenser: return {190, 220, 255, 255};
    case LightKind::Statue: return {255, 220, 170, 255};
    case LightKind::Torch: return {255, 200, 140, 255};
    case LightKind::None: break;
    }
    return DARK;
}

Color mix(Color a, Color b, float k) {
    auto m = [k](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * k + 0.5f); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), 255};
}

} // namespace

void drawLight(const World& world, const Human& viewer, bool reveal, int x0, int y0, int x1, int y1, int left,
               int top) {
    // Each cell's colour, with a cell of margin all round for the dither.
    const int cw = x1 - x0 + 3, ch = y1 - y0 + 3;
    std::vector<Color> cells(size_t(cw * ch));
    std::vector<bool> walls(size_t(cw * ch));
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++) {
            Vec2i c{x0 - 1 + x, y0 - 1 + y};
            Tile t = world.tile(c);
            size_t i = size_t(y * cw + x);
            walls[i] = opaque(t);
            if (t == Tile::Void || (!reveal && !viewer.sight.sees(c))) cells[i] = WHITE;
            else cells[i] = mix(DARK, tintOf(world.lightFrom(c)), LEVEL_MIX[world.light(c)]);
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
    // Each pixel takes its own cell's colour or a neighbour's, by how near
    // that neighbour's middle it lies against the dither; never a wall's
    // over the floor, so light doesn't leak through one.
    for (int py = 0; py < h; py++) {
        const int wy = y0 * CELL_PX + py;
        const int ownY = py / CELL_PX + 1;
        const float v = (float(py % CELL_PX) + 0.5f) / CELL_PX - 0.5f;
        for (int px = 0; px < w; px++) {
            const int wx = x0 * CELL_PX + px;
            const int ownX = px / CELL_PX + 1;
            const float u = (float(px % CELL_PX) + 0.5f) / CELL_PX - 0.5f;
            const float t = (float(BAYER[wy & 3][wx & 3]) + 0.5f) / 16.0f;
            int cx = ownX, cy = ownY;
            if (std::abs(u) > t) cx += u < 0 ? -1 : 1;
            if (std::abs(v) > t) cy += v < 0 ? -1 : 1;
            const size_t own = size_t(ownY * cw + ownX);
            size_t pick = size_t(cy * cw + cx);
            if (walls[pick] && !walls[own]) pick = own;
            pixels[size_t(py * w + px)] = cells[pick];
        }
    }
    UpdateTexture(texture, pixels.data());
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexture(texture, x0 * CELL_PX - left, y0 * CELL_PX - top, WHITE);
    EndBlendMode();
}
