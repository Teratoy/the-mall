// Every sprite drawn from data/sprites.txt, at 1x, in all the ways the game
// draws it: the machines at every stock and both blinks, the toilet turned
// to each wall and dirtied, the items and traps in sight and dimmed, ammo boxes
// and spent casings piling up, a sleeper's
// Z's, a human carrying cakes and shooting every way, and the icon. Compare two of these with
// cmp to check a change drew the same, pixel for pixel.
//
// Usage: build/sprite_sheet [out.png]      (make spritesheet)
//        build/sprite_sheet --ref DIR       the sprite editor's human and machine slots (make spriteref)
#include "core/rng.h"
#include "human/human_look.h"
#include "render/dispenser_art.h"
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
    // The drawn ones, as the world draws them, in sight then dimmed.
    int x = GAP;
    for (bool inSight : {true, false}) {
        drawGun(x, y, inSight);
        drawKey(x + 8, y, inSight);
        drawCake(x + 16, y, inSight);
        drawHeapCake(x + 24, y, inSight);
        int tx = x + 32;
        for (TrapKind kind : {TrapKind::Spikes, TrapKind::Slam})
            for (bool sprung : {false, true}) drawTrap(tx, y, kind, sprung, inSight), tx += CELL_PX + GAP;
        for (Vec2i back : {Vec2i{0, -1}, Vec2i{0, 1}, Vec2i{-1, 0}, Vec2i{1, 0}})
            for (float dirt : {0.0f, 1.0f, 3.0f, 8.0f}) drawToilet(tx, y, back, dirt, 0.0f, 7, inSight), tx += CELL_PX + GAP;
        y += CELL_PX + GAP;
    }
    // Ammo boxes shut, then open from full to empty; spent casings piling up
    // on a cell, one to one past the most shown; in sight then dimmed.
    x = GAP;
    for (bool inSight : {true, false}) {
        drawAmmoBox(x, y, false, 1.0f, inSight), x += CELL_PX + GAP;
        for (int left = AMMO_SLOTS; left >= 0; left--)
            drawAmmoBox(x, y, true, float(left) / AMMO_SLOTS, inSight), x += CELL_PX + GAP;
        for (int n = 1; n <= CASINGS_SHOWN + 1; n++) drawCasings(x, y, n, inSight), x += CELL_PX + GAP;
        x += 2 * GAP;
    }
    y += CELL_PX + GAP;
    // Water dispensers at every level the jug shows, from the front and both sides, in sight then dimmed.
    x = GAP;
    for (bool inSight : {true, false})
        for (int view = 0; view < 3; view++)
            for (int level = 0; level <= 6; level++, x += DISPENSER_W + GAP)
                drawDispenser(x, y + DISPENSER_H, float(level) / 6.0f, view > 0, view == 2, inSight);
    y += DISPENSER_H + GAP;
    // Toilets pissed in, yellowing, then shat in too, in sight then dimmed.
    x = GAP;
    for (bool inSight : {true, false}) {
        for (float piss : {0.0f, 0.04f, 0.1f, 0.2f, 0.3f}) drawToilet(x, y, {0, -1}, piss, piss, 7, inSight), x += CELL_PX + GAP;
        drawToilet(x, y, {0, -1}, 1.3f, 0.3f, 7, inSight), x += CELL_PX + 3 * GAP;
    }
    y += CELL_PX + GAP;
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
    // A human shooting every way, facing right then left, aiming then going off.
    y += icon.height + GAP;
    x = GAP + 6;
    for (bool flip : {false, true})
        for (float t : {1.0f, 0.0f})
            for (Pose p : {Pose::ShootUp, Pose::ShootUpAhead, Pose::Shoot, Pose::ShootDownAhead, Pose::ShootDown}) {
                human.draw({float(x + SPRITE_W / 2), float(y + SPRITE_H)}, p, t, flip, 1);
                x += SPRITE_W + 12;
            }
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
