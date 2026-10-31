// Draws a sheet of one human beside itself gone dirty, in every pose and
// with everything that rides on the body: a cake carried, a bottle in the
// pocket and in the hand, pointing, blood, feces and piss, and the remains
// eaten and rotten. It all goes through HumanSprites::draw, as in the game,
// so the sheet shows whether the dirty frames still line up with all of it.
// Usage: build/dirty_sheet [out.png] [seed] [scale] [styles|gun|shoot]   (make dirtysheet)
// Each cell is the normal human on the left and the dirty one on the right,
// with what it shows written above. With styles, the sheet is instead one
// row for each hair style, in a few poses, each row a human of its own with
// and without a beard in turn. With gun, it is two rows for each
// bottom (trousers, shorts, skirt), a small body and a big one, each
// with a gun tucked into the waistband. With shoot, it is a small body and
// a big one shooting every way, a row aiming and a row going off; each cell
// is the human facing right and facing left, for all eight ways.
#include "core/rng.h"
#include "human/body_blood.h"
#include "human/bottle.h"
#include "human/human_look.h"
#include "render/human_sprites.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <raylib.h>
#include <rlgl.h>
#include <string>
#include <vector>

namespace {

/// Sprite pixels each human gets, and the gap between the pair, in a cell.
constexpr int SLOT = 22, PAIR_GAP = 2;
/// Cells across a row, and the pixels of the label above each cell.
constexpr int COLUMNS = 8, LABEL = 12;
constexpr Color BACKDROP{58, 56, 62, 255};
constexpr Color FLOOR{92, 88, 80, 255};
constexpr Color INK{226, 224, 218, 255};

/// Which of the human's sprites a cell draws with: clean, or with
/// something on the body.
enum class Coat { Clean, Bloody, Dirty, Eaten, Rotten, Bones, CurledEaten };

struct Cell {
    const char* label;
    Pose pose;
    float time = 0.0f;
    bool carrying = false;
    /// The bottle shows, in the hand when inHand.
    bool bottle = false, inHand = false;
    Coat coat = Coat::Clean;
};

/// A human's sprites with one coat on them, and the blood that coats them.
struct Coated {
    std::unique_ptr<BodyBlood> blood, dirt, soaked;
    std::unique_ptr<HumanSprites> sprites;
};

/// Splashes blood (or dirt) over every pixel of the standing pose that is
/// one of these parts, every few pixels, thick and part dry.
void splash(BodyBlood& b, std::initializer_list<BodyPart> parts, int every, uint8_t amount) {
    int n = 0;
    for (size_t i = 0; i < b.parts.size(); i++) {
        if (b.parts[i] == NO_PART) continue;
        bool on = false;
        for (BodyPart p : parts) on = on || b.parts[i] == uint8_t(p);
        if (!on || n++ % every) continue;
        b.amount[i] = amount;
        b.wet[i] = (n % 3) ? 0.8f : 0.1f;
    }
    b.version++;
}

Coated coat(const HumanLook& look, Coat c) {
    Coated k;
    k.sprites = std::make_unique<HumanSprites>(look);
    switch (c) {
    case Coat::Clean:
        break;
    case Coat::Bloody:
        k.blood = std::make_unique<BodyBlood>(look);
        splash(*k.blood, {BodyPart::Head, BodyPart::FrontArm}, 2, 220);
        splash(*k.blood, {BodyPart::Torso, BodyPart::Feet}, 3, 180);
        k.sprites->bloody(*k.blood);
        break;
    case Coat::Dirty:
        k.dirt = std::make_unique<BodyBlood>(look);
        k.soaked = std::make_unique<BodyBlood>(look);
        splash(*k.dirt, {BodyPart::Legs}, 3, 200);
        splash(*k.soaked, {BodyPart::Legs, BodyPart::Feet}, 1, 200);
        k.sprites->dirty(*k.dirt, *k.soaked);
        break;
    case Coat::Eaten:
        k.sprites->remains(0.55f, 0.15f, 7);
        break;
    case Coat::Rotten:
        k.sprites->remains(0.3f, 0.6f, 7);
        break;
    case Coat::Bones:
        k.sprites->remains(1.0f, 1.0f, 7);
        break;
    case Coat::CurledEaten:
        k.sprites->remains(0.5f, 0.3f, 7, true);
        break;
    }
    return k;
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/dirty-sheet.png";
    uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42;
    int scale = argc > 3 ? std::atoi(argv[3]) : 4;
    bool styles = argc > 4 && std::string(argv[4]) == "styles";
    bool gun = argc > 4 && std::string(argv[4]) == "gun";
    bool shoot = argc > 4 && std::string(argv[4]) == "shoot";
    const std::vector<Cell> stylePoses = {
        {"Idle", Pose::Idle},
        {"Walk 1", Pose::Walk, 0.13f},
        {"Point", Pose::Point},
        {"Sip", Pose::Sip, 0.0f, false, true, true},
        {"Sit", Pose::Sit, 1.0f},
        {"Sleep curl", Pose::Sleep, 3.0f},
        {"Fall down", Pose::Fall, 1.0f},
        {"Bloody", Pose::Idle, 0.0f, false, false, false, Coat::Bloody},
    };
    const std::vector<Cell> gunPoses = {
        {"Idle", Pose::Idle},
        {"Walk 1", Pose::Walk, 0.13f},
        {"Run 1", Pose::Run, 0.0f},
        {"Punch", Pose::Punch},
        {"Winded", Pose::Winded, 0.0f},
        {"Sit", Pose::Sit, 1.0f},
        {"Sleep curl", Pose::Sleep, 3.0f},
        {"Bloody", Pose::Idle, 0.0f, false, false, false, Coat::Bloody},
    };
    // Shooting each way, aiming then going off; the left half faces right,
    // the right half left (drawn flipped).
    const std::vector<Cell> shootPoses = {
        {"Aim up", Pose::ShootUp, 1.0f},
        {"Aim up ahead", Pose::ShootUpAhead, 1.0f},
        {"Aim ahead", Pose::Shoot, 1.0f},
        {"Aim down ahead", Pose::ShootDownAhead, 1.0f},
        {"Aim down", Pose::ShootDown, 1.0f},
        {"Fire up", Pose::ShootUp},
        {"Fire up ahead", Pose::ShootUpAhead},
        {"Fire ahead", Pose::Shoot},
        {"Fire down ahead", Pose::ShootDownAhead},
        {"Fire down", Pose::ShootDown},
    };
    constexpr int BOTTOMS = int(Bottom::Skirt) + 1;
    const std::vector<Cell> cells = styles || gun || shoot ? std::vector<Cell>{} : std::vector<Cell>{
        {"Idle", Pose::Idle},
        {"Walk 1", Pose::Walk, 0.13f},
        {"Walk 3", Pose::Walk, 0.38f},
        {"Run 1", Pose::Run, 0.0f},
        {"Run 2", Pose::Run, 0.08f},
        {"Run 3", Pose::Run, 0.15f},
        {"Winded", Pose::Winded, 0.0f},
        {"Winded in", Pose::Winded, 0.3f},
        {"Wave", Pose::Wave, 0.0f},
        {"Wave out", Pose::Wave, 0.11f},
        {"Eat bite", Pose::Eat, 0.0f},
        {"Eat chew", Pose::Eat, 0.45f},
        {"Guard", Pose::Guard},
        {"Punch", Pose::Punch},
        {"Offer cake", Pose::Offer},
        {"Point", Pose::Point},
        {"PointUp", Pose::PointUp},
        {"PointDown", Pose::PointDown},
        {"Fall kneel", Pose::Fall, 0.2f},
        {"Fall down", Pose::Fall, 1.0f},
        {"Wake open", Pose::Wake, 2.0f},
        {"Sit", Pose::Sit, 1.0f},
        {"Rise", Pose::Rise, 0.1f},
        {"Wash", Pose::Wash},
        {"Search", Pose::Search},
        {"Feed", Pose::Feed, 0.4f},
        {"Squat", Pose::Squat},
        {"Piss", Pose::Piss},
        {"Doze", Pose::Sleep, 2.2f},
        {"Sleep curl", Pose::Sleep, 3.0f},
        {"WakeUp", Pose::WakeUp, 0.1f},
        {"DiedAsleep", Pose::DiedAsleep},
        {"Sip", Pose::Sip, 0.0f, false, true, true},
        {"Fill", Pose::Fill, 0.0f, false, true, true},
        {"Pour", Pose::Pour, 0.3f, false, true, true},
        {"OfferBottle", Pose::OfferBottle, 0.0f, false, true, true},
        {"Carry cake", Pose::Idle, 0.0f, true},
        {"Walk cake", Pose::Walk, 0.13f, true},
        {"Run cake", Pose::Run, 0.0f, true},
        {"Run back cake", Pose::Run, 0.15f, true},
        {"Winded cake", Pose::Winded, 0.0f, true},
        {"Sit cake", Pose::Sit, 1.0f, true},
        {"Curl cake", Pose::Sleep, 3.0f, true},
        {"Pocket", Pose::Idle, 0.0f, false, true},
        {"Walk pocket", Pose::Walk, 0.13f, false, true},
        {"Sit pocket", Pose::Sit, 1.0f, false, true},
        {"Bloody", Pose::Idle, 0.0f, false, false, false, Coat::Bloody},
        {"Bloody point", Pose::Point, 0.0f, false, false, false, Coat::Bloody},
        {"Bloody curl", Pose::Sleep, 3.0f, false, false, false, Coat::Bloody},
        {"Fecal, piss", Pose::Walk, 0.13f, false, false, false, Coat::Dirty},
        {"Eaten", Pose::Fall, 1.0f, false, false, false, Coat::Eaten},
        {"Rotten", Pose::Fall, 1.0f, false, false, false, Coat::Rotten},
        {"Bones", Pose::Fall, 1.0f, false, false, false, Coat::Bones},
        {"Eaten curled", Pose::DiedAsleep, 0.0f, false, false, false, Coat::CurledEaten},
    };
    constexpr int HAIRS = int(Hair::Cap) + 1;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "dirty sheet");
    const int columns = shoot ? SHOOT_AIMS : COLUMNS;
    const int cellW = (2 * SLOT + PAIR_GAP) * scale, cellH = SLOT * scale + LABEL;
    const int rows = styles ? HAIRS : gun ? 2 * BOTTOMS : shoot ? 4 : (int(cells.size()) + COLUMNS - 1) / COLUMNS;
    RenderTexture2D rt = LoadRenderTexture(columns * cellW, rows * cellH);
    bool ok = false;
    {
        Rng rng(seed);
        const Bottle bottle{0.7f};
        BeginTextureMode(rt);
        ClearBackground(BACKDROP);
        // Draws a row of cells for one human, normal and dirty, from cell first on.
        auto drawCells = [&](const HumanLook& normal, const std::vector<Cell>& row, int first, const char* tag) {
            HumanLook grubby = normal;
            grubby.dirty = true;
            Coated coats[2][7];
            for (int f = 0; f < 2; f++)
                for (int c = 0; c < 7; c++) coats[f][c] = coat(f ? grubby : normal, Coat(c));
            for (size_t j = 0; j < row.size(); j++) {
                const Cell& c = row[j];
                int i = first + int(j);
                int ox = i % columns * cellW, oy = i / columns * cellH;
                DrawText(j == 0 && tag ? tag : c.label, ox + 4, oy + 1, 10, INK);
                // The floor under the feet, a sprite pixel from the bottom.
                float floorY = float(oy + cellH - 2 * scale);
                DrawRectangle(ox, int(floorY), cellW - scale, 2 * scale, FLOOR);
                // Shooting, the pair is one human facing right and left.
                for (int f = 0; f < 2; f++) {
                    Vector2 feet{float(ox + (f * (SLOT + PAIR_GAP) + SLOT / 2) * scale), floorY};
                    coats[shoot ? 0 : f][int(c.coat)].sprites->draw(feet, c.pose, c.time, shoot && f == 1, scale, WHITE, c.carrying,
                                                         c.bottle ? &bottle : nullptr, c.inHand);
                }
            }
            // Drawn before the sprites are unloaded, which free their textures.
            rlDrawRenderBatchActive();
        };
        if (styles) {
            const char* names[HAIRS] = {"Bald", "Short", "Long", "Spiky", "Bun", "Mohawk", "Ponytail", "Cap"};
            for (int h = 0; h < HAIRS; h++) {
                HumanLook look = HumanLook::random(rng);
                look.hairStyle = Hair(h);
                look.beard = h % 2 == 1;
                drawCells(look, stylePoses, h * COLUMNS, TextFormat("%s%s", names[h], look.beard ? ", beard" : ""));
            }
        } else if (gun) {
            const char* names[BOTTOMS] = {"Pants", "Shorts", "Skirt"};
            for (int r = 0; r < 2 * BOTTOMS; r++) {
                HumanLook look = HumanLook::random(rng);
                look.tuckedGun = true;
                look.bottomStyle = Bottom(r / 2);
                bool big = r % 2 == 1;
                look.torsoW = big ? 7 : 5;
                look.torsoH = big ? 5 : 4;
                look.legH = big ? 5 : 3;
                drawCells(look, gunPoses, r * COLUMNS, TextFormat("%s, %s", names[r / 2], big ? "big" : "small"));
            }
        } else if (shoot) {
            for (int r = 0; r < 2; r++) {
                HumanLook look = HumanLook::random(rng);
                look.tuckedGun = true;
                bool big = r == 1;
                look.torsoW = big ? 7 : 5;
                look.torsoH = big ? 5 : 4;
                look.legH = big ? 5 : 3;
                drawCells(look, shootPoses, r * 2 * SHOOT_AIMS, nullptr);
            }
        } else {
            drawCells(HumanLook::random(rng), cells, 0, nullptr);
        }
        EndTextureMode();
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        ok = ExportImage(img, out);
        UnloadImage(img);
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    std::printf("%s %s\n", ok ? "wrote" : "could not write", out);
    return ok ? 0 : 1;
}
