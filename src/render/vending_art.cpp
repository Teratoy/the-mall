#include "render/vending_art.h"
#include "render/gun_art.h"
#include <raylib.h>

namespace {

/// Seen from the front, the same for every machine. K the dark trim, o the
/// LEDs round the sign, S the sign, where what it sells is drawn, B the
/// body, b its shaded right side and foot, G the glass, g a glint on it,
/// P the panel, d its little screen, p the buttons, y the coin slot, D the
/// drop what is bought falls into. The coils the goods sit on lie across
/// the glass at each kind's own shelves.
constexpr const char* FRONT_GLYPH[VENDING_H] = {
    "oooooooooooo",
    "oSSSSSSSSSSo",
    "oSSSSSSSSSSo",
    "oSSSSSSSSSSo",
    "oooooooooooo",
    "BgGGGGGGGPPb",
    "BgGGGGGGGddb",
    "BGGGGGGGGPPb",
    "BGGGGGGGGppB",
    "BGGGGgGGGPPb",
    "BGGGGgGGGppb",
    "BGGGGGGGGPPb",
    "BGGGGGGGGppB",
    "BGGGGGGGGPPb",
    "BGGGGGGGGyPb",
    "BGGGGGGGGyPb",
    "BGGGGGGGGPPb",
    "BBBBBBBBBBBb",
    "BKDDDDDDKBBb",
    "BKDDDDDDKBBb",
    "bbbbbbbbbbbb",
};
static_assert(sizeof FRONT_GLYPH / sizeof FRONT_GLYPH[0] == VENDING_H);

/// Seen from its right, the front's lit edge on the left: the sign's side
/// in LEDs, then the shaded flank with the mall's spiral on it in m, the
/// game's icon wound three times fewer, and what it sells painted below in
/// the E rows.
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
    "BEEEEEEEb",
    "BEEEEEEEb",
    "BEEEEEEEb",
    "BEEEEEEEb",
    "BEEEEEEEb",
    "BEEEEEEEb",
    "KKKKKKKKK",
};
static_assert(sizeof SIDE_GLYPH / sizeof SIDE_GLYPH[0] == VENDING_H);
/// Where the sign's picture and the flank's painting have their top-left.
constexpr int SIGN_X = 1, SIGN_Y = 1, DECO_X = 1, DECO_Y = 14;
/// The glass runs over these columns; the coils span it.
constexpr int GLASS_LEFT = 1, GLASS_RIGHT = 8;

/// Letters the goods are drawn in, on the sign, the flank and behind the
/// glass. Cake: c the cherry, l frosting, s sponge. Bottle: a its cap, A the
/// cap's shade, q its glass, Q a glint on it, v its glass
/// deeper, to show on the sign. Gun: as in gun_art.
/// '.' shows what is behind.
constexpr const char* CAKE_ITEM[] = {".c.", "lll", "sss"};
constexpr const char* BOTTLE_ITEM[] = {"aA", "Qq", "qq", "qq", "qq"};
/// The gun as it lies on the floor, without its empty top row.
constexpr const char* GUN_ITEM[] = {"nnnnnn", "NNNNNN", "rr.t..", "rrtt..", "rr...."};

/// A machine's look and how it holds its goods: up to three columns on up
/// to three shelves, the coils under each shelf.
struct Look {
    Rgba body, shade, ledA, ledB;
    const char* sign[3];
    const char* deco[6];
    const char* const* item;
    int itemW, itemH;
    int cols, rows;
    int slotX[3], slotY[3], coil[3];
};

constexpr Look LOOKS[] = {
    {hex(0xd8607e), hex(0xa84660), hex(0xffd84a), hex(0xff7ab0),
     {"....c.....", "...lll....", "...sss...."},
     {"...c...", ".lllll.", ".lslsl.", ".sssss.", ".lllll.", ".sssss."},
     CAKE_ITEM, 3, 3, 2, 3, {2, 6}, {5, 9, 13}, {8, 12, 16}},
    {hex(0x4a9ac4), hex(0x346f92), hex(0xffffff), hex(0x6ee0ff),
     {"..aA..aA..", "..qv..qv..", "..vv..vv.."},
     {"...a...", "...q...", "..qqq..", "..Qqq..", "..Qqq..", "..qqq.."},
     BOTTLE_ITEM, 2, 5, 3, 2, {1, 4, 7}, {5, 11}, {10, 16}},
    {hex(0x5e6648), hex(0x434a32), hex(0xff4a3a), hex(0xffb03a),
     {"..nnnnnn..", "..NNNNNN..", "..rrt....."},
     {".......", "nnnnnnn", "NNNNNNN", "rr.t...", "rrtt...", "rr....."},
     GUN_ITEM, 6, 5, 1, 2, {2}, {5, 11}, {10, 16}},
};
static_assert(sizeof LOOKS / sizeof LOOKS[0] == size_t(Vend::Count));

