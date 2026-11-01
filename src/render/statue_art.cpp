#include "render/statue_art.h"
#include "core/color.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace {

/// Remembered but out of sight: drawn at this brightness, as the rest of the world.
constexpr float MEMORY_DIM = 0.45f;
/// The statue's shadow on the floor round its base.
constexpr Rgba SHADOW = {0, 0, 0, 70};

/// From darkest to lightest.
enum Tone : uint8_t { OUTLINE, SHADE, BODY, LIGHT, SHINE };
/// Pop paints each part its own colour; the rest are all one material,
/// though an inside or an accent is a shade darker and an eye is outline.
/// Eyes and accents are inlaid: fine lines that neither outline nor get
/// outlined.
enum Part : uint8_t { NONE, MAIN, SECOND, INSIDE, EYE, ACCENT };

bool inlaid(Part p) { return p == EYE || p == ACCENT; }

/// A material's tones, OUTLINE to SHINE.
using Ramp = std::array<Rgba, 5>;

constexpr Ramp MARBLE = {hex(0x6a6660), hex(0xb0aaa2), hex(0xd4cfc6), hex(0xe8e4dc), hex(0xf8f6f2)};
constexpr Ramp BRONZE = {hex(0x34220e), hex(0x74501f), hex(0xa2702f), hex(0xc4934a), hex(0xecd29a)};
constexpr Ramp STONE = {hex(0x343436), hex(0x616165), hex(0x7e7e82), hex(0x98989c), hex(0xb4b4b8)};
constexpr Ramp GRANITE = {hex(0x26262a), hex(0x3e3e44), hex(0x52525a), hex(0x686872), hex(0x84848e)};
constexpr Ramp COFFEE = {hex(0x2a160c), hex(0x4a2a16), hex(0x6a3e22), hex(0x8a5630), hex(0xb07a50)};
constexpr Ramp INK = {hex(0x101010), hex(0x181818), hex(0x202020), hex(0x303030), hex(0xf4f4f4)};
constexpr Ramp CREAM = {hex(0x6a6458), hex(0xc8c0ae), hex(0xe6dfcf), hex(0xf4efe4), hex(0xffffff)};
constexpr Ramp STEEL = {hex(0x34383c), hex(0x6a7076), hex(0x8e959c), hex(0xb2b9c0), hex(0xe0e6ea)};
constexpr Ramp GLASS = {hex(0x7a6e3a), hex(0xd8cc8a), hex(0xf0e6a8), hex(0xfaf4cc), hex(0xffffff)};
constexpr Rgba MARBLE_VEIN = hex(0xa49e95);
constexpr Rgba PATINA = hex(0x5c9a7c), PATINA_DARK = hex(0x3e7058);

/// Hues pop statues are painted in.
constexpr float POP_HUES[] = {0.0f, 0.04f, 0.13f, 0.33f, 0.55f, 0.62f, 0.8f, 0.92f};
constexpr float DUCK_HUE = 0.14f, BEAK_HUE = 0.07f;

Ramp popRamp(float h) {
    return {hsv(h, 0.7f, 0.28f), hsv(h, 0.8f, 0.62f), hsv(h, 0.75f, 0.86f), hsv(h, 0.5f, 0.97f),
            hsv(h, 0.12f, 1.0f)};
}

uint32_t mix(uint32_t a) {
    a ^= a >> 16;
    a *= 0x7feb352du;
    a ^= a >> 15;
    a *= 0x846ca68bu;
    return a ^ (a >> 16);
}

uint32_t noise(uint32_t seed, int x, int y) { return mix(seed ^ mix(uint32_t(x) * 0x9e3779b1u ^ uint32_t(y) * 0x85ebca77u)); }

/// The statue painted as parts and tones, before any colour. Shapes are
/// given unflipped; a flipped statue is mirrored as it is painted, with
/// its light still from the top left.
class Painter {
public:
    /// Shapes are given at a scale of 1 and painted at scale.
    Painter(int w, int h, bool flip, float scale) : w_(w), h_(h), flip_(flip), scale_(scale), cells_(size_t(w * h)) {}

