// Draws a sheet of human frames with a bottle in the pocket or the hand, at
// several levels and dirt, scaled up, to check the bottle art by eye.
// Usage: build/bottle_sheet [out.png] [scale]   (make bottlesheet)
// Each row is one look; the columns are idle, a stride, kneeling at water,
// sitting, sipping, filling, pouring and handing a bottle over, each with
// the bottle full, half, nearly empty, foul and bloody in turn.
#include "human/human_look.h"
#include "render/bottle_art.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/bottle-sheet.png";
    int scale = argc > 2 ? std::atoi(argv[2]) : 8;
    SetTraceLogLevel(LOG_WARNING);
    const Bottle bottles[] = {{1.0f}, {0.5f}, {0.15f}, {0.8f, 0.8f, 0.0f}, {0.8f, 0.0f, 0.8f}};
    constexpr int LOOKS = 5, KINDS = 8, LEVELS = int(std::size(bottles));
    constexpr int PAD = 3, CW = SPRITE_W + 2 * PAD + 3, CH = SPRITE_H + 2 * PAD;
    Image sheet = GenImageColor(KINDS * LEVELS * CW, LOOKS * CH, Color{60, 60, 66, 255});
    Rng rng(42);
    for (int l = 0; l < LOOKS; l++) {
        HumanLook look = HumanLook::random(rng);
        SpriteFrame frames[KINDS] = {look.idle(), look.walk(1), look.crouch(), look.sit(0),
                                     look.sip(),  look.fill(),  look.pour(),   look.offerBottle()};
        for (int k = 0; k < KINDS; k++)
            for (int v = 0; v < LEVELS; v++) {
                const SpriteFrame& f = frames[k];
                int ox = (k * LEVELS + v) * CW + PAD, oy = l * CH + PAD;
                // A strip of floor under the feet.
                ImageDrawRectangle(&sheet, ox - PAD, oy + SPRITE_H, CW, PAD, Color{96, 92, 84, 255});
                for (int y = 0; y < SPRITE_H; y++)
                    for (int x = 0; x < SPRITE_W; x++) {
                        Rgba p = f.px[size_t(y * SPRITE_W + x)];
                        if (p.a) ImageDrawPixel(&sheet, ox + x, oy + y, Color{p.r, p.g, p.b, p.a});
                    }
                BottleSpot at = f.hand.valid() ? f.hand : f.pocket;
                if (!at.valid()) continue;
                forBottlePixels(at.x, at.y, at.turn, false, bottles[v], [&](int x, int y, Color c) {
                    Color under = GetImageColor(sheet, ox + x, oy + y);
                    float a = c.a / 255.0f;
                    auto m = [a](unsigned char u, unsigned char w) { return (unsigned char)(u + (w - u) * a); };
                    ImageDrawPixel(&sheet, ox + x, oy + y, Color{m(under.r, c.r), m(under.g, c.g), m(under.b, c.b), 255});
                });
            }
    }
    ImageResizeNN(&sheet, sheet.width * scale, sheet.height * scale);
    bool ok = ExportImage(sheet, out);
    UnloadImage(sheet);
    std::printf("%s %s\n", ok ? "wrote" : "could not write", out);
    return ok ? 0 : 1;
}
