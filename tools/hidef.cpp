// A trial of a human drawn at more detail: one standing pose, at twice
// (30x36) and three times (45x54) the pixels of 15x18, painted by hand from
// the same rolled colours, saved beside the sprite the game draws now.
//
// Usage: build/hidef [out.png] [seed]   (or: make hidef, to build/hidef.png)
//
// The picture shows the old sprite, the 2x and the 3x, left to right: on
// top all at the same size, blown up; below the same, smaller.
#include "core/rng.h"
#include "human/human_look.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>

namespace {

/// Blown up for looking at: the old sprite this many times, the 2x half as
/// many and the 3x a third; below, SMALL times instead.
constexpr int ZOOM = 12, SMALL = 6;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
constexpr Color BUCKLE{176, 168, 140, 255};
constexpr Color EYE_WHITE{226, 224, 218, 255};

/// The standing human, facing right, the back of the head and the back
/// arm on the left. H hair, G its shine, S skin, s skin in shade, m the
/// mouth, E eyes, T top, t top in shade, v the back sleeve's hem, B bottom,
/// b bottom in shade, k the buckle, O shoes, o the back shoe.
constexpr const char* STANDING_2X[SPRITE_H * 2] = {
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "..............................",
    "............HHHGGH............",
    "...........HHHHHGGH...........",
    "..........HHHHHHHSSS..........",
    "..........HHHSSSSSSS..........",
    "..........HHsSESSESS..........",
    "..........HHsSESSESS..........",
    "..........HSSSSSSSSS..........",
    "..........HSSSSSmmSS..........",
    "...........SSSSSSSS...........",
    "............SSSSSS............",
    ".............ssss.............",
    ".............sSSs.............",
    "............TTSSTT............",
    "...........TTTTTTTT...........",
    "........tttTTTTTTTTTTT........",
    "........tttTTTTTTTTTTT........",
    "........tttTTTTTTTTTTT........",
    "........vvtTTTTTTTTTtt........",
    "........sstTTTTTTTTTSS........",
    "........sstTTTTTTTTTSS........",
    "........ssbbbbbbbkbbSS........",
    "........ssBBBBBBBBBBSS........",
    "........ssBBBBBBBBBBSS........",
    ".........sbbbb..BBBBS.........",
    "..........bbbb..BBBB..........",
    "..........bbbb..BBBB..........",
    "..........bbbb..BBBB..........",
    "..........bbbb..BBBB..........",
    "..........oooo..OOOO..........",
    "..........ooooooOOOOOO........",
};

/// The same at three times the pixels, with more of it shown: h the hair's
/// edge and the brows, W the whites of the eyes, q skin in deep shade (the
/// inside of the ear, the back arm's far side), c the collar, d the belt,
/// P the laces, p the soles.
constexpr const char* STANDING_3X[SPRITE_H * 3] = {
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    ".............................................",
    "...................hhhhhhh...................",
    ".................hHHHHGGGHHh.................",
    "................HHHHHHHGGGGHH................",
    "...............HHHHHHHHHHHGHHH...............",
    "...............HHHHHHHHHHHSSSS...............",
    "...............HHHHHHSSHSSSSSS...............",
    "...............HHHSSShhSSShhSS...............",
    "...............HHHssSWESSSWESS...............",
    "...............HHHsqSWESSSWESS...............",
    "...............HHHssSSSSSSSSSSS..............",
    "...............HHSSSSSSSSSSSSs...............",
    "...............SSSSSSSSSSmmmSS...............",
    "................SSSSSSSSSSSSS................",
    ".................SSSSSSSSSSSS................",
    "...................SSSSSSSSS.................",
    "...................sssssss...................",
    "...................sSSSSSS...................",
    "...................sSSSSSS...................",
    ".................TTcSSSSScTT.................",
    "...............TTTTTcccccTTTTT...............",
    ".............ttttTTTTTTTTTTTTTTT.............",
    "............tttttTTTTTTTTTTTTTTTT............",
    "............tttttTTTTTTTTTTTTTTTT............",
    "............tttttTTTTTTTTTTTTTTTT............",
    "............tttttTTTTTTTTTtTTTTTT............",
    "............tttttTTTTTTTTTtTTTTTT............",
    "............vvvttTTTTTTTTTtTTTttt............",
    "............qssttTTTTTTTTTtTTTsSS............",
    "............qssttTTTTTTTTTTTTTsSS............",
    "............qssTTTTTTTTTTTTTTTsSS............",
    "............qssddddddddddkkdddsSS............",
    "............qssddddddddddkkdddsSS............",
    "............qssbbBBBBBBBBBBBBBsSS............",
    "............qssbbbbbbBBBBBBBBBsSS............",
    "............qssbbbbbbBBBBBBBBBSSS............",
    ".............qqbbbbbbb.BBBBBBBSSs............",
    "...............bbbbbb...BBBBBB...............",
    "...............bbbbbb...BBbBBB...............",
    "...............bbbbbb...BBbBBB...............",
    "...............bbbbbb...BBbBBB...............",
    "...............bbbbbb...BBBBBB...............",
    "...............bbbbbb...BBBBBB...............",
    "...............oooooo...OOOOOO...............",
    "...............oooooooo.OOOPPOOO.............",
    "...............pppppppppppppppppp............",
};

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

Color shaded(Rgba c, float k) { return {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), 255}; }