    int w() const { return w_; }
    int h() const { return h_; }

    /// Paints every pixel whose centre, in unflipped coordinates, covers
    /// says is in; tone says how lit it is. Shapes of one layer are one
    /// body; one in a later layer gets outlined where it lies over another.
    void fill(int layer, Part part, const std::function<bool(float, float)>& covers,
              const std::function<Tone(float, float)>& tone) {
        for (int y = 0; y < h_; y++)
            for (int px = 0; px < w_; px++) {
                float fx = ((flip_ ? w_ - 1 - px : px) + 0.5f) / scale_, fy = (y + 0.5f) / scale_;
                if (covers(fx, fy)) cells_[size_t(y * w_ + px)] = {part, tone(fx, fy), uint8_t(layer)};
            }
    }

    /// Lit across a round upright body, from x0 to x1: brightest a little
    /// in from the lit side, dark down the far one.
    Tone cylinder(float fx, float x0, float x1) const {
        float u = (fx - x0) / (x1 - x0);
        if (flip_) u = 1.0f - u;
        if (u < 0.08f) return BODY;
        if (u > 0.18f && u < 0.25f) return SHINE;
        if (u < 0.36f) return LIGHT;
        return u < 0.74f ? BODY : SHADE;
    }
    /// Lit across a tube or rim whose outward normal is nx, ny.
    Tone normal(float nx, float ny) const {
        float lit = -0.6f * nx * (flip_ ? -1.0f : 1.0f) - 0.8f * ny;
        if (lit > 0.85f) return SHINE;
        if (lit > 0.4f) return LIGHT;
        return lit < -0.45f ? SHADE : BODY;
    }
    /// Lit over a round body.
    Tone sphere(float fx, float fy, float cx, float cy, float rx, float ry) const {
        float dx = (fx - cx) / rx * (flip_ ? -1.0f : 1.0f), dy = (fy - cy) / ry;
        float nz = std::sqrt(std::max(0.0f, 1.0f - dx * dx - dy * dy));
        float lit = (-0.5f * dx - 0.6f * dy + 0.62f * nz) / 0.9306f;
        if (lit > 0.95f) return SHINE;
        if (lit > 0.72f) return LIGHT;
        return lit > 0.22f ? BODY : SHADE;
    }

    /// Outlines the statue against the empty pixels round it, and each
    /// layer where a later one lies over it. Eyes are left as they are.
    void outline() {
        std::vector<Cell> out = cells_;
        for (int y = 0; y < h_; y++)
            for (int x = 0; x < w_; x++) {
                Cell& c = out[size_t(y * w_ + x)];
                if (c.part == NONE || inlaid(c.part)) continue;
                for (Vec2i d : DIRS4) {
                    Cell n = at(x + d.x, y + d.y);
                    if (n.part == NONE || (n.layer > c.layer && !inlaid(n.part))) {
                        c.tone = OUTLINE;
                        break;
                    }
                }
            }
        cells_ = out;
    }

    Part part(int x, int y) const { return at(x, y).part; }
    Tone tone(int x, int y) const { return at(x, y).tone; }

private:
    struct Cell {
        Part part = NONE;
        Tone tone = BODY;
        uint8_t layer = 0;
    };
    int w_, h_;
    bool flip_;
    float scale_;
    std::vector<Cell> cells_;

    Cell at(int x, int y) const { return x < 0 || y < 0 || x >= w_ || y >= h_ ? Cell{} : cells_[size_t(y * w_ + x)]; }
};

bool inEllipse(float fx, float fy, float cx, float cy, float rx, float ry) {
    float dx = (fx - cx) / rx, dy = (fy - cy) / ry;
    return dx * dx + dy * dy <= 1.0f;
}

bool inRect(float fx, float fy, float x0, float y0, float x1, float y1) { return fx >= x0 && fx < x1 && fy >= y0 && fy < y1; }

