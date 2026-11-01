#include "render/gun_art.h"
#include "render/sprites.h"
#include "core/vec.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace {

/// The gun in the hand, aimed ahead, up ahead and south, from
/// data/sprites.txt; in each, the pixel the hand is on.
constexpr int HELD_W = 4, HELD_H = 2, RAISED_W = 3, RAISED_H = 4, TOWARD_W = 2, TOWARD_H = 2;
constexpr Vec2i HELD_HAND{0, 1}, RAISED_HAND{0, 3}, TOWARD_HAND{2, 0};

/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

/// The ammo box and a casing, from data/sprites.txt.
constexpr int AMMO_W = 6, AMMO_H = 6, CASING_W = 2, CASING_H = 1;
/// Where each spent casing on a cell lies, its first pixel and whether it
/// lies across (down the cell) rather than along, in the order they come.
struct CasingSpot {
    int x, y;
    bool across;
};
/// The first few keep clear of each other, so a few casings read as so many.
constexpr CasingSpot CASING_SPOTS[CASINGS_SHOWN] = {{1, 1, false}, {4, 3, true}, {1, 4, false}, {4, 0, false},
                                                    {0, 2, true},  {2, 2, false}, {3, 5, false}, {5, 1, true}};
/// A casing thrown out: seconds it flies before it lands, and the hop it
/// bounces; sprite pixels it goes out to the side landing, and the hop
/// further; how far it rises over the gun first, and how high it hops.
constexpr float CASING_FLIGHT = 0.3f, CASING_BOUNCE = 0.12f;
constexpr float CASING_OUT = 7.0f, CASING_HOP_OUT = 2.0f;
constexpr float CASING_RISE = 3.0f, CASING_HOP = 1.5f;
/// Flying, it turns a quarter this often.
constexpr float CASING_SPIN = 0.05f;

Color shaded(Rgba c, float k, Color tint) {
    return {uint8_t(c.r * k * tint.r / 255), uint8_t(c.g * k * tint.g / 255), uint8_t(c.b * k * tint.b / 255),
            uint8_t(c.a * tint.a / 255)};
}

/// Draws a casing with its first pixel at canvas pixel x, y, lying across
/// (down) or along, scale screen pixels to a pixel.
void drawCasing(float x, float y, bool across, int scale, float k, Color tint) {
    const Sprite& casing = sprites::get("casing", CASING_W, CASING_H);
    for (int i = 0; i < CASING_W; i++) {
        Rgba c = casing.pixel(i, 0);
        if (!c.solid()) continue;
        float px = x + (across ? 0 : i * scale), py = y + (across ? i * scale : 0);
        DrawRectangle(int(px), int(py), scale, scale, shaded(c, k, tint));
    }
}

/// Where a thrown casing is t seconds after the shot, in sprite pixels:
/// how far out along its side over the floor, and how high above it.
void casingFlight(float drop, float t, float& out, float& high) {
    if (t < CASING_FLIGHT) {
        float u = t / CASING_FLIGHT;
        out = CASING_OUT * u;
        high = drop * (1.0f - u) + 4.0f * CASING_RISE * u * (1.0f - u);
    } else if (t < CASING_FLIGHT + CASING_BOUNCE) {
        float v = (t - CASING_FLIGHT) / CASING_BOUNCE;
        out = CASING_OUT + CASING_HOP_OUT * v;
        high = 4.0f * CASING_HOP * v * (1.0f - v);
    } else {
        out = CASING_OUT + CASING_HOP_OUT;
        high = 0.0f;
    }
}

} // namespace

Rgba gunPixel(int gx, int gy) { return sprites::get("gun", GUN_W, GUN_H).pixel(gx, gy); }

void drawGun(int x, int y, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int gy = 0; gy < GUN_H; gy++)
        for (int gx = 0; gx < GUN_W; gx++) {
            Rgba c = gunPixel(gx, gy);
            if (c.solid()) DrawPixel(x + gx, y + gy, {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a});
        }
}

