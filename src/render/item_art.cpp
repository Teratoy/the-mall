#include "render/item_art.h"
#include "render/sprites.h"
#include "world/chunk.h"
#include <raylib.h>

namespace {

/// Remembered but out of sight: drawn at this brightness.
constexpr float MEMORY_DIM = 0.45f;

Color dim(Rgba c, bool lit) {
    float k = lit ? 1.0f : MEMORY_DIM;
    return {(unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), c.a};
}

/// Draws a sprite's pixels with its top-left at x, y, dimmed unless lit.
void drawSprite(const Sprite& s, int x, int y, bool lit) {
    for (int gy = 0; gy < s.h; gy++)
        for (int gx = 0; gx < s.w; gx++) {
            Rgba c = s.pixel(gx, gy);
            if (c.solid()) DrawPixel(x + gx, y + gy, dim(c, lit));
        }
}

} // namespace

void drawKey(int x, int y, bool lit) { drawSprite(sprites::get("key", CELL_PX, CELL_PX), x, y, lit); }

void drawCake(int x, int y, bool lit) { drawSprite(sprites::get("cake", CELL_PX, CELL_PX), x, y, lit); }

void drawHeapCake(int x, int y, bool lit) {
    drawSprite(sprites::get("cake put down", HEAP_CAKE_W, HEAP_CAKE_H), x, y, lit);
}

// Spikes up and plates down show a trap that just went off, or a spent one.
void drawTrap(int x, int y, TrapKind kind, bool sprung, bool lit) {
    const char* name = kind == TrapKind::Spikes ? (sprung ? "spikes up" : "spikes") : (sprung ? "plate down" : "plate");
    drawSprite(sprites::get(name, CELL_PX, CELL_PX), x, y, lit);
}