/// Whether p is on the inner side of all three edges of a triangle.
bool inTriangle(float fx, float fy, Vector2 a, Vector2 b, Vector2 c) {
    auto side = [&](Vector2 p, Vector2 q) { return (q.x - p.x) * (fy - p.y) - (q.y - p.y) * (fx - p.x); };
    float s1 = side(a, b), s2 = side(b, c), s3 = side(c, a);
    return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
}

/// A rectangle with its corners rounded to radius r.
bool inRounded(float fx, float fy, float x0, float y0, float x1, float y1, float r) {
    if (!inRect(fx, fy, x0, y0, x1, y1)) return false;
    float dx = fx - std::clamp(fx, x0 + r, x1 - r), dy = fy - std::clamp(fy, y0 + r, y1 - r);
    return dx * dx + dy * dy <= r * r;
}

/// The nearest point to p on the segment from a to b: how far along it is,
/// 0 to 1, how far p is from it, and the way out from it to p.
struct Near {
    float t, d, nx, ny;
};

Near nearSegment(float fx, float fy, Vector2 a, Vector2 b) {
    float vx = b.x - a.x, vy = b.y - a.y;
    float t = std::clamp(((fx - a.x) * vx + (fy - a.y) * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
    float dx = fx - (a.x + t * vx), dy = fy - (a.y + t * vy);
    float d = std::sqrt(dx * dx + dy * dy);
    return d > 0.0f ? Near{t, d, dx / d, dy / d} : Near{t, 0.0f, 0.0f, 0.0f};
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

auto flat(Tone t) {
    return [t](float, float) { return t; };
}

// 42x48, its handle to the right: a round body, the rim seen from a
// little above, and the dark inside.
void paintMug(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) {
        return x > 26 && inEllipse(x, y, 33, 24, 8.5f, 11) && !inEllipse(x, y, 33, 24, 4, 6.5f);
    }, [&](float x, float y) { return p.sphere(x, y, 33, 24, 8.5f, 11); });
    p.fill(1, MAIN, [](float x, float y) { return inRect(x, y, 2, 6, 32, 42) || inEllipse(x, y, 17, 42, 15, 5); },
           [&](float x, float) { return p.cylinder(x, 2, 32); });
    p.fill(2, MAIN, [](float x, float y) { return inEllipse(x, y, 17, 6, 15, 5); }, flat(LIGHT));
    p.fill(3, INSIDE, [](float x, float y) { return inEllipse(x, y, 17, 6.5f, 12.5f, 3.6f); },
           [](float, float y) { return y < 6 ? SHADE : BODY; });
}

// 48x48, facing left: a tail cocked up behind, the body with a wing on it,
// and the head over the front with its beak and eye.
void paintDuck(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) { return inTriangle(x, y, {34, 28}, {47, 12}, {46, 33}); },
           [&](float x, float y) { return p.sphere(x, y, 42, 22, 7, 11); });
    p.fill(1, MAIN, [](float x, float y) { return inEllipse(x, y, 26, 35, 21, 12.5f); },
           [&](float x, float y) { return p.sphere(x, y, 26, 35, 21, 12.5f); });
    p.fill(2, MAIN, [](float x, float y) { return inEllipse(x, y, 31, 34, 10, 6); },
           [&](float x, float y) { return p.sphere(x, y, 31, 33, 10, 6); });
    p.fill(3, MAIN, [](float x, float y) { return inEllipse(x, y, 15, 16, 11, 11); },
           [&](float x, float y) { return p.sphere(x, y, 15, 16, 11, 11); });
    p.fill(4, SECOND, [](float x, float y) { return inEllipse(x, y, 5, 19.5f, 5, 3); },
           [&](float x, float y) { return p.sphere(x, y, 5, 19.5f, 5, 3); });
    p.fill(5, EYE, [](float x, float y) { return inEllipse(x, y, 12.5f, 14, 1.6f, 2.3f); },
           [](float x, float y) { return x < 12.5f && y < 13 ? SHINE : OUTLINE; });
}

