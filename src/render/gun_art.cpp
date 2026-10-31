#include "render/gun_art.h"
#include "render/sprites.h"
#include "core/vec.h"
#include <raylib.h>

namespace {

/// The gun in the hand, aimed ahead and aimed up ahead, from data/sprites.txt;
/// in each, the pixel the hand is on, and the middle of the muzzle flash.
constexpr int HELD_W = 4, HELD_H = 2, RAISED_W = 4, RAISED_H = 5;
constexpr Vec2i HELD_HAND{0, 1}, RAISED_HAND{0, 4};
constexpr Vec2i HELD_FLASH{5, 0}, RAISED_FLASH{5, -2};
constexpr int FLASH_SIZE = 3;

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
    // Aimed straight, the gun held ahead; on the slant, held raised. Each
    // turns a quarter to aim down, and the one held ahead the other quarter
    // to aim up, mirrored so its slide still faces ahead.
    bool slant = at.aim % 2 != 0;
    const Sprite& gun = slant ? sprites::get("gun held raised", RAISED_W, RAISED_H)
                              : sprites::get("gun held", HELD_W, HELD_H);
    Vec2i hand = slant ? RAISED_HAND : HELD_HAND;
    auto turn = [aim = at.aim](Vec2i r) {
        return aim > 0 ? Vec2i{-r.y, r.x} : aim == -2 ? Vec2i{-r.y, -r.x} : r;
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
    if (!at.fire) return;
    const Sprite& flash = sprites::get("muzzle flash", FLASH_SIZE, FLASH_SIZE);
    Vec2i mid = (slant ? RAISED_FLASH : HELD_FLASH) - hand;
    for (int fy = 0; fy < FLASH_SIZE; fy++)
        for (int fx = 0; fx < FLASH_SIZE; fx++)
            put({mid.x + fx - FLASH_SIZE / 2, mid.y + fy - FLASH_SIZE / 2}, flash.pixel(fx, fy));
}