void drawAmmoBox(int x, int y, bool opened, float left, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    const Sprite& box = sprites::get(opened ? "ammo box open" : "ammo box", AMMO_W, AMMO_H);
    // The rounds still in it, counted in reading order; the last taken first.
    int shown = int(std::ceil(std::clamp(left, 0.0f, 1.0f) * AMMO_SLOTS)), slot = 0;
    for (int gy = 0; gy < AMMO_H; gy++)
        for (int gx = 0; gx < AMMO_W; gx++) {
            char l = box.letter(gx, gy);
            Rgba c = box.pixel(gx, gy);
            if (opened && (l == 'b' || l == 'c') && slot++ >= shown) c = box.ink('i');
            if (c.solid()) DrawPixel(x + gx, y + gy, shaded(c, k, WHITE));
        }
}

void drawCasings(int x, int y, int count, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    for (int i = 0; i < std::min(count, CASINGS_SHOWN); i++) {
        const CasingSpot& s = CASING_SPOTS[i];
        drawCasing(float(x + s.x), float(y + s.y), s.across, 1, k, WHITE);
    }
}

void drawEjectedCasing(Vector2 port, Vector2 side, float drop, float t, int scale, Color tint) {
    if (t < 0 || t >= CASING_TIME) return;
    float out, high;
    casingFlight(drop, t, out, high);
    // Over the floor under the port, out along its side, up off the floor.
    float x = port.x + std::round(side.x * out) * scale;
    float y = port.y + std::round(drop + side.y * out - high) * scale;
    // Tumbling as it flies; down, it lies the way it went.
    bool flying = t < CASING_FLIGHT + CASING_BOUNCE;
    bool across = flying ? int(t / CASING_SPIN) % 2 == 1 : std::abs(side.y) > std::abs(side.x);
    drawCasing(x, y, across, scale, 1.0f, tint);
}

Vector2 ejectedCasingRest(Vector2 port, Vector2 side, float drop, int scale) {
    float out, high;
    casingFlight(drop, CASING_TIME, out, high);
    return {port.x + std::round(side.x * out) * scale, port.y + std::round(drop + side.y * out) * scale};
}

void drawHeldGun(Vector2 origin, GunSpot at, bool flip, int scale, Color tint) {
    if (!at.valid()) return;
    // Going off, the kick tips the muzzle up a step, the way the slide
    // faces; aimed south, the hand's kick shows it.
    int aim = at.fire && at.aim < 2 ? (at.aim == -2 ? -1 : at.aim - 1) : at.aim;
    // Aimed ahead or up, the gun held ahead; on the slant, held raised;
    // south, end on. The slant turns a quarter to aim down ahead, and the
    // one held ahead a quarter to aim up, mirrored so its slide faces ahead.
    bool slant = aim % 2 != 0;
    const Sprite& gun = aim == 2 ? sprites::get("gun held toward", TOWARD_W, TOWARD_H)
                        : slant  ? sprites::get("gun held raised", RAISED_W, RAISED_H)
                                 : sprites::get("gun held", HELD_W, HELD_H);
    Vec2i hand = aim == 2 ? TOWARD_HAND : slant ? RAISED_HAND : HELD_HAND;
    auto turn = [aim](Vec2i r) {
        return aim == 1 ? Vec2i{-r.y, r.x} : aim == -2 ? Vec2i{-r.y, -r.x} : r;
    };
    auto put = [&](Vec2i r, Rgba c) {
        if (!c.solid()) return;
        Vec2i t = turn(r);
        int x = at.x + t.x, y = at.y + t.y;
        if (flip) x = SPRITE_W - 1 - x;
        DrawRectangle(int(origin.x) + x * scale, int(origin.y) + y * scale, scale, scale,
                      {uint8_t(c.r * tint.r / 255), uint8_t(c.g * tint.g / 255), uint8_t(c.b * tint.b / 255),
                       uint8_t(c.a * tint.a / 255)});
    };
    for (int gy = 0; gy < gun.h; gy++)
        for (int gx = 0; gx < gun.w; gx++) put({gx - hand.x, gy - hand.y}, gun.pixel(gx, gy));
}
