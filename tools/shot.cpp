// A screenshot of the game without a visible window: runs a scene for a while
// in a hidden raylib window and saves what it draws as a PNG.
//
// Usage: build/shot out.png [options]   (or: make shot, to build/shot.png)
//   --scene world|menu|create|obituary   which screen (world)
//   --seconds S    world time to run before the shot (8)
//   --seed N       the world's seed (7); the human you made is seeded 99
//   --zoom Z       world zoom level 0-3 (2)
//   --stats        open the followed human's stats
//   --log          open the followed human's log
//   --reveal       show the whole world, not just what the human knows
//
// The world is never put in the obituary: data/obituary.txt is left alone.
// The image is read back before EndDrawing (after the swap it is black).
#include "core/rng.h"
#include "human/human_look.h"
#include "render/canvas.h"
#include "scenes/create_scene.h"
#include "scenes/menu_scene.h"
#include "scenes/obituary_scene.h"
#include "scenes/world_scene.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <raylib.h>
#include <rlgl.h>
#include <string>

/// Sets up the world scene the way the options ask; a friend of WorldScene.
struct ShotProbe {
    static void neverRecord(WorldScene& s) { s.recorded_ = true; }
    static void set(WorldScene& s, int zoom, bool stats, bool log, bool reveal) {
        s.zoom_ = zoom;
        s.statsOpen_ = stats;
        s.logOpen_ = log && !stats;
        s.reveal_ = reveal;
    }
};

int main(int argc, char** argv) {
    if (argc < 2 || argv[1][0] == '-') {
        std::fprintf(stderr, "usage: %s out.png [--scene world|menu|create|obituary] [--seconds S] [--seed N] "
                             "[--zoom 0-3] [--stats] [--log] [--reveal]\n", argv[0]);
        return 1;
    }
    const char* out = argv[1];
    std::string scene = "world";
    float seconds = 8.0f;
    uint64_t seed = 7;
    int zoom = 2;
    bool stats = false, log = false, reveal = false;
    for (int i = 2; i < argc; i++) {
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", argv[i]);
                std::exit(1);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--scene")) scene = value();
        else if (!std::strcmp(argv[i], "--seconds")) seconds = float(std::atof(value()));
        else if (!std::strcmp(argv[i], "--seed")) seed = std::strtoull(value(), nullptr, 10);
        else if (!std::strcmp(argv[i], "--zoom")) zoom = std::atoi(value());
        else if (!std::strcmp(argv[i], "--stats")) stats = true;
        else if (!std::strcmp(argv[i], "--log")) log = true;
        else if (!std::strcmp(argv[i], "--reveal")) reveal = true;
        else {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(Canvas::W * 2, Canvas::H * 2, "shot");
    bool ok = false;
    {
        Canvas canvas;
        Rng rng(42);
        std::unique_ptr<Scene> s;
        WorldScene* world = nullptr;
        if (scene == "world") {
            auto w = std::make_unique<WorldScene>(HumanLook::random(rng), 99, seed);
            ShotProbe::neverRecord(*w);
            world = w.get();
            s = std::move(w);
        } else if (scene == "menu") {
            s = std::make_unique<MenuScene>();
        } else if (scene == "create") {
            s = std::make_unique<CreateScene>(seed);
        } else if (scene == "obituary") {
            s = std::make_unique<ObituaryScene>();
        } else {
            std::fprintf(stderr, "unknown scene %s\n", scene.c_str());
        }
        if (s) {
            for (int f = 0; f < int(seconds * 60.0f); f++) s->update(1.0f / 60.0f, canvas);
            if (world) ShotProbe::set(*world, zoom, stats, log, reveal);
            canvas.begin();
            s->draw(canvas);
            canvas.end();
            BeginDrawing();
            ClearBackground(BLACK);
            canvas.present();
            rlDrawRenderBatchActive();
            Image img = LoadImageFromScreen();
            EndDrawing();
            ok = ExportImage(img, out);
            UnloadImage(img);
            if (ok) std::printf("wrote %s\n", out);
        }
    }
    CloseWindow();
    return ok ? 0 : 1;
}
