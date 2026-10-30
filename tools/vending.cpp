// A look at the vending machines, a row for each kind: full, some sold,
// empty, its side, then the full one and its side at the size a phone
// shows them. A human stands beside the cakes for size, and the gun the
// last machine sells lies beside it, blown up and at phone size.
//
// Usage: build/vending [out.png] [seed]
// Build, after make: g++ -std=c++20 -Isrc tools/vending.cpp $(find build -name "*.o" ! -name main.o)
//                    -o build/vending $(pkg-config --libs raylib) -lm
#include "core/rng.h"
#include "human/human_look.h"
#include "render/gun_art.h"
#include "render/vending_art.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

namespace {

/// Blown up this many times for looking at, and as a phone shows the canvas.
constexpr int ZOOM = 10, PHONE = 2;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
/// What each kind has left when some is sold.
constexpr uint8_t SOME_SOLD[] = {0b100110, 0b011010, 0b01};
static_assert(sizeof SOME_SOLD / sizeof SOME_SOLD[0] == size_t(Vend::Count));

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

/// Paints w x h pixels from px() with their bottom-left at (x, y), each one size screen pixels.
template <typename F> void blit(Image& img, int x, int y, int w, int h, int size, F px) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            Color c = px(i, j);
            if (c.a) ImageDrawRectangle(&img, x + i * size, y - (h - j) * size, size, size, c);
        }
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/vending.png";
    Rng rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3);
    const SpriteFrame human = HumanLook::random(rng).idle();
    auto humanPx = [&](int i, int j) { return toColor(human.px[size_t(j * SPRITE_W + i)]); };

    const int big = VENDING_W * ZOOM, side = VENDING_DEPTH * ZOOM, rowH = VENDING_H * ZOOM + GAP;
    const int phoneX = GAP + 3 * (big + GAP) + side + GAP;
    const int extraX = phoneX + (VENDING_W + VENDING_DEPTH) * PHONE + 2 * GAP;
    const int width = extraX + SPRITE_W * ZOOM + GAP;
    const int kinds = int(Vend::Count);
    Image img = GenImageColor(width, GAP + kinds * rowH, BACKDROP);
    for (int k = 0; k < kinds; k++) {
        Vend kind = Vend(k);
        const int floor = GAP + k * rowH + VENDING_H * ZOOM;
        const uint8_t stocks[3] = {vendingFull(kind), SOME_SOLD[k], 0};
        for (int m = 0; m < 3; m++) {
            auto px = [&](int i, int j) { return toColor(vendingPixel(kind, i, j, stocks[m], 0)); };
            blit(img, GAP + m * (big + GAP), floor, VENDING_W, VENDING_H, ZOOM, px);
        }
        auto sidePx = [&](int i, int j) { return toColor(vendingSidePixel(kind, i, j, 0)); };
        blit(img, GAP + 3 * (big + GAP), floor, VENDING_DEPTH, VENDING_H, ZOOM, sidePx);
        auto fullPx = [&](int i, int j) { return toColor(vendingPixel(kind, i, j, stocks[0], 0)); };
        blit(img, phoneX, floor, VENDING_W, VENDING_H, PHONE, fullPx);
        blit(img, phoneX + VENDING_W * PHONE + GAP, floor, VENDING_DEPTH, VENDING_H, PHONE, sidePx);
        if (kind == Vend::Cake) blit(img, extraX, floor, SPRITE_W, SPRITE_H, ZOOM, humanPx);
        if (kind == Vend::Gun) {
            auto gunPx = [&](int i, int j) { return toColor(gunPixel(i, j)); };
            blit(img, extraX, floor - VENDING_H * PHONE - GAP, GUN_W, GUN_H, ZOOM, gunPx);
            blit(img, extraX, floor, GUN_W, GUN_H, PHONE, gunPx);
        }
    }
    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
