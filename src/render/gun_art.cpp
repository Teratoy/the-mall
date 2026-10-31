#include "render/gun_art.h"
#include "render/sprites.h"
#include "core/vec.h"
#include <raylib.h>

namespace {

/// The gun in the hand, aimed ahead, up ahead and south, from
/// data/sprites.txt; in each, the pixel the hand is on.
constexpr int HELD_W = 4, HELD_H = 2, RAISED_W = 3, RAISED_H = 4, TOWARD_W = 2, TOWARD_H = 2;
constexpr Vec2i HELD_HAND{0, 1}, RAISED_HAND{0, 3}, TOWARD_HAND{2, 0};

/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;

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
