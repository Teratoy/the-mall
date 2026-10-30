#include "render/vending_art.h"
#include <raylib.h>

namespace {

/// Seen from the front. K the dark trim, o the LEDs round the sign, L the
/// lit sign, c l s the cake drawn on it, B the body, b its shaded right
/// side and foot, G the glass, g a glint on it, w the coil each cake sits on, P the panel, d its little
/// screen, p the buttons, y the coin slot, D the drop the cake falls into.
constexpr const char* VENDING_GLYPH[VENDING_H] = {
    "oooooooooooo",
    "oLLLLcLLLLLo",
    "oLLLlllLLLLo",
    "oLLLsssLLLLo",
    "oooooooooooo",
    "BgGGGGGGGPPb",
    "BgGGGGGGGddb",
    "BGGGGGGGGPPb",
    "BwwwwwwwwppB",
    "BGGGGgGGGPPb",
    "BGGGGgGGGppb",
    "BGGGGGGGGPPb",
    "BwwwwwwwwppB",
    "BGGGGGGGGPPb",
    "BGGGGGGGGyPb",
    "BGGGGGGGGyPb",
    "BwwwwwwwwPPb",
    "BBBBBBBBBBBb",
    "BKDDDDDDKBBb",
    "BKDDDDDDKBBb",
    "bbbbbbbbbbbb",
};
static_assert(sizeof VENDING_GLYPH / sizeof VENDING_GLYPH[0] == VENDING_H);

/// Seen from its right, the front's lit edge on the left: the sign's side
/// in LEDs, then the shaded flank with the mall's spiral on it in m, the
/// game's icon wound three times fewer, and a cake painted below.
constexpr const char* SIDE_GLYPH[VENDING_H] = {
    "ooooooooo",
    "oLLLLLLLo",
    "oLLLLLLLo",
    "oLLLLLLLo",
    "ooooooooo",
    "Bbbbbbbbb",
    "Bmmmmmmmb",
    "Bbbbbbbmb",
    "Bmmmmmbmb",
    "Bmbbbmbmb",
    "Bmbmmmbmb",
    "Bmbbbbbmb",
    "Bmmmmmmmb",
    "Bbbbbbbbb",
    "Bbbbcbbbb",
    "Bblllllbb",
    "Bblslslbb",
    "Bbsssssbb",
    "Bblllllbb",
    "Bbsssssbb",
    "KKKKKKKKK",
};
static_assert(sizeof SIDE_GLYPH / sizeof SIDE_GLYPH[0] == VENDING_H);

/// A cake behind the glass, small: c the cherry, f frosting, s sponge.
constexpr const char* SLOT_CAKE[3] = {".c.", "fff", "sss"};
/// Where each slot's cake has its top-left: two columns, three shelves.
constexpr int SLOT_X[2] = {2, 6};
constexpr int SLOT_Y[3] = {5, 9, 13};

constexpr Rgba TRIM = hex(0x3a2630);
constexpr Rgba SIGN = hex(0xfff2d8);
/// The LEDs round the sign, every other one gold, the rest pink.
constexpr Rgba LED_GOLD = hex(0xffd84a);
constexpr Rgba LED_PINK = hex(0xff7ab0);
constexpr Rgba BODY = hex(0xd8607e);
constexpr Rgba BODY_SHADE = hex(0xa84660);
constexpr Rgba GLASS = hex(0x2e3a4c);
constexpr Rgba GLINT = hex(0x8ea6c0);
constexpr Rgba COIL = hex(0x9a9ca2);
constexpr Rgba PANEL = hex(0x5a4e58);
constexpr Rgba SCREEN = hex(0x7ee08a);
constexpr Rgba BUTTON = hex(0xf2d060);
constexpr Rgba COIN_SLOT = hex(0x16121a);
constexpr Rgba DROP = hex(0x120c12);
/// The same colours the cakes on the floor are drawn in.
constexpr Rgba CHERRY = {200, 48, 58, 255};
constexpr Rgba FROSTING = {236, 180, 196, 255};
constexpr Rgba SPONGE = {176, 124, 80, 255};
/// The LEDs swap colours this many times a second, so they seem to run round the sign.
constexpr float LED_BLINKS = 4.0f;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

Rgba cakePixel(char g) { return g == 'c' ? CHERRY : g == 'f' ? FROSTING : g == 's' ? SPONGE : Rgba{}; }

/// The colour a glyph letter stands for, the LEDs taking turns by blink.
Rgba ink(char g, int gx, int gy, int blink) {
    switch (g) {
    case 'K': return TRIM;
    case 'o': return (gx + gy + blink) % 2 ? LED_PINK : LED_GOLD;
    case 'L':
    case 'm': return SIGN;
    case 'c': return CHERRY;
    case 'l': return FROSTING;
    case 's': return SPONGE;
    case 'B': return BODY;
    case 'b': return BODY_SHADE;
    case 'G': return GLASS;
    case 'g': return GLINT;
    case 'w': return COIL;
    case 'P': return PANEL;
    case 'd': return SCREEN;
    case 'p': return BUTTON;
    case 'y': return COIN_SLOT;
    case 'D': return DROP;
    default: return {};
    }
}

/// Draws a glyph's pixels from px() with its bottom-left at x, y, dimmed unless lit.
template <typename F> void drawGlyph(int x, int y, int w, bool lit, F px) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < VENDING_H; gy++)
        for (int gx = 0; gx < w; gx++) {
            Rgba c = px(gx, gy);
            if (!c.solid()) continue;
            DrawPixel(x + gx, y - VENDING_H + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}

int blinkAt(bool lit, float seconds) { return lit ? int(seconds * LED_BLINKS) : 0; }

} // namespace

Rgba vendingPixel(int gx, int gy, uint8_t stock, int blink) {
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 2; col++) {
            int dx = gx - SLOT_X[col], dy = gy - SLOT_Y[row];
            if (dx < 0 || dy < 0 || dx > 2 || dy > 2 || !(stock >> (row * 2 + col) & 1)) continue;
            if (Rgba c = cakePixel(SLOT_CAKE[dy][dx]); c.solid()) return c;
        }
    return ink(VENDING_GLYPH[gy][gx], gx, gy, blink);
}

Rgba vendingSidePixel(int gx, int gy, int blink) { return ink(SIDE_GLYPH[gy][gx], gx, gy, blink); }

void drawVendingMachine(int x, int y, uint8_t stock, bool lit, float seconds) {
    int blink = blinkAt(lit, seconds);
    drawGlyph(x, y, VENDING_W, lit, [&](int gx, int gy) { return vendingPixel(gx, gy, stock, blink); });
}

void drawVendingMachineSide(int x, int y, bool lit, float seconds) {
    int blink = blinkAt(lit, seconds);
    drawGlyph(x, y, VENDING_DEPTH, lit, [&](int gx, int gy) { return vendingSidePixel(gx, gy, blink); });
}