// 42x72, from the front and a little above: the back's posts, rails and
// slats, the seat's top and front, and the legs, the back two in shadow.
void paintChair(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) { return inRect(x, y, 9, 54, 12, 66) || inRect(x, y, 30, 54, 33, 66); },
           flat(SHADE));
    p.fill(1, MAIN, [](float x, float y) {
        return inRect(x, y, 4, 2, 9, 50) || inRect(x, y, 33, 2, 38, 50) || inRect(x, y, 4, 2, 38, 10) ||
               inRect(x, y, 4, 34, 38, 38) || inRect(x, y, 12, 10, 15, 34) || inRect(x, y, 19.5f, 10, 22.5f, 34) ||
               inRect(x, y, 27, 10, 30, 34);
    }, [&](float x, float y) {
        if (x < 9) return p.cylinder(x, 4, 9);
        if (x >= 33) return p.cylinder(x, 33, 38);
        return y < 4 || (y >= 34 && y < 35) ? LIGHT : BODY;
    });
    p.fill(2, SECOND, [](float x, float y) { return inRect(x, y, 1, 38, 41, 50); },
           [](float x, float y) { return y < 40 || x < 4 ? LIGHT : BODY; });
    p.fill(3, SECOND, [](float x, float y) { return inRect(x, y, 1, 50, 41, 55); }, flat(SHADE));
    p.fill(4, MAIN, [](float x, float y) { return inRect(x, y, 2, 55, 7, 72) || inRect(x, y, 35, 55, 40, 72); },
           [&](float x, float) { return x < 7 ? p.cylinder(x, 2, 7) : p.cylinder(x, 35, 40); });
}

// 60x48, spout to the left and handle to the right: a squat round body on
// a foot, its domed lid with a rim and a knob.
void paintTeapot(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) {
        return x > 44 && inEllipse(x, y, 50, 25, 8.5f, 11) && !inEllipse(x, y, 50, 25, 4.5f, 6.5f);
    }, [&](float x, float y) { return p.sphere(x, y, 50, 25, 8.5f, 11); });
    p.fill(0, MAIN, [](float x, float y) {
        Near n = nearSegment(x, y, {17, 33}, {3, 13});
        return n.d <= lerp(4.2f, 2.2f, n.t);
    }, [&](float x, float y) {
        Near n = nearSegment(x, y, {17, 33}, {3, 13});
        return p.normal(n.nx, n.ny);
    });
    p.fill(1, MAIN, [](float x, float y) { return inEllipse(x, y, 30, 31, 18.5f, 14.5f) || inEllipse(x, y, 30, 44, 11, 3); },
           [&](float x, float y) { return p.sphere(x, y, 30, 31, 18.5f, 14.5f); });
    p.fill(2, MAIN, [](float x, float y) { return y < 17 && inEllipse(x, y, 30, 17, 10, 6); },
           [&](float x, float y) { return p.sphere(x, y, 30, 17, 10, 6); });
    p.fill(3, MAIN, [](float x, float y) { return inEllipse(x, y, 30, 17.5f, 11.5f, 2.5f); }, flat(LIGHT));
    p.fill(4, SECOND, [](float x, float y) { return inEllipse(x, y, 30, 9, 3, 2.6f); },
           [&](float x, float y) { return p.sphere(x, y, 30, 9, 3, 2.6f); });
}

