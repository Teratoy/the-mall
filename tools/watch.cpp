// Watches one world at the top speed in a phone-shaped window, standing in
// one of several slots side by side, so a few worlds can be watched at
// once. The camera moves on to the nearest living human whenever the one
// it follows dies. Once everyone is dead (or the time cap is reached), it
// stops the world, prints who lived how long and how they died, and
// writes every human's log to build/watched/seed-N.txt. Nothing goes into
// the obituary.
//
// Usage: build/watch [seed] [slot] [slots] [turbo] [cap minutes]
//   turbo: world updates per frame, each at X16 (default 6, so X96).
//   e.g. for i in 0 1 2 3; do build/watch $((100+i)) $i 4 & done
#include "core/rng.h"
#include "human/human.h"
#include "human/human_look.h"
#include "render/canvas.h"
#include "scenes/world_scene.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <raylib.h>
#include <string>

namespace {
/// Share of the monitor's height a window may take.
constexpr float HEIGHT_SHARE = 0.93f;
/// The fastest of WorldScene's speeds.
constexpr int TOP_SPEED = 4;
} // namespace

/// The friend tools/shot.cpp uses; this binary has its own.
struct ShotProbe {
    static void neverRecord(WorldScene& s) { s.recorded_ = true; }
    static void topSpeed(WorldScene& s) { s.speed_ = TOP_SPEED; }
    static World& world(WorldScene& s) { return s.world_; }
    static int follow(const WorldScene& s) { return s.follow_; }
    static void next(WorldScene& s) { s.nextHuman_ = true; }
};

namespace {

/// A log line with @ and # read as the names the world knows.
std::string line(const World& w, const LifeLog::Entry& e) {
    std::string out;
    for (char c : e.what) {
        int id = c == '@' ? e.who : c == '#' ? e.whom : -2;
        if (id == -2) out += c;
        else if (id >= 0 && id < int(w.humans().size())) out += w.human(id).name;
        else out += "SOMEONE";
    }
    if (e.times > 1) out += " (x" + std::to_string(e.times) + ")";
    return out;
}

void report(const World& w, uint64_t seed) {
    std::filesystem::create_directories("build/watched");
    std::string path = "build/watched/seed-" + std::to_string(seed) + ".txt";
    FILE* f = std::fopen(path.c_str(), "w");
    std::printf("== seed %llu: world time %.0f s, %zu humans\n", (unsigned long long)seed, w.time(),
                w.humans().size());
    for (const auto& h : w.humans()) {
        float lived = w.time() - h->born - h->deadFor();
        const auto& es = h->log.entries();
        std::string cause = h->dead() && !es.empty() ? line(w, es.back()) : "ALIVE";
        std::printf("  %-10s lived %5.0f s  %s\n", h->name.c_str(), lived, cause.c_str());
        if (!f) continue;
        std::fprintf(f, "### %s (id %d) lived %.0f s: %s\n", h->name.c_str(), h->id, lived, cause.c_str());
        for (const auto& e : es) std::fprintf(f, "%7.0f  %s\n", e.time - h->born, line(w, e).c_str());
        std::fprintf(f, "\n");
    }
    if (f) std::fclose(f), std::printf("  logs in %s\n", path.c_str());
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv) {
    uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
    int slot = argc > 2 ? std::atoi(argv[2]) : 0;
    int slots = std::max(1, argc > 3 ? std::atoi(argv[3]) : 1);
    int turbo = std::max(1, argc > 4 ? std::atoi(argv[4]) : 6);
    float cap = 60.0f * (argc > 5 ? std::atof(argv[5]) : 240.0f);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(Canvas::W, Canvas::H, TextFormat("The Mall %llu", (unsigned long long)seed));
    int monitor = GetCurrentMonitor();
    int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    float s = std::min(mw / float(slots * Canvas::W), HEIGHT_SHARE * mh / float(Canvas::H));
    int w = int(Canvas::W * s), h = int(Canvas::H * s);
    SetWindowSize(w, h);
    int gap = (mw - slots * w) / (slots + 1);
    SetWindowPosition(gap + slot * (w + gap), (mh - h) / 2);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    {
        Canvas canvas;
        Rng rng(seed * 7919 + 13);
        WorldScene scene(HumanLook::random(rng), rng.next(), seed);
        ShotProbe::neverRecord(scene);
        ShotProbe::topSpeed(scene);
        World& world = ShotProbe::world(scene);
        bool over = false;
        while (!WindowShouldClose()) {
            canvas.fit(false);
            for (int i = 0; i < turbo && !over; i++) {
                scene.update(1.0f / 60.0f, canvas);
                bool anyAlive = false;
                for (const auto& hu : world.humans()) anyAlive |= !hu->dead();
                if (!anyAlive || world.time() >= cap) {
                    over = true;
                    report(world, seed);
                } else if (world.human(ShotProbe::follow(scene)).dead()) {
                    ShotProbe::next(scene);
                }
            }
            if (over) scene.update(0.0f, canvas);
            canvas.begin();
            scene.draw(canvas);
            canvas.end();
            BeginDrawing();
            ClearBackground(BLACK);
            canvas.present();
            EndDrawing();
        }
        if (!over) report(world, seed);
    }
    CloseWindow();
}