Color shine(Rgba c, float k) {
    auto f = [k](uint8_t v) { return uint8_t(v + (255 - v) * k); };
    return {f(c.r), f(c.g), f(c.b), 255};
}

Color ink(const HumanLook& l, char ch) {
    switch (ch) {
    case 'H': return toColor(l.hair);
    case 'h': return shaded(l.hair, 0.72f);
    case 'G': return shine(l.hair, 0.3f);
    case 'S': return toColor(l.skin);
    case 's': return shaded(l.skin, 0.82f);
    case 'q': return shaded(l.skin, 0.68f);
    case 'm': return shaded(l.skin, 0.62f);
    case 'W': return EYE_WHITE;
    case 'E': return {20, 20, 20, 255};
    case 'T': return toColor(l.top);
    case 't': return shaded(l.top, 0.78f);
    case 'v': return shaded(l.top, 0.62f);
    case 'c': return shaded(l.top, 0.68f);
    case 'B': return toColor(l.bottom);
    case 'b': return shaded(l.bottom, 0.78f);
    case 'd': return shaded(l.bottom, 0.55f);
    case 'k': return BUCKLE;
    case 'O': return toColor(l.shoes);
    case 'o': return shaded(l.shoes, 0.75f);
    case 'P': return shine(l.shoes, 0.35f);
    case 'p': return shaded(l.shoes, 0.55f);
    default: return BLANK;
    }
}

/// Paints w x h pixels from px() at (x, y), each one size screen pixels.
template <typename F> void blit(Image& img, int x, int y, int w, int h, int size, F px) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            Color c = px(i, j);
            if (c.a) ImageDrawRectangle(&img, x + i * size, y + j * size, size, size, c);
        }
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/hidef.png";
    Rng rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3);
    // The look the hand-drawn one is drawn for: the same colours, its shape fixed.
    HumanLook look = HumanLook::random(rng);
    look.hairStyle = Hair::Short;
    look.topStyle = Top::Tee;
    look.bottomStyle = Bottom::Pants;
    look.torsoW = 5;
    look.torsoH = 5;
    look.legH = 4;
    look.beard = false;
    const SpriteFrame old = look.idle();
    auto oldPx = [&](int i, int j) { Rgba c = old.px[size_t(j * SPRITE_W + i)]; return toColor(c); };
    auto px2 = [&](int i, int j) { return ink(look, STANDING_2X[j][i]); };
    auto px3 = [&](int i, int j) { return ink(look, STANDING_3X[j][i]); };

    const int big = SPRITE_W * ZOOM, bigH = SPRITE_H * ZOOM;
    const int small = SPRITE_W * SMALL, smallH = SPRITE_H * SMALL;
    Image img = GenImageColor(GAP * 4 + big * 3, GAP * 3 + bigH + smallH, BACKDROP);
    const int y = GAP * 2 + bigH;
    for (int col = 0; col < 3; col++) {
        int x = GAP + col * (big + GAP), k = col + 1;
        int sx = x + (big - small) / 2;
        if (col == 0) {
            blit(img, x, GAP, SPRITE_W, SPRITE_H, ZOOM, oldPx);
            blit(img, sx, y, SPRITE_W, SPRITE_H, SMALL, oldPx);
        } else if (col == 1) {
            blit(img, x, GAP, SPRITE_W * k, SPRITE_H * k, ZOOM / k, px2);
            blit(img, sx, y, SPRITE_W * k, SPRITE_H * k, SMALL / k, px2);
        } else {
            blit(img, x, GAP, SPRITE_W * k, SPRITE_H * k, ZOOM / k, px3);
            blit(img, sx, y, SPRITE_W * k, SPRITE_H * k, SMALL / k, px3);
        }
    }
    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