// 54x60, from the front and a little above: the tank behind with a flush
// lever, its lid propped up against it, then the bowl with its seat down
// and the dark of its hole, on a pedestal that flares out to the floor.
void paintToilet(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) { return inRounded(x, y, 11, 5, 43, 30, 3); },
           [&](float x, float) { return p.cylinder(x, 11, 43); });
    p.fill(1, MAIN, [](float x, float y) { return inRounded(x, y, 9, 2, 45, 8, 2); },
           [](float x, float y) { return y < 4 || x < 12 ? LIGHT : BODY; });
    p.fill(1, SECOND, [](float x, float y) { return inRounded(x, y, 13, 9, 19, 11, 1); }, flat(LIGHT));
    // The lid is flat: a rim lit on its upper left, shaded on its lower right.
    p.fill(2, SECOND, [](float x, float y) { return inEllipse(x, y, 27, 24, 14, 12); }, [](float x, float y) {
        if (inEllipse(x, y, 27, 24, 11.5f, 9.5f)) return BODY;
        return x + y < 51 ? LIGHT : SHADE;
    });
    auto half = [](float y) { return lerp(9, 13, (y - 44) / 14); };
    p.fill(3, MAIN, [&](float x, float y) {
        return (y >= 35 && inEllipse(x, y, 27, 36, 19, 12)) || (y >= 44 && y < 58 && std::fabs(x - 27) <= half(y));
    }, [&](float x, float y) {
        if (y < 46) return p.sphere(x, y, 27, 37, 19, 12);
        float h = half(std::min(y, 58.0f));
        return p.cylinder(x, 27 - h, 27 + h);
    });
    p.fill(4, SECOND, [](float x, float y) { return inEllipse(x, y, 27, 35.5f, 20.5f, 6.5f); },
           [](float x, float y) { return y < 32.5f || (x < 14 && y < 36) ? LIGHT : BODY; });
    p.fill(5, INSIDE, [](float x, float y) { return inEllipse(x, y, 27, 36, 13, 3.8f); },
           [](float, float y) { return y < 35 ? SHADE : BODY; });
}

// 54x42, from the front and a little above: a rounded box, its top with two
// bread slots, a lever down the side, a dial, and little feet.
void paintToaster(Painter& p) {
    p.fill(0, SECOND, [](float x, float y) { return inRect(x, y, 7, 38, 13, 42) || inRect(x, y, 41, 38, 47, 42); },
           flat(SHADE));
    p.fill(1, MAIN, [](float x, float y) { return inRounded(x, y, 2, 8, 52, 40, 6); },
           [&](float x, float) { return p.cylinder(x, 2, 52); });
    p.fill(2, MAIN, [](float x, float y) { return inRounded(x, y, 4, 2, 50, 14, 5); },
           [](float x, float y) { return x < 10 && y < 6 ? SHINE : LIGHT; });
    p.fill(3, INSIDE, [](float x, float y) {
        return inRounded(x, y, 11, 4, 43, 6, 1) || inRounded(x, y, 11, 9, 43, 11, 1) ||

               inRect(x, y, 45, 17, 47.5f, 34);
    }, flat(SHADE));
    p.fill(4, SECOND, [](float x, float y) { return inRounded(x, y, 42, 20, 51, 24, 1.5f); }, flat(LIGHT));
    p.fill(4, SECOND, [](float x, float y) { return inEllipse(x, y, 11, 29, 3.5f, 3.5f); },
           [&](float x, float y) { return p.sphere(x, y, 11, 29, 3.5f, 3.5f); });
}

// 48x84: a round foot, an arm in two straight lengths with joints, and a
// cone of a shade at the top looking down to the left.
void paintLamp(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) { return inEllipse(x, y, 28, 78, 17, 5.5f); },
           [&](float x, float y) { return p.sphere(x, y, 28, 77, 17, 6); });
    p.fill(1, MAIN, [](float x, float y) { return inEllipse(x, y, 28, 74.5f, 8, 3.5f); },
           [&](float x, float y) { return p.sphere(x, y, 28, 74.5f, 8, 3.5f); });
    for (auto [a, b] : {std::pair<Vector2, Vector2>{{28, 73}, {39, 42}}, {{39, 42}, {20, 16}}})
        p.fill(2, MAIN, [a, b](float x, float y) { return nearSegment(x, y, a, b).d <= 2.2f; },
               [&p, a, b](float x, float y) {
                   Near n = nearSegment(x, y, a, b);
                   return p.normal(n.nx, n.ny);
               });
    p.fill(3, SECOND, [](float x, float y) { return inEllipse(x, y, 39, 42, 3.5f, 3.5f); },
           [&](float x, float y) { return p.sphere(x, y, 39, 42, 3.5f, 3.5f); });
    auto half = [](float y) { return lerp(4, 13, (y - 5) / 22); };
    p.fill(4, MAIN, [&](float x, float y) {
        return (y >= 5 && y < 27 && std::fabs(x - 13) <= half(y)) || inEllipse(x, y, 13, 27, 13, 3);
    }, [&](float x, float y) {
        float h = half(std::clamp(y, 5.0f, 27.0f));
        return p.cylinder(x, 13 - h, 13 + h);
    });
    p.fill(5, INSIDE, [](float x, float y) { return inEllipse(x, y, 13, 27.5f, 11, 2.5f); }, flat(SHADE));
    p.fill(5, SECOND, [](float x, float y) { return inEllipse(x, y, 13, 5, 4.5f, 2); },
           [&](float x, float y) { return p.sphere(x, y, 13, 5, 4.5f, 2); });
}

