// Every sprite drawn from data/sprites.txt, at 1x, in all the ways the game
// draws it: the machines at every stock and both blinks, the toilet turned
// to each wall and dirtied, the items and traps lit and dimmed, a sleeper's
// Z's and a human carrying cakes, and the icon. Compare two of these with
// cmp to check a change drew the same, pixel for pixel.
//
// Usage: build/sprite_sheet [out.png]      (make spritesheet)
//        build/sprite_sheet --ref DIR       the sprite editor's human and machine slots (make spriteref)
#include "core/rng.h"
#include "human/human_look.h"
#include "render/gun_art.h"
#include "render/human_sprites.h"
#include "render/icon_art.h"
#include "render/item_art.h"
#include "render/toilet_art.h"
#include "render/vending_art.h"
#include "world/chunk.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <raylib.h>

namespace {

constexpr int SHEET_W = 900, SHEET_H = 360;
/// Pixels between one sprite and the next.
constexpr int GAP = 2;
constexpr Color BACKDROP{58, 56, 62, 255};

/// What each machine is called in data/sprites.txt, before " machine".
constexpr const char* MACHINES[] = {"cake", "bottle", "gun"};
static_assert(sizeof MACHINES / sizeof MACHINES[0] == size_t(Vend::Count));

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

/// Paints w x h pixels from px() with their top-left at (x, y).
template <typename F> void blit(int x, int y, int w, int h, F px) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            Color c = px(i, j);
            if (c.a) DrawPixel(x + i, y + j, c);
        }
}

/// Draws the whole sheet, the human and the icon given.
void drawSheet(const HumanSprites& human, Texture2D icon) {
    ClearBackground(BACKDROP);
    int y = GAP;
    // A row for each machine: every stock, at both blinks, then its side.
    for (int k = 0; k < int(Vend::Count); k++) {
        Vend kind = Vend(k);
        int x = GAP;
        for (int blink = 0; blink < 2; blink++) {
            for (int stock = 0; stock <= vendingFull(kind); stock++) {
                if (x + VENDING_W > SHEET_W) x = GAP, y += VENDING_H + GAP;
                blit(x, y, VENDING_W, VENDING_H,
                     [&](int i, int j) { return toColor(vendingPixel(kind, i, j, uint8_t(stock), blink)); });
                x += VENDING_W + GAP;
            }
            blit(x, y, VENDING_DEPTH, VENDING_H,
                 [&](int i, int j) { return toColor(vendingSidePixel(kind, i, j, blink)); });
            x += VENDING_DEPTH + GAP;
        }
        y += VENDING_H + GAP;
    }
    // The drawn ones, as the world draws them, lit then dimmed.
    int x = GAP;
    for (bool lit : {true, false}) {
        drawGun(x, y, lit);
        drawKey(x + 8, y, lit);
        drawCake(x + 16, y, lit);
        drawHeapCake(x + 24, y, lit);
        int tx = x + 32;
        for (TrapKind kind : {TrapKind::Spikes, TrapKind::Slam})
            for (bool sprung : {false, true}) drawTrap(tx, y, kind, sprung, lit), tx += CELL_PX + GAP;
        for (Vec2i back : {Vec2i{0, -1}, Vec2i{0, 1}, Vec2i{-1, 0}, Vec2i{1, 0}})
            for (float dirt : {0.0f, 1.0f, 3.0f, 8.0f}) drawToilet(tx, y, back, dirt, 7, lit), tx += CELL_PX + GAP;
        y += CELL_PX + GAP;
    }
    // A human carrying cakes, and asleep with its Z's rising at a few moments.
    x = GAP;
    for (int cakes = 1; cakes <= 3; cakes++, x += SPRITE_W + GAP)
        human.draw({float(x + SPRITE_W / 2), float(y + SPRITE_H + 24)}, Pose::Idle, 0.0f, false, 1, WHITE, cakes);
    for (float t : {0.5f, 1.4f, 2.0f, 3.3f}) {
        human.draw({float(x + SPRITE_W / 2), float(y + SPRITE_H + 24)}, Pose::Sleep, t, false, 1);
        human.draw({float(x + SPRITE_W / 2 + 30), float(y + SPRITE_H + 24)}, Pose::Sleep, t, false, 2);
        x += 80;
    }
    DrawTexture(icon, x, y, WHITE);
}

/// Writes what the sprite editor's previews need into dir: human.png, a
/// human standing, for size; and ref.json, where each machine holds its goods.
bool writeRef(const std::string& dir) {
    Rng rng(3);
    const SpriteFrame frame = HumanLook::random(rng).idle();
    Image human{const_cast<Rgba*>(frame.px.data()), SPRITE_W, SPRITE_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (!ExportImage(human, (dir + "/human.png").c_str())) return false;
    FILE* f = std::fopen((dir + "/ref.json").c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "{\"machines\": {");
    for (int k = 0; k < int(Vend::Count); k++) {
        std::fprintf(f, "%s\n  \"%s machine\": [", k ? "," : "", MACHINES[k]);
        for (int i = 0; i < vendingSlots(Vend(k)); i++) {
            int x, y;
            vendingSlotAt(Vend(k), i, x, y);
            std::fprintf(f, "%s[%d, %d]", i ? ", " : "", x, y);
        }
        std::fprintf(f, "]");
    }
    std::fprintf(f, "\n}}\n");
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    if (argc > 2 && !std::strcmp(argv[1], "--ref")) {
        bool ok = writeRef(argv[2]);
        std::printf("%s %s\n", ok ? "wrote the sprite editor's reference to" : "could not write to", argv[2]);
        return ok ? 0 : 1;
    }
    const char* out = argc > 1 ? argv[1] : "build/sprite-sheet.png";
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(SHEET_W, SHEET_H, "sprite sheet");
    bool ok = false;
    {
        Rng rng(3);
        HumanSprites human(HumanLook::random(rng));
        RenderTexture2D target = LoadRenderTexture(SHEET_W, SHEET_H);
        Image icon = makeIcon();
        Texture2D iconTex = LoadTextureFromImage(icon);
        UnloadImage(icon);
        BeginTextureMode(target);
        drawSheet(human, iconTex);
        EndTextureMode();
        Image img = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&img);
        ok = ExportImage(img, out);
        UnloadImage(img);
        UnloadTexture(iconTex);
        UnloadRenderTexture(target);
    }
    CloseWindow();
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
