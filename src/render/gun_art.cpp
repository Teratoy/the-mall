#include "render/gun_art.h"
#include "render/sprites.h"
#include <raylib.h>

namespace {

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