constexpr Rgba TRIM = hex(0x3a2630);
constexpr Rgba SIGN = hex(0xfff2d8);
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
/// An empty bottle, as the bottles humans carry are drawn, its glass made solid.
constexpr Rgba CAP = {196, 210, 222, 255};
constexpr Rgba CAP_SHADE = {120, 134, 150, 255};
constexpr Rgba BOTTLE_GLASS = hex(0x9cc0d6);
constexpr Rgba BOTTLE_GLINT = hex(0xe4f2fa);
constexpr Rgba BOTTLE_DEEP = hex(0x5a8cb0);
/// The LEDs swap colours this many times a second, so they seem to run round the sign.
constexpr float LED_BLINKS = 4.0f;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

const Look& lookOf(Vend kind) { return LOOKS[size_t(kind)]; }

/// The colour a glyph letter stands for on this machine, the LEDs taking turns by blink.
Rgba ink(const Look& look, char g, int gx, int gy, int blink) {
    switch (g) {
    case 'K': return TRIM;
    case 'o': return (gx + gy + blink) % 2 ? look.ledB : look.ledA;
    case 'L':
    case 'm': return SIGN;
    case 'B': return look.body;
    case 'b': return look.shade;
    case 'G': return GLASS;
    case 'g': return GLINT;
    case 'P': return PANEL;
    case 'd': return SCREEN;
    case 'p': return BUTTON;
    case 'y': return COIN_SLOT;
    case 'D': return DROP;
    case 'c': return CHERRY;
    case 'l': return FROSTING;
    case 's': return SPONGE;
    case 'a': return CAP;
    case 'A': return CAP_SHADE;
    case 'q': return BOTTLE_GLASS;
    case 'Q': return BOTTLE_GLINT;
    case 'v': return BOTTLE_DEEP;
    default: return gunInk(g);
    }
}

/// The letter of a picture w wide and h tall placed at x, y, or 0 off it.
char inPicture(const char* const* pic, int w, int h, int x, int y, int gx, int gy) {
    int dx = gx - x, dy = gy - y;
    return dx >= 0 && dy >= 0 && dx < w && dy < h ? pic[dy][dx] : 0;
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

int vendingSlots(Vend kind) { return lookOf(kind).cols * lookOf(kind).rows; }

uint8_t vendingFull(Vend kind) { return uint8_t((1u << vendingSlots(kind)) - 1); }

Rgba vendingPixel(Vend kind, int gx, int gy, uint8_t stock, int blink) {
    const Look& look = lookOf(kind);
    for (int row = 0; row < look.rows; row++)
        for (int col = 0; col < look.cols; col++) {
            if (!(stock >> (row * look.cols + col) & 1)) continue;
            char g = inPicture(look.item, look.itemW, look.itemH, look.slotX[col], look.slotY[row], gx, gy);
            if (g && g != '.') return ink(look, g, gx, gy, blink);
        }
    char g = FRONT_GLYPH[gy][gx];
    if (g == 'S') {
        char s = inPicture(look.sign, 10, 3, SIGN_X, SIGN_Y, gx, gy);
        return ink(look, s && s != '.' ? s : 'L', gx, gy, blink);
    }
    for (int r = 0; r < look.rows; r++)
        if (gy == look.coil[r] && gx >= GLASS_LEFT && gx <= GLASS_RIGHT) return COIL;
    return ink(look, g, gx, gy, blink);
}

Rgba vendingSidePixel(Vend kind, int gx, int gy, int blink) {
    const Look& look = lookOf(kind);
    char g = SIDE_GLYPH[gy][gx];
    if (g == 'E') {
        char d = inPicture(look.deco, 7, 6, DECO_X, DECO_Y, gx, gy);
        g = d && d != '.' ? d : 'b';
    }
    return ink(look, g, gx, gy, blink);
}

void drawVendingMachine(Vend kind, int x, int y, uint8_t stock, bool lit, float seconds) {
    int blink = blinkAt(lit, seconds);
    drawGlyph(x, y, VENDING_W, lit, [&](int gx, int gy) { return vendingPixel(kind, gx, gy, stock, blink); });
}

void drawVendingMachineSide(Vend kind, int x, int y, bool lit, float seconds) {
    int blink = blinkAt(lit, seconds);
    drawGlyph(x, y, VENDING_DEPTH, lit, [&](int gx, int gy) { return vendingSidePixel(kind, gx, gy, blink); });
}
