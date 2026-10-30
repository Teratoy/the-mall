#pragma once
#include <cmath>
#include <cstdint>

/// A sprite pixel. Kept apart from raylib so the simulation never needs it.
struct Rgba {
    uint8_t r = 0, g = 0, b = 0, a = 0;
    constexpr bool solid() const { return a > 0; }
};

constexpr Rgba hex(uint32_t rgb) {
    return {uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb), 255};
}

/// h, s and v in 0..1.
inline Rgba hsv(float h, float s, float v) {
    h -= std::floor(h);
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    switch (int(h * 6.0f) % 6) {
    case 0: r = c, g = x; break;
    case 1: r = x, g = c; break;
    case 2: g = c, b = x; break;
    case 3: g = x, b = c; break;
    case 4: r = x, b = c; break;
    default: r = c, b = x; break;
    }
    auto u = [m](float f) { return uint8_t(std::lround((f + m) * 255.0f)); };
    return {u(r), u(g), u(b), 255};
}
