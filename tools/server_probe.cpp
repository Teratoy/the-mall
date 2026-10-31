// Grows a soak world from its spawn through its doors, as humans would,
// and reports on each chunk: its kind, server rooms, keys, locked doors, toilets and water, so a data
// centre's effect on the humans can be checked. Saves a picture of each
// data centre to build/server-probe-<chunk>.png (genprobe's colours).
//
// Usage: build/server_probe [soak seed] [chunks]
// Build, after make: g++ -std=c++20 -Isrc -Itests tools/server_probe.cpp $(find build -path build/android -prune -o -name "*.o" ! -name main.o -print)
//                    -o build/server_probe $(pkg-config --libs raylib) -lm
#include "human/human_look.h"
#include "probe.h"
#include "world/world.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <raylib.h>

int main(int argc, char** argv) {
    using P = SoakProbe;
    const int soakSeed = argc > 1 ? std::atoi(argv[1]) : 2;
    const int want = argc > 2 ? std::atoi(argv[2]) : 8;
    SetTraceLogLevel(LOG_WARNING);
    const uint64_t seed = uint64_t(soakSeed) * 7919;
    World w(seed);
    Rng looks(seed);
    w.makeSpawn(HumanLook::random(looks), seed);
    // Each time, the first door (in a fixed order) that leads where nothing is yet.
    while (int(w.chunkCount()) < want) {
        std::vector<std::pair<EdgeKey, Door>> doors(P::doors(w).begin(), P::doors(w).end());
        std::sort(doors.begin(), doors.end(), [](const auto& a, const auto& b) {
            Vec2i p = a.first.slot, q = b.first.slot;
            return std::tuple(p.y, p.x, int(a.first.side)) < std::tuple(q.y, q.x, int(b.first.side));
        });
        auto open = std::find_if(doors.begin(), doors.end(), [&](const auto& d) {
            return !w.generated(d.second.slot) || !w.generated(d.second.other());
        });
        if (open == doors.end()) break;
        const Door& via = open->second;
        const Vec2i into = w.generated(via.slot) ? via.other() : via.slot;
        w.ensureGenerated(via.cellsIn(into)[size_t(via.width / 2)]);
    }
    for (const auto& cp : P::chunks(w)) {
        const Chunk& c = *cp;
        int keys = 0, water = 0, locked = 0, rooms = 0, floor = 0, toilets = 0;
        for (Vec2i k : w.keys()) keys += w.chunkIdAt(k) == c.id;
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                Tile t = c.get({x, y});
                water += pooled(t);
                locked += t == Tile::DoorLocked;
                toilets += t == Tile::Toilet;
                floor += t == Tile::Floor;
            }
        for (const ServerRoom& r : w.serverRooms()) rooms += w.chunkIdAt({r.x, r.y}) == c.id;
        std::printf("chunk %2d kind %d slots %d at (%d,%d): %d server rooms, %d keys, %d locked door cells, %d toilets, %d water, "
                    "%d floor\n",
                    c.id, int(c.kind), c.slotCount(), c.slot.x, c.slot.y, rooms, keys, locked, toilets, water, floor);
        if (c.kind != ChunkKind::DataCentre) continue;
        Image img = GenImageColor(c.w, c.h, BLACK);
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                Color col = BLACK;
                switch (c.get({x, y})) {
                case Tile::Floor: col = {200, 190, 170, 255}; break;
                case Tile::Wall: col = {70, 60, 80, 255}; break;
                case Tile::DoorLocked: col = {200, 60, 40, 255}; break;
                case Tile::DoorOpen: col = {230, 160, 40, 255}; break;
                case Tile::Rack: col = {40, 200, 80, 255}; break;
                case Tile::Console: col = {160, 230, 120, 255}; break;
                case Tile::Table: col = {140, 90, 50, 255}; break;
                case Tile::Toilet: col = {250, 250, 250, 255}; break;
                default: if (pooled(c.get({x, y}))) col = {60, 120, 200, 255}; break;
                }
                if (w.hasKey(c.origin() + Vec2i{x, y})) col = {250, 230, 0, 255};
                ImageDrawPixel(&img, x, y, col);
            }
        ImageResizeNN(&img, c.w * 4, c.h * 4);
        std::string path = "build/server-probe-" + std::to_string(c.id) + ".png";
        ExportImage(img, path.c_str());
        UnloadImage(img);
    }
    return 0;
}
