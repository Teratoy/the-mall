#include "render/computer_art.h"
#include <cmath>
#include <cstdint>
#include <raylib.h>

namespace {

/// The terminal seen from the front. C its putty case, c the case's shaded
/// side, S the screen, o the power light, k the keyboard on its tray, j the
/// tray's lip, P the stalk, p its shaded side, F the foot.
constexpr const char* TERMINAL_GLYPH[TERMINAL_H] = {
    ".CCCC.",
    "CSSSSc",
    "CSSSSc",
    "CSSSSc",
    "CCCoCc",
    "kkkkkk",
    "jjjjjj",
    "..Pp..",
    "..Pp..",
    "..Pp..",
    "..Pp..",
    ".PPPp.",
    "FFFFFF",
};
static_assert(sizeof TERMINAL_GLYPH / sizeof TERMINAL_GLYPH[0] == TERMINAL_H);

/// The console desk, the screens and the instrument box on top and its
/// footprint from row CONSOLE_RISE down: a desk along the wall with a wing
/// coming forward each side, its inner edges slanting in towards the chair.
/// The screens as the terminal's, d the side of the small one, turned in,
/// n their necks. V the instrument box, v its shade, R its round scope with
/// the sweep going round, r the scope's rim. T the desk's top, t its shade
/// against the wall, h its lit front rim, E its front, e the drawer
/// handles, K the dark under it where the knees go. On the top: P the
/// switch panels, l their lamps, x their toggles, k the keyboard set
/// askew and j its lip, M the mug and m the coffee, G the printout curling
/// out of the printer and g its green bars, the printer in the screens'
/// colours with its slot in K, F a floppy and f its label, Q the phone and
/// q its dial, W loose sheets. The chair, turned a little to the left: s
/// its seat, B its back, b the back's shade, a the arms, n the stem, w the
/// wheels. L a cable falling off the left wing and trailing on the floor.
constexpr const char* CONSOLE_GLYPH[CONSOLE_H] = {
    ".............CCCCCCCCC.....VVVVVV...",
    "............CCCCCCCCCCC...VVrRRrVv..",
    "...........CCSSSSSSSSSCc..VrRRRRrv..",
    "...........CCSSSSSSSSSCc..VrRRRRrv..",
    "..CCCCC....CCSSSSSSSSSCc..VVrRRrVv..",
    ".dCSSSSc...CCSSSSSSSSSCc..VVVVoVVv..",
    ".dCSSSSc...CCSSSSSSSSSCc...vvvvvv...",
    ".dCSSSSc...CCSSSSSSSSSCc.PPPPPPPPP..",
    ".dCCCoCc...CCCCCCCCoCCCc.PlPlPlPxP..",
    "..cccc......cccccccccc...PPPPPPPPP..",
    "...nn..........nnnn......PxPxPlPlP..",
    "tttttttttttttttttttttttttPPPPPPPPPtt",
    "tttttttttttttttttttttttttttttttttttt",
    "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT",
    "TTTTTTTTTTTTTTTTTTTTTTTTTTPPPPPPPFFT",
    "TTTTTTTTTMmMTTkkkkkkkkkkkTPxPxPxPFfT",
    "TTTGgGTTTMMMMTkkkkkkkkkkkTPPPPPPPTTT",
    "TTGgGgTTTMMMTkkkkkkkkkkkTTTTTTTTTTTT",
    "TTgGgGTTTTTTTjjjjjjjjjjjTTTTTTTTTTTT",
    "TCCCCCCCcTTTTTTTTTTTTTTTTTTTTTTTTTTT",
    "TCKKKKKCcTTTThhhhhhhhhhhTTTTTTTTTTTT",
    "TCCCCCoCcTTTTEEEEEEEEEEETTTTQQQTTWWT",
    "TccccccccTTTTKKKKKKKKKKKTTTQqqqQWWWT",
    "TTTTTTTTTTTT.....sss.....TTQQQQQWWWT",
    "TTTTTTTTTTTT....sssss.....TTTTTTWWTT",
    "TTTTTTTTTTT....aBBBBsa....hhhhhhhhhh",
    "hhhhhhhhhhh...aBBBBBBa....EEEEeeEEEE",
    "EEEEeeELEEE....BBBBBb.....EEEEEEEEEE",
    "EEEEEEELEEE....bbbbb................",
    ".......L..........n.................",
    "........L.......w.n.w..........WW...",
    "........L......w..w..w........WWW...",
    ".........LL...................WW....",
    "...........LL.......................",
    ".............L......................",
};
static_assert(sizeof CONSOLE_GLYPH / sizeof CONSOLE_GLYPH[0] == CONSOLE_H);
/// The scope's middle, in pixels from the glyph's top-left.
constexpr float SCOPE_X = 30.0f, SCOPE_Y = 3.0f;
/// Where the scope shows blips, in pixels from its middle, each lit as the sweep passes and fading after.
constexpr float BLIP_X[] = {-1.5f, 1.5f, 0.5f}, BLIP_Y[] = {-0.5f, 0.5f, -1.5f};

constexpr Rgba PUTTY = hex(0xc8bc9c);
constexpr Rgba PUTTY_SHADE = hex(0xa89c7e);
constexpr Rgba PUTTY_TURNED = hex(0x8c8066);
constexpr Rgba PHOSPHOR_DARK = hex(0x0c2a14);
constexpr Rgba PHOSPHOR = hex(0x6aff8a);
constexpr Rgba POWER = hex(0x5cff6a);
constexpr Rgba KEY = hex(0xddd4bc);
constexpr Rgba KEY_GAP = hex(0x7a7262);
constexpr Rgba STEEL = hex(0x50545c);
constexpr Rgba STEEL_SHADE = hex(0x3a3e46);
constexpr Rgba FOOT = hex(0x2a2c30);
constexpr Rgba DESK_TOP = hex(0x7c8088);
constexpr Rgba DESK_SHADE = hex(0x60646c);
constexpr Rgba DESK_RIM = hex(0xa0a4ac);
constexpr Rgba DESK_FRONT = hex(0x50545c);
constexpr Rgba HANDLE = hex(0xa0a4ac);
constexpr Rgba KNEEHOLE = hex(0x18191c);
constexpr Rgba PANEL = hex(0x3a3e48);
constexpr Rgba TOGGLE = hex(0xd0d4dc);
constexpr Rgba TOGGLE_DOWN = hex(0x80848c);
constexpr Rgba MUG = hex(0xe8e0d0);
constexpr Rgba COFFEE = hex(0x4a2a18);
constexpr Rgba PAPER = hex(0xf0ece0);
constexpr Rgba PAPER_BAR = hex(0xb8e0b0);
constexpr Rgba PAPER_EDGE = hex(0xc8c4b8);
constexpr Rgba INSTRUMENT = hex(0x5e6a5a);
constexpr Rgba INSTRUMENT_SHADE = hex(0x444e42);
constexpr Rgba SCOPE_RIM = hex(0x1e4a26);
constexpr Rgba PHOSPHOR_DIM = hex(0x2e8a44);
constexpr Rgba FLOPPY = hex(0x2a3450);
constexpr Rgba FLOPPY_LABEL = hex(0xe8e0d0);
constexpr Rgba PHONE = hex(0xc0302c);
constexpr Rgba PHONE_DIAL = hex(0x7a1c1a);
constexpr Rgba SHEET = hex(0xf4f2ec);
constexpr Rgba CABLE = hex(0x1e1e22);
constexpr Rgba SEAT = hex(0x3a4a6a);
constexpr Rgba SEAT_BACK = hex(0x2e3c58);
constexpr Rgba SEAT_SHADE = hex(0x222c40);
constexpr Rgba ARM = hex(0x222428);
constexpr Rgba WHEEL = hex(0x18191c);
/// The panels' lamps, each lit and dark: most green, some amber, a few red.
constexpr Rgba GREEN_ON = hex(0x5cff6a), GREEN_OFF = hex(0x163020);
constexpr Rgba AMBER_ON = hex(0xffb030), AMBER_OFF = hex(0x3a2810);
constexpr Rgba RED_ON = hex(0xff4040), RED_OFF = hex(0x3a1214);
/// The clock runs this many ticks a second, as the server rack's.
constexpr float TICKS = 8.0f;
/// A screen scrolls a line every this many ticks, and its cursor blinks every this many.
constexpr int SCREEN_SCROLL = 12, CURSOR_BLINK = 4;
/// A lamp holds for between LAMP_MIN and LAMP_MIN + LAMP_SPREAD - 1 ticks.
constexpr int LAMP_MIN = 3, LAMP_SPREAD = 6;
/// The sweep goes round once in this many ticks, and leaves a trail this many ticks long.
constexpr int SWEEP_TICKS = 16, SWEEP_TRAIL = 4;
/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

/// A well-stirred number from a, b and c, so each screen and lamp keeps its own pace.
uint32_t stir(int a, int b, int c) {
    uint32_t h = uint32_t(a) * 0x9e3779b1u ^ uint32_t(b) * 0x85ebca77u ^ uint32_t(c) * 0xc2b2ae3du;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

/// A screen w wide and h tall, at sx, sy of it: lines of text scrolling
/// up, the last still being typed by a blinking cursor. Each screen goes
/// by its own which, so no two say the same.
Rgba screen(int sx, int sy, int w, int h, int which, int tick) {
    int t = tick + int(stir(which, 5, 6) % SCREEN_SCROLL);
    int line = t / SCREEN_SCROLL + sy;
    int len = 1 + int(stir(line, which, 2) % uint32_t(w - 1));
    if (sy == h - 1 && sx == len) return t / CURSOR_BLINK % 2 ? PHOSPHOR : PHOSPHOR_DARK;
    bool ink = sx < len && stir(sx, line, which * 7 + 3) % 4 != 0;
    return ink ? PHOSPHOR : PHOSPHOR_DARK;
}

/// The screen pixel at gx, gy of a glyph rows tall, found by where its screen begins and ends.
Rgba screenAt(const char* const* glyph, int rows, int gx, int gy, int tick) {
    int left = gx, right = gx, top = gy, bottom = gy;
    while (left > 0 && glyph[gy][left - 1] == 'S') left--;
    while (glyph[gy][right + 1] == 'S') right++;
    while (top > 0 && glyph[top - 1][gx] == 'S') top--;
    while (bottom + 1 < rows && glyph[bottom + 1][gx] == 'S') bottom++;
    return screen(gx - left, gy - top, right - left + 1, bottom - top + 1, left * 31 + top, tick);
}

/// How many ticks ago the sweep last passed the angle of dx, dy from the scope's middle.
int sinceSweep(float dx, float dy, int tick) {
    constexpr float TURN = 6.2831853f;
    float a = std::atan2(dy, dx) / TURN;
    a -= std::floor(a);
    int at = int(a * SWEEP_TICKS);
    return ((tick - at) % SWEEP_TICKS + SWEEP_TICKS) % SWEEP_TICKS;
}

/// The scope: a sweep going round, its trail fading, and blips lighting up as it passes.
Rgba scope(int gx, int gy, int tick) {
    float dx = gx + 0.5f - SCOPE_X, dy = gy + 0.5f - SCOPE_Y;
    int since = sinceSweep(dx, dy, tick);
    for (size_t i = 0; i < sizeof BLIP_X / sizeof BLIP_X[0]; i++)
        if (dx == BLIP_X[i] && dy == BLIP_Y[i]) return since < SWEEP_TRAIL * 2 ? PHOSPHOR : PHOSPHOR_DARK;
    if (since == 0) return PHOSPHOR;
    return since < SWEEP_TRAIL ? PHOSPHOR_DIM : PHOSPHOR_DARK;
}

/// A panel's lamp: its colour fixed by where it sits, lit or dark for spells of its own length.
Rgba lamp(int gx, int gy, int tick) {
    uint32_t who = stir(gx, gy, 9);
    int hold = LAMP_MIN + int(who % LAMP_SPREAD);
    bool on = stir(gx, gy, (tick + int(who >> 8) % hold) / hold + 1) % 5 < 3;
    switch (who >> 16 & 7) {
    case 6: return on ? AMBER_ON : AMBER_OFF;
    case 7: return on ? RED_ON : RED_OFF;
    default: return on ? GREEN_ON : GREEN_OFF;
    }
}

/// Draws a glyph's pixels from px() with its top-left at x, y, dimmed unless lit.
template <typename F> void drawGlyph(int x, int y, int w, int h, bool lit, F px) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < h; gy++)
        for (int gx = 0; gx < w; gx++) {
            Rgba c = px(gx, gy);
            if (!c.solid()) continue;
            DrawPixel(x + gx, y + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}

int tickAt(bool lit, float seconds) { return lit ? int(seconds * TICKS) : 0; }

} // namespace

Rgba terminalPixel(int gx, int gy, int tick) {
    switch (TERMINAL_GLYPH[gy][gx]) {
    case 'C': return PUTTY;
    case 'c': return PUTTY_SHADE;
    case 'S': return screenAt(TERMINAL_GLYPH, TERMINAL_H, gx, gy, tick);
    case 'o': return POWER;
    case 'k': return gx % 2 ? KEY_GAP : KEY;
    case 'j': return PUTTY_SHADE;
    case 'P': return STEEL;
    case 'p': return STEEL_SHADE;
    case 'F': return FOOT;
    default: return {};
    }
}

Rgba consoleDeskPixel(int gx, int gy, int tick) {
    switch (CONSOLE_GLYPH[gy][gx]) {
    case 'C': return PUTTY;
    case 'c': return PUTTY_SHADE;
    case 'd': return PUTTY_TURNED;
    case 'S': return screenAt(CONSOLE_GLYPH, CONSOLE_H, gx, gy, tick);
    case 'o': return POWER;
    case 'n': return gy < CONSOLE_RISE ? STEEL_SHADE : STEEL;
    case 'V': return INSTRUMENT;
    case 'v': return INSTRUMENT_SHADE;
    case 'R': return scope(gx, gy, tick);
    case 'r': return SCOPE_RIM;
    case 'F': return FLOPPY;
    case 'f': return FLOPPY_LABEL;
    case 'Q': return PHONE;
    case 'q': return PHONE_DIAL;
    case 'W': return SHEET;
    case 'L': return CABLE;
    case 'T': return DESK_TOP;
    case 't': return DESK_SHADE;
    case 'h': return DESK_RIM;
    case 'E': return DESK_FRONT;
    case 'e': return HANDLE;
    case 'K': return KNEEHOLE;
    case 'P': return PANEL;
    case 'l': return lamp(gx, gy, tick);
    case 'x': return stir(gx, 0, 8) % 3 ? TOGGLE : TOGGLE_DOWN;
    case 'k': return (gx + gy) % 2 ? KEY_GAP : KEY;
    case 'j': return PUTTY_SHADE;
    case 'M': return MUG;
    case 'm': return COFFEE;
    case 'G': return PAPER;
    case 'g': return PAPER_BAR;
    case 'p': return PAPER_EDGE;
    case 's': return SEAT;
    case 'B': return SEAT_BACK;
    case 'b': return SEAT_SHADE;
    case 'a': return ARM;
    case 'w': return WHEEL;
    default: return {};
    }
}

void drawTerminal(int x, int y, bool lit, float seconds) {
    int tick = tickAt(lit, seconds);
    drawGlyph(x, y - TERMINAL_H, TERMINAL_W, TERMINAL_H, lit,
              [&](int gx, int gy) { return terminalPixel(gx, gy, tick); });
}

void drawConsoleDesk(int x, int y, bool lit, float seconds) {
    int tick = tickAt(lit, seconds);
    drawGlyph(x, y - CONSOLE_RISE, CONSOLE_W, CONSOLE_H, lit,
              [&](int gx, int gy) { return consoleDeskPixel(gx, gy, tick); });
}
