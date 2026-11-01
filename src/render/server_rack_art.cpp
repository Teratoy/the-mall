#include "render/server_rack_art.h"
#include <cstdint>
#include <raylib.h>

namespace {

/// Seen from the front, top to bottom. K the cabinet's steel frame, v its
/// vent. The tape drive: T its putty face, W the smoked window, R the tape
/// wound on each reel, h the reel's hub, which turns, H the head and t the
/// tape running past it, l its two lamps. The status bank: P its bezel, L
/// the lights. The two drives: D their faces, d the slots, a the busy
/// lights. The console: C the screen's bezel, S the screen, n its knobs,
/// k the keyboard. The power unit: B its face, r the big switch, o its
/// lamp. F the plinth, f its feet.
constexpr const char* FRONT_GLYPH[SERVER_RACK_H] = {
    "KKKKKKKKKKKK",
    "KvvvvvvvvvvK",
    "KTTTTTTTTTTK",
    "KWRRWWWWRRWK",
    "KRhhRWWRhhRK",
    "KRhhRHHRhhRK",
    "KWRRWttWRRWK",
    "KTlTlTTTTTTK",
    "KKKKKKKKKKKK",
    "KPPPPPPPPPPK",
    "KPLLLLLLLLPK",
    "KPLLLLLLLLPK",
    "KPPPPPPPPPPK",
    "KKKKKKKKKKKK",
    "KDddddddDaDK",
    "KDDDDDDDDDDK",
    "KDddddddDaDK",
    "KKKKKKKKKKKK",
    "KCCCCCCCCCCK",
    "KCSSSSSSCnCK",
    "KCSSSSSSCnCK",
    "KCCCCCCCCCCK",
    "KkkkkkkkkkkK",
    "KBBBBBBorBBK",
    "FfFFFFFFFFfF",
};
static_assert(sizeof FRONT_GLYPH / sizeof FRONT_GLYPH[0] == SERVER_RACK_H);

/// Seen from its right, the front's frame on the left: the steel flank with
/// two banks of louvres in u, the maker's plate in p with its writing in q,
/// the back post in E.
constexpr const char* SIDE_GLYPH[SERVER_RACK_H] = {
    "KKKKKKKKKK",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "KAApppppAE",
    "KAApqqqpAE",
    "KAApppppAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAuuuuuuAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "KAAAAAAAAE",
    "FfFFFFFFfF",
};
static_assert(sizeof SIDE_GLYPH / sizeof SIDE_GLYPH[0] == SERVER_RACK_H);
/// Where each reel's hub has its top-left, and where the screen has its top-left and its size.
constexpr int HUB_X[2] = {2, 8}, HUB_Y = 4;
constexpr int SCREEN_X = 2, SCREEN_Y = 19, SCREEN_W = 6, SCREEN_H = 2;

constexpr Rgba FRAME = hex(0x1c1e24);
constexpr Rgba VENT_SLOT = hex(0x0c0d10);
constexpr Rgba VENT_BAR = hex(0x3a3e48);
constexpr Rgba PUTTY = hex(0xc8bc9c);
constexpr Rgba PUTTY_SHADE = hex(0xa89c7e);
constexpr Rgba WINDOW = hex(0x1a2230);
constexpr Rgba TAPE = hex(0x5a3a28);
constexpr Rgba HUB = hex(0xb8bcc4);
constexpr Rgba HUB_SPOKE = hex(0x4a4e58);
constexpr Rgba HEAD = hex(0x8a8e96);
constexpr Rgba TAPE_RUN = hex(0x7a5236);
constexpr Rgba BEZEL = hex(0x3a3e48);
constexpr Rgba SLOT = hex(0x24221e);
constexpr Rgba KNOB = hex(0x2a2c30);
constexpr Rgba PHOSPHOR_DARK = hex(0x0c2a14);
constexpr Rgba PHOSPHOR = hex(0x6aff8a);
constexpr Rgba KEY = hex(0xddd4bc);
constexpr Rgba KEY_GAP = hex(0x7a7262);
constexpr Rgba POWER_FACE = hex(0x2c3038);
constexpr Rgba SWITCH = hex(0xd03030);
constexpr Rgba PLINTH = hex(0x101114);
constexpr Rgba FOOT = hex(0x34363c);
constexpr Rgba FLANK = hex(0x2c3038);
constexpr Rgba LOUVRE = hex(0x121418);
constexpr Rgba BACK_POST = hex(0x16181c);
constexpr Rgba PLATE = hex(0xa8acb4);
constexpr Rgba PLATE_TEXT = hex(0x3a3e48);
/// The status lights, each lit and dark: most green, some amber, a few red.
constexpr Rgba GREEN_ON = hex(0x5cff6a), GREEN_OFF = hex(0x163020);
constexpr Rgba AMBER_ON = hex(0xffb030), AMBER_OFF = hex(0x3a2810);
constexpr Rgba RED_ON = hex(0xff4040), RED_OFF = hex(0x3a1214);
/// The rack's clock runs this many ticks a second; everything that moves moves on its ticks.
constexpr float TICKS = 8.0f;
/// The reels turn a quarter every this many ticks, and spin or rest in bursts this many ticks long.
constexpr int REEL_STEP = 2, REEL_BURST = 16;
/// A status light holds for between LIGHT_MIN and LIGHT_MIN + LIGHT_SPREAD - 1 ticks.
constexpr int LIGHT_MIN = 2, LIGHT_SPREAD = 6;
/// The screen scrolls a line every this many ticks, and its cursor blinks every this many.
constexpr int SCREEN_SCROLL = 12, CURSOR_BLINK = 4;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

/// A well-stirred number from a, b and c, so each light keeps its own colour and pace.
uint32_t stir(int a, int b, int c) {
    uint32_t h = uint32_t(a) * 0x9e3779b1u ^ uint32_t(b) * 0x85ebca77u ^ uint32_t(c) * 0xc2b2ae3du;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

/// A status light: its colour fixed by where it sits, lit or dark for spells of its own length.
Rgba statusLight(int gx, int gy, int tick) {
    uint32_t who = stir(gx, gy, 0);
    int hold = LIGHT_MIN + int(who % LIGHT_SPREAD);
    bool on = stir(gx, gy, (tick + int(who >> 8) % hold) / hold + 1) % 5 < 2;
    switch (who >> 16 & 7) {
    case 6: return on ? AMBER_ON : AMBER_OFF;
    case 7: return on ? RED_ON : RED_OFF;
    default: return on ? GREEN_ON : GREEN_OFF;
    }
}

/// A reel's hub, a spoke going round it clockwise while the tape runs.
Rgba hub(int gx, int gy, int tick) {
    int reel = gx >= HUB_X[1] ? 1 : 0;
    bool running = stir(tick / REEL_BURST, 0, 1) % 3 != 0;
    int turn = running ? tick / REEL_STEP % 4 : 0;
    /// The spoke's four places, clockwise from the top-left.
    constexpr int SPOKE_X[4] = {0, 1, 1, 0}, SPOKE_Y[4] = {0, 0, 1, 1};
    int dx = gx - HUB_X[reel], dy = gy - HUB_Y;
    return dx == SPOKE_X[turn] && dy == SPOKE_Y[turn] ? HUB_SPOKE : HUB;
}

/// The console's screen: lines of text scrolling up, the last one still being typed by a blinking cursor.
Rgba screen(int gx, int gy, int tick) {
    int sx = gx - SCREEN_X, sy = gy - SCREEN_Y;
    int line = tick / SCREEN_SCROLL + sy;
    int len = 2 + int(stir(line, 7, 2) % (SCREEN_W - 2));
    bool last = sy == SCREEN_H - 1;
    if (last && sx == len) return tick / CURSOR_BLINK % 2 ? PHOSPHOR : PHOSPHOR_DARK;
    bool ink = sx < len && stir(sx, line, 3) % 4 != 0;
    return ink ? PHOSPHOR : PHOSPHOR_DARK;
}

/// The colour a front glyph letter stands for at gx, gy.
Rgba ink(char g, int gx, int gy, int tick) {
    switch (g) {
    case 'K': return FRAME;
    case 'v': return gx % 2 ? VENT_BAR : VENT_SLOT;
    case 'T':
    case 'C':
    case 'D': return PUTTY;
    case 'W': return WINDOW;
    case 'R': return TAPE;
    case 'h': return hub(gx, gy, tick);
    case 'H': return HEAD;
    case 't': return TAPE_RUN;
    case 'l': return tick / (2 * REEL_BURST) % 2 && gx > 2 ? AMBER_ON : GREEN_ON;
    case 'P': return BEZEL;
    case 'L': return statusLight(gx, gy, tick);
    case 'd': return SLOT;
    case 'a': return stir(gx, tick, 4) % 3 ? GREEN_OFF : GREEN_ON;
    case 'S': return screen(gx, gy, tick);
    case 'n': return KNOB;
    case 'k': return gx % 2 ? KEY : KEY_GAP;
    case 'B': return POWER_FACE;
    case 'r': return SWITCH;
    case 'o': return AMBER_ON;
    case 'F': return PLINTH;
    case 'f': return FOOT;
    default: return PUTTY_SHADE;
    }
}

/// Draws a glyph's pixels from px() with its bottom-left at x, y, dimmed unless in sight.
template <typename F> void drawGlyph(int x, int y, int w, bool inSight, F px) {
    float k = inSight ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < SERVER_RACK_H; gy++)
        for (int gx = 0; gx < w; gx++) {
            Rgba c = px(gx, gy);
            if (!c.solid()) continue;
            DrawPixel(x + gx, y - SERVER_RACK_H + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}

} // namespace

Rgba serverRackPixel(int gx, int gy, int tick) { return ink(FRONT_GLYPH[gy][gx], gx, gy, tick); }

Rgba serverRackSidePixel(int gx, int gy) {
    switch (SIDE_GLYPH[gy][gx]) {
    case 'K': return FRAME;
    case 'A': return FLANK;
    case 'u': return LOUVRE;
    case 'E': return BACK_POST;
    case 'p': return PLATE;
    case 'q': return PLATE_TEXT;
    case 'F': return PLINTH;
    default: return FOOT;
    }
}

void drawServerRack(int x, int y, bool inSight, float seconds) {
    int tick = inSight ? int(seconds * TICKS) : 0;
    drawGlyph(x, y, SERVER_RACK_W, inSight, [&](int gx, int gy) { return serverRackPixel(gx, gy, tick); });
}

void drawServerRackSide(int x, int y, bool inSight) {
    drawGlyph(x, y, SERVER_RACK_DEPTH, inSight, [&](int gx, int gy) { return serverRackSidePixel(gx, gy); });
}
