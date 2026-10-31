// Draws one human shooting all eight ways, as frames of an animation: it
// turns from aiming ahead round to aiming up ahead, and at each way holds
// the aim, fires twice, and holds it again. It goes through
// HumanSprites::draw with the time since the last shot, as in the game.
// Usage: build/shoot_anim [dir] [seed] [scale]   (make shootanim)
// writes dir/frame000.png on, one every FRAME_MS; tools/frames_gif.py
// (on the host, which has PIL) joins them into a GIF.
#include "core/rng.h"
#include "human/human_look.h"
#include "render/human_sprites.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>
#include <rlgl.h>
#include <string>
#include <sys/stat.h>

namespace {

/// Milliseconds a frame shows for, which tools/frames_gif.py is told too.
constexpr int FRAME_MS = 50;
/// At each way: holds the aim this long, then fires at these moments, and
/// holds on to the end, in seconds.
constexpr float WAY_TIME = 1.4f;
constexpr float SHOTS[] = {0.5f, 0.85f};
/// Sprite pixels around the human, and the pixels of the label above.
constexpr int ROOM_W = 30, ROOM_H = 28, LABEL = 14;
constexpr Color BACKDROP{58, 56, 62, 255};
constexpr Color FLOOR{92, 88, 80, 255};
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

} // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "build/shoot-anim";
    uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 42;
    int scale = argc > 3 ? std::atoi(argv[3]) : 8;
    mkdir(dir.c_str(), 0755);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "shoot anim");
    const int w = ROOM_W * scale, h = ROOM_H * scale + LABEL;
    RenderTexture2D rt = LoadRenderTexture(w, h);
    int frames = 0;
    bool ok = true;
    {
        Rng rng(seed);
        HumanLook look = HumanLook::random(rng);
        look.tuckedGun = true;
        HumanSprites human(look);
        const int perWay = int(WAY_TIME * 1000) / FRAME_MS;
        for (const Way& way : WAYS)
            for (int f = 0; f < perWay && ok; f++) {
                float t = float(f * FRAME_MS) / 1000.0f;
                // Time since the last shot; before the first, long enough to hold the aim.
                float since = 1.0f;
                for (float s : SHOTS)
                    if (t >= s) since = t - s;
                BeginTextureMode(rt);
                ClearBackground(BACKDROP);
                DrawText(way.name, 6, 2, 10, INK);
                float floorY = float(h - 3 * scale);
                DrawRectangle(0, int(floorY), w, 3 * scale, FLOOR);
                human.draw({w / 2.0f, floorY}, way.pose, since, way.flip, scale);
                EndTextureMode();
                Image img = LoadImageFromTexture(rt.texture);
                ImageFlipVertical(&img);
                ok = ExportImage(img, TextFormat("%s/frame%03d.png", dir.c_str(), frames++));
                UnloadImage(img);
            }
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    std::printf("%s %d frames of %d ms to %s\n", ok ? "wrote" : "could not write", frames, FRAME_MS, dir.c_str());
    return ok ? 0 : 1;
}
