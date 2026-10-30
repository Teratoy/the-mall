#include "render/mess_art.h"
#include "render/water_art.h"
#include "world/mess.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <raylib.h>

namespace {

/// Piss: wet, its darker rim, the glint on it; and dry, a faint stain.
constexpr Color PISS_WET = {218, 192, 56, 150};
constexpr Color PISS_RIM = {176, 146, 38, 175};
constexpr Color PISS_GLINT = {250, 244, 186, 210};
constexpr Color PISS_DRY = {150, 132, 72, 60};
/// Spreads to its full size over this long; half axes in pixels.
constexpr float PISS_SPREAD_TIME = 2.5f;
/// Poured water: as thick as this while wet, fading to nothing as it dries,
/// spreading wider than piss by this much for as much water.
constexpr float PUDDLE_ALPHA = 200.0f;
constexpr float PUDDLE_WIDER = 1.3f;
constexpr float PISS_RADIUS_MIN = 2.5f, PISS_RADIUS_PER_SIZE = 2.5f, PISS_FLAT = 0.55f;
/// Feces, fresh: the dark edge, the body, the lighter coils, the wet shine
/// and the damp round it; dry, all darker and greyer, with no shine.
constexpr Color FECES_EDGE = {42, 24, 11, 255};
constexpr Color FECES_BODY = {84, 50, 22, 255};
constexpr Color FECES_COIL = {106, 66, 32, 255};
constexpr Color FECES_SHINE = {152, 108, 64, 255};
constexpr Color FECES_DAMP = {60, 36, 16, 90};
constexpr Color FECES_DRY_EDGE = {24, 19, 14, 255};
constexpr Color FECES_DRY_BODY = {42, 33, 25, 255};
/// Water fouled through: murky brown over the blue.
constexpr Color WATER_FOUL = {92, 70, 34, 170};
constexpr Color FECES_DRY_COIL = {52, 41, 30, 255};

uint32_t hash(uint32_t seed, int a, int b) {
    uint32_t h = seed ^ uint32_t(a) * 0x9E3779B1u ^ uint32_t(b) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

float unit(uint32_t h) { return float(h & 0xFFFF) / 65535.0f; }

Color mix(Color dry, Color wet, float fresh, float brightness) {
    auto m = [&](unsigned char a, unsigned char b) { return (unsigned char)((a + (b - a) * fresh) * brightness); };
    return {m(dry.r, wet.r), m(dry.g, wet.g), m(dry.b, wet.b), (unsigned char)(dry.a + (wet.a - dry.a) * fresh)};
}

/// While one is drawn: which world pixels may show mess (floor, not wall),
/// and the shift from world pixels to where they are drawn, to tell which
/// world pixel a dot lands on.
const std::function<bool(int, int)>* shows = nullptr;
int shiftX = 0, shiftY = 0;

void dot(int x, int y, Color c) {
    if (shows && !(*shows)(x - shiftX, y - shiftY)) return;
    DrawPixel(x, y, c);
}

void drawPiss(const Dropping& d, float fresh, float age, int ox, int oy, float brightness) {
    bool water = d.kind == MessKind::Water;
    float grow = 0.3f + 0.7f * std::clamp(age / PISS_SPREAD_TIME, 0.0f, 1.0f);
    float rx = (PISS_RADIUS_MIN + PISS_RADIUS_PER_SIZE * d.size) * grow * (water ? PUDDLE_WIDER : 1.0f), ry = rx * PISS_FLAT;
    int x0 = int(std::floor(d.x - rx)), x1 = int(std::ceil(d.x + rx));
    int y0 = int(std::floor(d.y - ry)), y1 = int(std::ceil(d.y + ry));
    Color body = mix(PISS_DRY, PISS_WET, fresh, brightness);
    Color rim = mix(PISS_DRY, PISS_RIM, fresh, brightness);
    // Poured water is the water it was, and dries away to nothing.
    if (water) {
        if (fresh <= 0.0f) return;
        Color w = waterColor(d.red, d.foul);
        body = {(unsigned char)(w.r * brightness), (unsigned char)(w.g * brightness), (unsigned char)(w.b * brightness),
                (unsigned char)(PUDDLE_ALPHA * fresh)};
        rim = body;
        rim.a = (unsigned char)(body.a * 0.7f);
    }
    for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
            float dx = (px + 0.5f - d.x) / rx, dy = (py + 0.5f - d.y) / ry;
            float r = dx * dx + dy * dy;
            // A ragged edge, its own every time.
            if (r > 1.0f - 0.35f * unit(hash(d.seed, px, py))) continue;
            dot(px + ox, py + oy, r > 0.55f ? rim : body);
        }
    if (fresh > 0.5f) {
        int gx = int(std::floor(d.x - rx * 0.3f)), gy = int(std::floor(d.y - ry * 0.3f));
        Color g = water ? mix(Color{WATER_INK.r, WATER_INK.g, WATER_INK.b, 0}, WATER_INK, fresh, brightness)
                        : mix(PISS_DRY, PISS_GLINT, fresh, brightness);
        dot(gx + ox, gy + oy, g);
        if (d.size > 0.6f) dot(gx + 1 + ox, gy + oy, g);
    }
}

// A coil heaped in rows, widest at the bottom, each row shifted a pixel or
// so either way, a dark edge round it, flecks and a smear about its foot.
void drawFeces(const Dropping& d, float fresh, int ox, int oy, float brightness) {
    Color edge = mix(FECES_DRY_EDGE, FECES_EDGE, fresh, brightness);
    Color body = mix(FECES_DRY_BODY, FECES_BODY, fresh, brightness);
    Color coil = mix(FECES_DRY_COIL, FECES_COIL, fresh, brightness);
    int base = int(std::lround(3.5f + 2.0f * d.size));
    // A small one is a plain mound, centred, with no room for coils to
    // lean or a smear to drag out, or it reads as something else.
    bool small = d.size <= 0.8f;
    int rows = small ? 3 : 4;
    int cx = int(std::floor(d.x)), by = int(std::floor(d.y));
    // Damp round its foot while fresh.
    if (fresh > 0.3f) {
        Color damp = FECES_DAMP;
        damp.a = (unsigned char)(damp.a * fresh);
        int reach = base / 2 + (small ? 0 : 2);
        for (int px = -reach; px <= reach; px++) dot(cx + px + ox, by + 1 + oy, damp);
    }
    // A smear dragged out one side, and flecks.
    int side = hash(d.seed, 1, 0) & 1 ? 1 : -1;
    int smear = small ? int(hash(d.seed, 2, 0) % 2) : 1 + int(hash(d.seed, 2, 0) % 3);
    for (int i = 1; i <= smear; i++) dot(cx + side * (base / 2 + i) + ox, by + oy, i == smear ? edge : body);
    int flecks = 2 + int(hash(d.seed, 3, 0) % 3);
    for (int i = 0; i < flecks; i++) {
        uint32_t h = hash(d.seed, 4, i);
        int fx = cx + int(h % 9) - 4, fy = by - int((h >> 8) % 3) + 1;
        if (std::abs(fx - cx) <= base / 2) fx += fx < cx ? -base / 2 : base / 2;
        dot(fx + ox, fy + oy, i % 2 ? body : edge);
    }
    int shift = 0;
    for (int row = 0; row < rows; row++) {
        int w = std::max(1, base - row * 2 + (row == 0 || small ? 0 : 1));
        if (row == rows - 1 && !small) w = std::max(1, std::min(w, 2));
        if (row > 0 && !small) shift += int(hash(d.seed, 5, row) % 3) - 1;
        int left = cx - w / 2 + shift, y = by - row;
        for (int i = 0; i < w; i++) {
            bool rim = i == 0 || i == w - 1 || row == 0;
            Color c = rim ? edge : (i + row) % 3 == 0 ? coil : body;
            dot(left + i + ox, y + oy, c);
        }
        // Wet shine on the upper left of each coil.
        if (fresh > 0.4f && w >= 3) dot(left + 1 + ox, y + oy, mix(coil, FECES_SHINE, fresh, brightness));
    }
}

// A footprint: a couple of pixels of the sole, fainter the more steps
// since it stepped in it.
void drawPrint(const Dropping& d, float fresh, int ox, int oy, float brightness) {
    Color c = mix(FECES_DRY_BODY, FECES_BODY, fresh, brightness);
    c.a = (unsigned char)(80 + 150 * std::min(1.0f, d.size));
    int x = int(std::floor(d.x)), y = int(std::floor(d.y));
    dot(x + ox, y + oy, c);
    dot(x + 1 + ox, y + oy, c);
    if (d.size > 0.5f) dot(x + (hash(d.seed, 6, 0) & 1) + ox, y + 1 + oy, c);
}

} // namespace

Color fecesColor(float fresh) { return mix(FECES_DRY_BODY, FECES_BODY, fresh, 1.0f); }

Color pissColor(float fresh) { return mix(PISS_DRY, PISS_RIM, fresh, 1.0f); }

Color waterFoulCloud(float foul) {
    Color c = WATER_FOUL;
    c.a = (unsigned char)(std::min(1.0f, foul) * WATER_FOUL.a);
    return c;
}

void drawWaterFoul(int x, int y, float foul) { DrawRectangle(x, y, 6, 6, waterFoulCloud(foul)); }

void drawDropping(const Dropping& d, float fresh, float age, int left, int top, float brightness,
                  const std::function<bool(int, int)>& onFloor) {
    shows = &onFloor;
    shiftX = -left;
    shiftY = -top;
    if (d.kind == MessKind::Piss || d.kind == MessKind::Water)
        drawPiss(d, fresh, age, -left, -top, brightness);
    else if (d.kind == MessKind::Print)
        drawPrint(d, fresh, -left, -top, brightness);
    else
        drawFeces(d, fresh, -left, -top, brightness);
    shows = nullptr;
}