// 48x54: two bells and their hammer on top, splayed legs, a round body and
// its face, with ticks at the quarters and the hands at ten past twelve.
void paintClock(Painter& p) {
    p.fill(0, MAIN, [](float x, float y) {
        return y < 11 && (inEllipse(x, y, 12, 10, 8, 6.5f) || inEllipse(x, y, 36, 10, 8, 6.5f));
    }, [&](float x, float y) { return x < 24 ? p.sphere(x, y, 12, 10, 8, 6.5f) : p.sphere(x, y, 36, 10, 8, 6.5f); });
    p.fill(0, SECOND, [](float x, float y) { return inRect(x, y, 22.5f, 3, 25.5f, 12) || inEllipse(x, y, 24, 3, 2.5f, 2.5f); },
           [&](float x, float y) { return p.sphere(x, y, 24, 3, 2.5f, 2.5f); });
    for (auto [a, b] : {std::pair<Vector2, Vector2>{{14, 44}, {8, 52}}, {{34, 44}, {40, 52}}})
        p.fill(0, MAIN, [a, b](float x, float y) { return nearSegment(x, y, a, b).d <= 2; },
               [&p, a, b](float x, float y) {
                   Near n = nearSegment(x, y, a, b);
                   return p.normal(n.nx, n.ny);
               });
    p.fill(1, MAIN, [](float x, float y) { return inEllipse(x, y, 24, 30, 20, 19.5f); },
           [&](float x, float y) { return p.sphere(x, y, 24, 30, 20, 19.5f); });
    p.fill(2, SECOND, [](float x, float y) { return inEllipse(x, y, 24, 30, 15, 14.5f); },
           [](float x, float y) { return inEllipse(x, y, 25.5f, 31.5f, 14, 13.5f) ? BODY : SHADE; });
    p.fill(3, EYE, [](float x, float y) {
        return nearSegment(x, y, {24, 30}, {24, 20}).d <= 1.2f || nearSegment(x, y, {24, 30}, {31, 33}).d <= 1.0f ||
               inRect(x, y, 23, 16.5f, 25, 19) || inRect(x, y, 23, 41, 25, 43.5f) || inRect(x, y, 10.5f, 29, 13, 31) ||
               inRect(x, y, 35, 29, 37.5f, 31);
    }, flat(OUTLINE));
}

// 18x90: the bowl up top, hollow toward us with its light on the far
// side, then the neck narrowing and the handle widening down into the floor.
void paintSpoon(Painter& p) {
    auto half = [](float y) { return y < 36 ? lerp(3, 1.8f, (y - 28) / 8) : lerp(1.8f, 3.6f, (y - 36) / 50); };
    p.fill(0, MAIN, [&](float x, float y) {
        return (y >= 28 && y < 86 && std::fabs(x - 9) <= half(y)) || inEllipse(x, y, 9, 86, 3.6f, 2.5f);
    }, [&](float x, float y) {
        float h = half(std::clamp(y, 28.0f, 86.0f));
        return p.cylinder(x, 9 - h, 9 + h);
    });
    p.fill(1, MAIN, [](float x, float y) { return inEllipse(x, y, 9, 17, 8.5f, 15); },
           [&](float x, float y) { return p.sphere(x, y, 9, 17, 8.5f, 15); });
    p.fill(2, MAIN, [](float x, float y) { return inEllipse(x, y, 9, 16, 6, 11.5f); },
           [&](float x, float y) { return p.sphere(18 - x, 32 - y, 9, 16, 6, 11.5f); });
}

