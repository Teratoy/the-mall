#include "render/bottle_art.h"
#include "render/water_art.h"
#include <algorithm>
#include <cmath>

namespace {

/// The cap, and the clear glass where there is no water.
constexpr Color CAP = {196, 210, 222, 255};
constexpr Color CAP_SHADE = {120, 134, 150, 255};
constexpr Color GLASS = {200, 222, 236, 130};
/// The top of the water catches the light.
constexpr float SURFACE_LIGHT = 0.35f;
/// The stream falls this fast, in pixels a second, broken every few pixels.
constexpr float STREAM_SPEED = 24.0f;
constexpr int STREAM_GAP = 3;

Color lighter(Color c, float k) {
    auto m = [k](unsigned char v) { return (unsigned char)(v + (255 - v) * k); };
    return {m(c.r), m(c.g), m(c.b), c.a};
}

Color tinted(Color c, Color tint) {
    return {(unsigned char)(c.r * tint.r / 255), (unsigned char)(c.g * tint.g / 255),
            (unsigned char)(c.b * tint.b / 255), (unsigned char)(c.a * tint.a / 255)};
}

/// How many steps of it the water fills: any at all shows as one.
int filled(const Bottle& b) {
    if (b.empty()) return 0;
    return std::clamp(int(std::ceil(b.water * BOTTLE_STEPS - 0.01f)), 1, BOTTLE_STEPS);
}

} // namespace

// Along runs from the base (0) to the cap (BOTTLE_LONG - 1); across from
// one side (0) to the other. Upright or upside down the water lies along,
// from whichever end is down; on its side it lies across, the bottom row
// first, then the top once more than half full.
void forBottlePixels(int x, int y, BottleTurn turn, bool flip, const Bottle& b,
                     const std::function<void(int, int, Color)>& put) {
    Color water = waterColor(b.red, b.foul);
    int steps = filled(b);
    bool lying = turn == BottleTurn::Ahead || turn == BottleTurn::Back;
    for (int along = 0; along < BOTTLE_LONG; along++)
        for (int across = 0; across < BOTTLE_ACROSS; across++) {
            int px = x + across, py = y - along;
            switch (turn) {
            case BottleTurn::Up: break;
            case BottleTurn::Down: px = x + across, py = y + along; break;
            case BottleTurn::Ahead: px = x + along, py = y - across; break;
            case BottleTurn::Back: px = x - along, py = y - across; break;
            }
            if (flip) px = SPRITE_W - 1 - px;
            Color c;
            if (along == BOTTLE_LONG - 1) {
                c = across == 0 ? CAP : CAP_SHADE;
            } else if (lying) {
                bool wet = across == 0 ? steps > 0 : steps > BOTTLE_STEPS / 2;
                c = !wet ? GLASS : across == 1 || steps <= BOTTLE_STEPS / 2 ? lighter(water, SURFACE_LIGHT) : water;
            } else {
                // Upside down, the water sits at the cap's end.
                int level = turn == BottleTurn::Down ? BOTTLE_STEPS - 1 - along : along;
                c = level >= steps ? GLASS : level == steps - 1 ? lighter(water, SURFACE_LIGHT) : water;
            }
            put(px, py, c);
        }
}

void drawBottle(Vector2 origin, int x, int y, BottleTurn turn, bool flip, const Bottle& b, int scale, Color tint) {
    forBottlePixels(x, y, turn, flip, b, [&](int px, int py, Color c) {
        DrawRectangle(int(origin.x) + px * scale, int(origin.y) + py * scale, scale, scale, tinted(c, tint));
    });
}

void drawPourStream(Vector2 origin, int x, int y, int floorY, bool flip, const Bottle& b, int scale, Color tint,
                    float time) {
    Color c = tinted(lighter(waterColor(b.red, b.foul), SURFACE_LIGHT), tint);
    int px = x + BOTTLE_ACROSS / 2;
    if (flip) px = SPRITE_W - 1 - px;
    int shift = int(time * STREAM_SPEED);
    for (int py = y + BOTTLE_LONG; py <= floorY; py++)
        if ((py - shift) % STREAM_GAP != 0)
            DrawRectangle(int(origin.x) + px * scale, int(origin.y) + py * scale, scale, scale, c);
}

void drawBottleIcon(int x, int y, const Bottle& b) {
    forBottlePixels(0, BOTTLE_LONG - 1, BottleTurn::Up, false, b,
                    [&](int px, int py, Color c) { DrawPixel(x + px, y + py, c); });
}
