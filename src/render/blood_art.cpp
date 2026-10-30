#include "render/blood_art.h"
#include "world/blood.h"
#include <algorithm>
#include <raylib.h>

namespace {

constexpr Color BLOOD_WET = {150, 12, 18, 255};
constexpr Color BLOOD_DRY = {70, 22, 16, 255};
/// The thinnest blood shows this see-through, the thickest not at all.
constexpr int ALPHA_MIN = 90;
/// Water at its reddest, laid over it this thickly.
constexpr Color WATER_BLOOD = {140, 24, 30, 255};
constexpr float WATER_TINT_ALPHA = 170.0f;

unsigned char mix(unsigned char dry, unsigned char wet, float t, float brightness) {
    return (unsigned char)((dry + (wet - dry) * t) * brightness);
}

} // namespace

Color bloodColor(float wet) {
    return {mix(BLOOD_DRY.r, BLOOD_WET.r, wet, 1.0f), mix(BLOOD_DRY.g, BLOOD_WET.g, wet, 1.0f),
            mix(BLOOD_DRY.b, BLOOD_WET.b, wet, 1.0f), 255};
}

unsigned char bloodAlpha(int amount) {
    return (unsigned char)std::min(255, ALPHA_MIN + amount * (255 - ALPHA_MIN) / 160);
}

void drawWaterTint(int x, int y, float tint) {
    Color c = WATER_BLOOD;
    c.a = (unsigned char)(std::min(1.0f, tint) * WATER_TINT_ALPHA);
    DrawRectangle(x, y, CELL_PX, CELL_PX, c);
}

void drawStain(int x, int y, const Stain& stain, float brightness) {
    Color c{mix(BLOOD_DRY.r, BLOOD_WET.r, stain.wet, brightness), mix(BLOOD_DRY.g, BLOOD_WET.g, stain.wet, brightness),
            mix(BLOOD_DRY.b, BLOOD_WET.b, stain.wet, brightness), 255};
    for (int py = 0; py < CELL_PX; py++)
        for (int px = 0; px < CELL_PX; px++) {
            int a = stain.amount[size_t(py * CELL_PX + px)];
            if (a == 0) continue;
            c.a = bloodAlpha(a);
            DrawPixel(x + px, y + py, c);
        }
}