// 36x60: a round glass globe over a neck, the filament on its wires inside,
// and the threaded screw it stands on with its contact under it.
void paintBulb(Painter& p) {
    p.fill(0, SECOND, [](float x, float y) { return inRect(x, y, 11, 40, 25, 55) || inEllipse(x, y, 18, 55, 6.5f, 2); },
           [&](float x, float y) { return int(y) % 4 == 0 ? SHADE : p.cylinder(x, 11, 25); });
    p.fill(1, INSIDE, [](float x, float y) { return inEllipse(x, y, 18, 57.5f, 3.5f, 2); }, flat(SHADE));
    auto half = [](float y) { return lerp(10.5f, 7, (y - 28) / 14); };
    p.fill(1, MAIN, [&](float x, float y) { return y >= 28 && y < 42 && std::fabs(x - 18) <= half(y); },
           [&](float x, float y) { return p.cylinder(x, 18 - half(y), 18 + half(y)); });
    p.fill(2, MAIN, [](float x, float y) { return inEllipse(x, y, 18, 17, 16, 15.5f); },
           [&](float x, float y) { return p.sphere(x, y, 18, 17, 16, 15.5f); });
    p.fill(3, ACCENT, [](float x, float y) {
        return nearSegment(x, y, {15, 38}, {14, 20}).d <= 0.6f || nearSegment(x, y, {21, 38}, {22, 20}).d <= 0.6f ||
               (inRect(x, y, 14, 18.5f, 22, 21) && int(x) % 2 == 0);
    }, flat(BODY));
}

Ramp materialRamp(StatueMaterial m) {
    switch (m) {
    case StatueMaterial::Marble: return MARBLE;
    case StatueMaterial::Bronze: return BRONZE;
    case StatueMaterial::Stone: return STONE;
    case StatueMaterial::Pop:
    case StatueMaterial::Count: break;
    }
    return MARBLE;
}

/// What colour a part comes out in a tone, with the material's veins,
/// patina or speckle worked in by where the pixel is.
Rgba colorOf(const Statue& s, Part part, Tone tone, int x, int y) {
    uint32_t n = noise(s.seed, x, y);
    if (s.material == StatueMaterial::Pop) {
        float main = s.kind == StatueKind::Duck ? DUCK_HUE : POP_HUES[s.seed % std::size(POP_HUES)];
        const Ramp body = s.kind == StatueKind::Bulb ? GLASS : popRamp(main);
        switch (part) {
        case EYE: return INK[tone];
        case ACCENT: return popRamp(BEAK_HUE)[tone];
        case INSIDE:
            if (s.kind == StatueKind::Mug) return COFFEE[tone];
            return s.kind == StatueKind::Bulb ? INK[tone] : body[std::max(0, tone - 1)];
        case SECOND:
            switch (s.kind) {
            case StatueKind::Duck: return popRamp(BEAK_HUE)[tone];
            case StatueKind::Clock: return CREAM[tone];
            case StatueKind::Toaster:
            case StatueKind::Lamp:
            case StatueKind::Bulb: return STEEL[tone];
            default: return popRamp(main + 0.5f)[tone];
            }
        default: return body[tone];
        }
    }
    const Ramp r = materialRamp(s.material);
    if (part == EYE) return r[OUTLINE];
    int t = part == INSIDE || part == ACCENT ? std::max(0, tone - 1) : tone;
    if (t == OUTLINE) return r[OUTLINE];
    switch (s.material) {
    case StatueMaterial::Marble: {
        int wobble = int(noise(s.seed, 0, y / 2) % 3);
        if (t >= BODY && (x + y + wobble + int(s.seed % 13)) % 13 == 0) return MARBLE_VEIN;
        break;
    }
    case StatueMaterial::Bronze:
        if (t <= BODY && n % 11 == 0) return t == SHADE ? PATINA_DARK : PATINA;
        break;
    case StatueMaterial::Stone:
        if (n % 6 == 0) t = std::max(1, t - 1);
        else if (n % 11 == 0) t = std::min(4, t + 1);
        break;
    default: break;
    }
    return r[t];
}

