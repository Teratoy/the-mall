// A look at the server rack: its front at a few ticks of its clock, so the
// lights, reels and screen can be seen moving, then its side, then front and
// side at the size a phone shows them with a human and a vending machine
// beside them for size (the human blown up too), and the same row again
// dimmed, as remembered.
//
// Usage: build/server_rack [out.png] [seed]
// Build, after make: g++ -std=c++20 -Isrc tools/server_rack.cpp $(find build -path build/android -prune -o -name "*.o" ! -name main.o -print)
//                    -o build/server_rack $(pkg-config --libs raylib) -lm
#include "core/rng.h"
#include "human/human_look.h"
#include "render/server_rack_art.h"
#include "render/vending_art.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

namespace {

/// Blown up this many times for looking at, and as a phone shows the canvas.
constexpr int ZOOM = 10, PHONE = 2;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
/// The ticks the front is shown at: still, then a few apart.
constexpr int TICKS[] = {0, 5, 11, 19};
constexpr int FRAMES = sizeof TICKS / sizeof TICKS[0];
/// Remembered but out of sight, as the art dims it.
constexpr float MEMORY_DIM = 0.45f;

Color toColor(Rgba c, bool inSight = true) {
    float k = inSight ? 1.0f : MEMORY_DIM;
    return {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a};
}

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
    const char* out = argc > 1 ? argv[1] : "build/server_rack.png";
    Rng rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3);
    const SpriteFrame human = HumanLook::random(rng).idle();

    const int big = SERVER_RACK_W * ZOOM, side = SERVER_RACK_DEPTH * ZOOM;
    const int phoneX = GAP + FRAMES * (big + GAP) + side + GAP + SPRITE_W * ZOOM + GAP;
    const int phoneW = (SERVER_RACK_W + SERVER_RACK_DEPTH + SPRITE_W + VENDING_W) * PHONE + 4 * GAP;
    const int rowH = SERVER_RACK_H * ZOOM + GAP;
    Image img = GenImageColor(phoneX + phoneW, GAP + 2 * rowH, BACKDROP);
    for (int row = 0; row < 2; row++) {
        const bool inSight = row == 0;
        const int floor = GAP + row * rowH + SERVER_RACK_H * ZOOM;
        for (int f = 0; f < FRAMES; f++) {
            const int tick = inSight ? TICKS[f] : 0;
            auto px = [&](int i, int j) { return toColor(serverRackPixel(i, j, tick), inSight); };
            blit(img, GAP + f * (big + GAP), floor, SERVER_RACK_W, SERVER_RACK_H, ZOOM, px);
        }
        auto sidePx = [&](int i, int j) { return toColor(serverRackSidePixel(i, j), inSight); };
        blit(img, GAP + FRAMES * (big + GAP), floor, SERVER_RACK_DEPTH, SERVER_RACK_H, ZOOM, sidePx);
        auto humanPx = [&](int i, int j) { return toColor(human.px[size_t(j * SPRITE_W + i)], inSight); };
        blit(img, GAP + FRAMES * (big + GAP) + side + GAP, floor, SPRITE_W, SPRITE_H, ZOOM, humanPx);

        int x = phoneX;
        auto frontPx = [&](int i, int j) { return toColor(serverRackPixel(i, j, TICKS[1]), inSight); };
        blit(img, x, floor, SERVER_RACK_W, SERVER_RACK_H, PHONE, frontPx);
        x += SERVER_RACK_W * PHONE + GAP;
        blit(img, x, floor, SERVER_RACK_DEPTH, SERVER_RACK_H, PHONE, sidePx);
        x += SERVER_RACK_DEPTH * PHONE + GAP;
        blit(img, x, floor, SPRITE_W, SPRITE_H, PHONE, humanPx);
        x += SPRITE_W * PHONE + GAP;
        auto vendPx = [&](int i, int j) {
            return toColor(vendingPixel(Vend::Cake, i, j, vendingFull(Vend::Cake), 0), inSight);
        };
        blit(img, x, floor, VENDING_W, VENDING_H, PHONE, vendPx);
    }
    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
