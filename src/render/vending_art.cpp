#include "render/vending_art.h"
#include "render/sprites.h"
#include <raylib.h>

namespace {

/// A machine's sprites in data/sprites.txt (its front, with the sign and the
/// coils, where the LEDs blink as o and O; its side; one of its goods, of
/// this size) and how it holds its goods: up to three columns on up to
/// three shelves, each slot's top-left on the front.
struct Look {
    const char *front, *side, *goods;
    int goodsW, goodsH;
    int cols, rows;
    int slotX[3], slotY[3];
};

constexpr Look LOOKS[] = {
    {"cake machine", "cake machine side", "cake machine goods", 3, 3, 2, 3, {2, 6}, {5, 9, 13}},
    {"bottle machine", "bottle machine side", "bottle machine goods", 2, 5, 3, 2, {1, 4, 7}, {5, 11}},
    {"gun machine", "gun machine side", "gun machine goods", 6, 5, 1, 2, {2}, {5, 11}},
};
static_assert(sizeof LOOKS / sizeof LOOKS[0] == size_t(Vend::Count));

/// The LEDs swap colours this many times a second, so they seem to run round the sign.
constexpr float LED_BLINKS = 4.0f;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

const Look& lookOf(Vend kind) { return LOOKS[size_t(kind)]; }

/// A machine's sprites, looked up once.
struct Art {
    const Sprite *front = nullptr, *side = nullptr, *goods = nullptr;
};

const Art& artOf(Vend kind) {
    static Art arts[size_t(Vend::Count)];
    Art& art = arts[size_t(kind)];
    if (!art.front) {
        const Look& look = lookOf(kind);
        art.front = &sprites::get(look.front, VENDING_W, VENDING_H);
        art.side = &sprites::get(look.side, VENDING_DEPTH, VENDING_H);
        art.goods = &sprites::get(look.goods, look.goodsW, look.goodsH);
    }
    return art;
}

/// Draws a glyph's pixels from px() with its bottom-left at x, y, dimmed
/// unless in sight, and mirrored left to right when flipped.
template <typename F> void drawGlyph(int x, int y, int w, bool inSight, F px, bool flip = false) {
    float k = inSight ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < VENDING_H; gy++)
        for (int gx = 0; gx < w; gx++) {
            Rgba c = px(gx, gy);
            if (!c.solid()) continue;
            DrawPixel(x + (flip ? w - 1 - gx : gx), y - VENDING_H + gy,
                      {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}

int blinkAt(bool inSight, float seconds) { return inSight ? int(seconds * LED_BLINKS) : 0; }

} // namespace

int vendingSlots(Vend kind) { return lookOf(kind).cols * lookOf(kind).rows; }

void vendingSlotAt(Vend kind, int slot, int& x, int& y) {
    const Look& look = lookOf(kind);
    x = look.slotX[slot % look.cols], y = look.slotY[slot / look.cols];
}

uint8_t vendingFull(Vend kind) { return uint8_t((1u << vendingSlots(kind)) - 1); }

Rgba vendingPixel(Vend kind, int gx, int gy, uint8_t stock, int blink) {
    const Look& look = lookOf(kind);
    const Art& art = artOf(kind);
    for (int row = 0; row < look.rows; row++)
        for (int col = 0; col < look.cols; col++) {
            if (!(stock >> (row * look.cols + col) & 1)) continue;
            int dx = gx - look.slotX[col], dy = gy - look.slotY[row];
            if (dx < 0 || dy < 0 || dx >= look.goodsW || dy >= look.goodsH) continue;
            Rgba c = art.goods->pixel(dx, dy);
            if (c.solid()) return c;
        }
    return art.front->pixel(gx, gy, blink);
}

Rgba vendingSidePixel(Vend kind, int gx, int gy, int blink) { return artOf(kind).side->pixel(gx, gy, blink); }

void drawVendingMachine(Vend kind, int x, int y, uint8_t stock, bool inSight, float seconds) {
    int blink = blinkAt(inSight, seconds);
    drawGlyph(x, y, VENDING_W, inSight, [&](int gx, int gy) { return vendingPixel(kind, gx, gy, stock, blink); });
}

void drawVendingMachineSide(Vend kind, int x, int y, bool inSight, float seconds, bool flip) {
    int blink = blinkAt(inSight, seconds);
    drawGlyph(x, y, VENDING_DEPTH, inSight, [&](int gx, int gy) { return vendingSidePixel(kind, gx, gy, blink); }, flip);
}
