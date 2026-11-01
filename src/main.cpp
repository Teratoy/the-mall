#include "audio/audio.h"
#include "core/rng.h"
#include "human/human_look.h"
#include "render/canvas.h"
#include "render/icon_art.h"
#include "scenes/menu_scene.h"
#include "scenes/world_scene.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <raylib.h>

namespace {
/// The window first opens at twice the phone canvas, then grows to the wide
/// canvas as large as fits this share of the monitor.
constexpr int WINDOW_SCALE = 2;
constexpr float MONITOR_SHARE = 0.9f;

/// Opens the window wide, for the PC layout, in whole steps where it can.
void fitWindowWide() {
    int monitor = GetCurrentMonitor();
    int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    if (mw <= 0 || mh <= 0) return;
    float s = MONITOR_SHARE * std::min(mw / float(Canvas::WIDE_W), mh / float(Canvas::H));
    if (s >= 2.0f) s = std::floor(s);
    int w = int(Canvas::WIDE_W * s), h = int(Canvas::H * s);
    SetWindowSize(w, h);
    SetWindowPosition((mw - w) / 2, (mh - h) / 2);
}
/// The game opened on a chunk from tools/genview.cpp:
///   --seed N              the world's seed, the human you play grown from it too
///   --chunk KIND x,y;...  the chunk grown past the spawn's door: rooms, maze,
///                         watermaze, plaza or datacentre, on these slots; left
///                         out, the game rolls it
/// Straight into the world, past the menu, and kept out of the obituary.
std::unique_ptr<Scene> tryOut(int argc, char** argv) {
    uint64_t seed = 0;
    bool seeded = false;
    std::optional<ChunkKind> kind;
    Footprint parts;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 10);
            seeded = true;
        } else if (!std::strcmp(argv[i], "--chunk") && i + 2 < argc) {
            const std::string k = argv[++i];
            kind = k == "maze"         ? ChunkKind::Maze
                   : k == "watermaze"  ? ChunkKind::WaterMaze
                   : k == "plaza"      ? ChunkKind::Plaza
                   : k == "datacentre" ? ChunkKind::DataCentre
                   : k == "spawn"      ? ChunkKind::Spawn
                                       : ChunkKind::Rooms;
            for (const char* p = argv[++i]; *p;) {
                char* end = nullptr;
                int x = int(std::strtol(p, &end, 10));
                if (*end != ',') break;
                int y = int(std::strtol(end + 1, &end, 10));
                parts.push_back({x, y});
                p = *end == ';' ? end + 1 : end;
                if (!*end) break;
            }
        }
    }
    if (!seeded) return nullptr;
    Rng looks{seed};
    auto scene = std::make_unique<WorldScene>(HumanLook::random(looks), seed, seed);
    scene->tryOut(kind, parts);
    return scene;
}
} // namespace

int main(int argc, char** argv) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Canvas::W * WINDOW_SCALE, Canvas::H * WINDOW_SCALE, "The Mall");
    fitWindowWide();
    Image icon = makeIcon();
    SetWindowIcon(icon);
    UnloadImage(icon);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    audio::open();
    {
        Canvas canvas;
        std::unique_ptr<Scene> scene = tryOut(argc, argv);
        if (!scene) scene = std::make_unique<MenuScene>();
        while (!WindowShouldClose()) {
            float dt = std::min(GetFrameTime(), 0.1f);
            canvas.fit(scene->canBeWide());
            scene->update(dt, canvas);
            if (scene->next) scene = std::move(scene->next);
            canvas.begin();
            scene->draw(canvas);
            canvas.end();
            BeginDrawing();
            ClearBackground(BLACK);
            canvas.present();
            EndDrawing();
        }
    }
    audio::close();
    CloseWindow();
}
