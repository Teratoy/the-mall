// A look at the cake vending machine: full, half sold and empty, and full
// again on the next blink of its LEDs, then its side, blown up, with a
// human beside it for its size; then all of them at the size a phone
// shows them, on a floor-dark backdrop.
//
// Usage: build/vending [out.png] [seed]
// Build, after make: g++ -std=c++20 -Isrc tools/vending.cpp $(find build -name "*.o" ! -name main.o)
//                    -o build/vending $(pkg-config --libs raylib) -lm
#include "core/rng.h"
#include "human/human_look.h"
#include "render/vending_art.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

namespace {

/// Blown up this many times for looking at, and as a phone shows the canvas.
constexpr int ZOOM = 10, PHONE = 2;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
/// Stock for each machine shown, and its LEDs' blink: full, three sold, none left, full on the next blink.
constexpr uint8_t STOCKS[] = {VENDING_FULL, 0b100110, 0, VENDING_FULL};
constexpr int BLINKS[] = {0, 0, 0, 1};

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

    const int n = sizeof STOCKS / sizeof STOCKS[0];
    const int big = VENDING_W * ZOOM, bigH = VENDING_H * ZOOM;
    const int side = VENDING_DEPTH * ZOOM;
    const int width = GAP + n * (big + GAP) + side + GAP + SPRITE_W * ZOOM + GAP;
    const int floor = GAP + bigH, phoneFloor = floor + GAP + VENDING_H * PHONE + GAP;
    Image img = GenImageColor(width, phoneFloor + GAP, BACKDROP);
    for (int m = 0; m < n; m++) {
        auto px = [&](int i, int j) { return toColor(vendingPixel(i, j, STOCKS[m], BLINKS[m])); };
        blit(img, GAP + m * (big + GAP), floor, VENDING_W, VENDING_H, ZOOM, px);
        blit(img, GAP + m * (VENDING_W * PHONE + GAP), phoneFloor, VENDING_W, VENDING_H, PHONE, px);
    }
    auto sidePx = [&](int i, int j) { return toColor(vendingSidePixel(i, j, 0)); };
    blit(img, GAP + n * (big + GAP), floor, VENDING_DEPTH, VENDING_H, ZOOM, sidePx);
    const int phoneSide = GAP + n * (VENDING_W * PHONE + GAP);
    blit(img, phoneSide, phoneFloor, VENDING_DEPTH, VENDING_H, PHONE, sidePx);
    blit(img, GAP + n * (big + GAP) + side + GAP, floor, SPRITE_W, SPRITE_H, ZOOM, humanPx);
    blit(img, phoneSide + VENDING_DEPTH * PHONE + GAP, phoneFloor, SPRITE_W, SPRITE_H, PHONE, humanPx);
    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
