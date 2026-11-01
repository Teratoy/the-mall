// Draws one human shooting all eight ways, as frames of an animation: it
// stands on a floor seen from above and turns from aiming east round to
// aiming north-east, and at each way holds the aim, fires twice, and holds
// it again. Each shot's spent casing flies out to the gun's right and lands
// on the floor, where they pile up on their cells, and a row of ammo boxes
// lies by the wall, shut and then open from full to empty. It goes through
// HumanSprites::draw with the time since the last shot, as in the game.
// With reload, it instead presses RELOAD_ROUNDS rounds into the gun facing
// right, then as many facing left.
// Usage: build/shoot_anim [dir] [seed] [scale] [reload]   (make shootanim)
// writes dir/frame000.png on, one every FRAME_MS; tools/frames_gif.py
// (on the host, which has PIL) joins them into a GIF.
#include "core/rng.h"
#include "human/human_look.h"
#include "render/gun_art.h"
#include "render/human_sprites.h"
#include "world/chunk.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <span>
#include <raylib.h>
#include <string>
#include <sys/stat.h>
#include <utility>

namespace {

/// Milliseconds a frame shows for, which tools/frames_gif.py is told too.
constexpr int FRAME_MS = 50;
/// At each way: holds the aim this long, then fires at these moments, and
/// holds on to the end, in seconds; long enough for the last casing to land.
constexpr float WAY_TIME = 1.6f;
constexpr float SHOTS[] = {0.5f, 0.85f};
/// The floor in cells, the human's cell, and the pixels of the label above.
constexpr int ROOM_W = 9, ROOM_H = 7, LABEL = 14;
constexpr int HUMAN_X = 4, HUMAN_Y = 4;
constexpr Color BACKDROP{58, 56, 62, 255};
constexpr Color FLOOR{92, 88, 80, 255};
constexpr Color GRID{84, 80, 73, 255};
constexpr Color INK{226, 224, 218, 255};

/// A way to shoot: the pose, mirrored or not, and its name.
struct Way {
    Pose pose;
    bool flip;
    const char* name;
};

/// The eight ways round, from east clockwise on screen.
constexpr Way WAYS[] = {
    {Pose::Shoot, false, "East"},          {Pose::ShootDownAhead, false, "South-east"},
    {Pose::ShootDown, false, "South"},     {Pose::ShootDownAhead, true, "South-west"},
    {Pose::Shoot, true, "West"},           {Pose::ShootUpAhead, true, "North-west"},
    {Pose::ShootUp, false, "North"},       {Pose::ShootUpAhead, false, "North-east"},
};

/// Reloading, how many rounds each way, and the seconds a round takes, as
/// HumanSprites draws it.
constexpr int RELOAD_ROUNDS = 3;
constexpr float ROUND_TIME = 1.2f;

/// The ammo boxes along the top row: shut, then open with this much left.
constexpr float BOXES[] = {-1.0f, 1.0f, 0.75f, 0.5f, 0.2f, 0.0f};

} // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "build/shoot-anim";
    uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42;
    int scale = argc > 3 ? std::atoi(argv[3]) : 8;
    bool reload = argc > 4 && std::string(argv[4]) == "reload";
    mkdir(dir.c_str(), 0755);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "shoot anim");
    const int w = ROOM_W * CELL_PX * scale, h = ROOM_H * CELL_PX * scale + LABEL;
    RenderTexture2D rt = LoadRenderTexture(w, h);
    int frames = 0;
    bool ok = true;
    {
        Rng rng(seed);
        HumanLook look = HumanLook::random(rng);
        look.tuckedGun = true;
        HumanSprites human(look);
        const Vector2 feet{HUMAN_X * CELL_PX + CELL_PX / 2.0f, float(HUMAN_Y * CELL_PX + CELL_PX)};
        // Spent casings lying on each cell, as the floor would keep them.
        std::map<std::pair<int, int>, int> casings;
        Camera2D camera{{0, float(LABEL)}, {0, 0}, 0.0f, float(scale)};
        // Draws the floor, what lies on it and the human, as it is now.
        auto frame = [&](const char* label, Pose pose, float time, bool flip) {
            BeginTextureMode(rt);
            ClearBackground(BACKDROP);
            DrawText(label, 6, 2, 10, INK);
            BeginMode2D(camera);
            DrawRectangle(0, 0, ROOM_W * CELL_PX, ROOM_H * CELL_PX, FLOOR);
            for (int cy = 0; cy < ROOM_H; cy++)
                for (int cx = 0; cx < ROOM_W; cx++)
                    if ((cx + cy) % 2) DrawRectangle(cx * CELL_PX, cy * CELL_PX, CELL_PX, CELL_PX, GRID);
            for (auto [cell, n] : casings) drawCasings(cell.first * CELL_PX, cell.second * CELL_PX, n, true);
            for (int b = 0; b < int(std::size(BOXES)); b++)
                drawAmmoBox((1 + b) * CELL_PX, 0, BOXES[b] >= 0, BOXES[b], true);
            human.draw(feet, pose, time, flip, 1);
            EndMode2D();
            EndTextureMode();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            bool done = ExportImage(img, TextFormat("%s/frame%03d.png", dir.c_str(), frames++));
            UnloadImage(img);
            return done;
        };
        if (reload) {
            const int perWay = int(RELOAD_ROUNDS * ROUND_TIME * 1000) / FRAME_MS;
            for (bool flip : {false, true})
                for (int f = 0; f < perWay && ok; f++)
                    ok = frame(TextFormat("Reload, round %d", 1 + int(f * FRAME_MS / 1000.0f / ROUND_TIME)), Pose::Reload,
                               float(f * FRAME_MS) / 1000.0f, flip);
        }
        const int perWay = int(WAY_TIME * 1000) / FRAME_MS;
        for (const Way& way : reload ? std::span<const Way>{} : std::span<const Way>{WAYS})
            for (int f = 0; f < perWay && ok; f++) {
                float t = float(f * FRAME_MS) / 1000.0f, last = t - FRAME_MS / 1000.0f;
                // Time since the last shot; before the first, long enough to hold the aim.
                float since = 1.0f;
                for (float s : SHOTS) {
                    if (t >= s) since = t - s;
                    // Its casing has come to rest: the floor has it now.
                    Vector2 rest;
                    if (t >= s + CASING_TIME && last < s + CASING_TIME &&
                        human.casingRest(feet, way.pose, way.flip, 1, rest))
                        casings[{int(std::floor(rest.x / CELL_PX)), int(std::floor(rest.y / CELL_PX))}]++;
                }
                ok = frame(way.name, way.pose, since, way.flip);
            }
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    std::printf("%s %d frames of %d ms to %s\n", ok ? "wrote" : "could not write", frames, FRAME_MS, dir.c_str());
    return ok ? 0 : 1;
}