void put(Image& img, int x, int y, Rgba c) { ImageDrawPixel(&img, x, y, {c.r, c.g, c.b, c.a}); }

// The base's bottom edge is the image's. A plinth fills the base, its top
// raised by its rise over its front face, and the statue stands a cell in
// from its edges; without one the statue stands on the floor in its shadow.
// A grand statue rounds its base up to whole cells, and stands in the
// middle of it.
Texture2D paint(const Statue& s) {
    const int sw = int(STATUE_BASES[size_t(s.kind)].x * CELL_PX * statueScale(s)), sh = statueBodyPx(s);
    const int w = s.size.x * CELL_PX, h = statueHeightPx(s);
    Image img = GenImageColor(w, h, BLANK);

    Painter p(sw, sh, s.flip, statueScale(s));
    switch (s.kind) {
    case StatueKind::Mug: paintMug(p); break;
    case StatueKind::Duck: paintDuck(p); break;
    case StatueKind::Chair: paintChair(p); break;
    case StatueKind::Teapot: paintTeapot(p); break;
    case StatueKind::Toilet: paintToilet(p); break;
    case StatueKind::Toaster: paintToaster(p); break;
    case StatueKind::Lamp: paintLamp(p); break;
    case StatueKind::Clock: paintClock(p); break;
    case StatueKind::Spoon: paintSpoon(p); break;
    case StatueKind::Bulb: paintBulb(p); break;

    case StatueKind::Count: break;
    }
    p.outline();

    const int inner = (s.size.x - (s.plinth ? 2 : 0)) * CELL_PX;
    int ox = (inner - sw) / 2, oy = h - sh;
    if (s.plinth) {
        const Ramp r = s.material == StatueMaterial::Marble || s.material == StatueMaterial::Bronze ? GRANITE : MARBLE;
        const int d = s.size.y * CELL_PX, top = h - d - PLINTH_RISE_PX;
        for (int y = top; y < h; y++)
            for (int x = 0; x < w; x++) {
                bool edge = x == 0 || x == w - 1 || y == top || y == h - 1 || y == h - PLINTH_RISE_PX;
                Tone t = edge ? OUTLINE : y > h - PLINTH_RISE_PX ? SHADE : y == top + 1 || x == 1 ? LIGHT : BODY;
                put(img, x, y, r[t]);
            }
        ox += CELL_PX;

        oy = h - PLINTH_RISE_PX - CELL_PX - sh;
    } else {
        const float d = s.size.y * CELL_PX;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (inEllipse(x + 0.5f, y + 0.5f, w / 2.0f, h - d / 2.0f, w / 2.0f + 1, d / 2.0f))
                    put(img, x, y, SHADOW);
    }
    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++) {
            Part part = p.part(x, y);
            if (part != NONE) put(img, ox + x, oy + y, colorOf(s, part, p.tone(x, y), x, y));
        }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

} // namespace

StatueArt::~StatueArt() {
    for (auto& [id, tex] : textures_) UnloadTexture(tex);
}

void StatueArt::draw(const Statue& s, int id, int x, int y, bool inSight) const {
    auto it = textures_.find(id);
    if (it == textures_.end()) it = textures_.emplace(id, paint(s)).first;
    unsigned char k = (unsigned char)(inSight ? 255 : 255 * MEMORY_DIM);
    DrawTexture(it->second, x, y - it->second.height, {k, k, k, 255});
}
