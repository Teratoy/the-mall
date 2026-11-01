// Grows chunk interiors on every footprint, turned every way, and checks
// that generation keeps to the footprint: no floor outside it, the border
// whole round it, and the floor on each side of the water all one.
//
// Usage: build/genprobe [seeds] [png]   (or: make genprobe)
//   seeds   how many seeds per footprint and kind, and how many worlds (20)
//   png     also save build/genprobe-png/<footprint>-<kind>.png of seed 1,
//           one pixel a cell
//
// First it runs the generators alone on every footprint, without a World.
// Then it grows whole worlds, crossing door after door the way humans do,
// and checks in each new chunk that every new door, key and cake can be
// reached from the door it was entered by, locks aside. Last it grows each
// kind on each footprint the game grows it on, beside the spawn, as
// tools/genview.cpp does, and checks those the same way. Exits 1 if any
// check fails.
#include "../tests/probe.h"
#include "chunk_checks.h"
#include "core/rng.h"
#include "human/human_look.h"
#include "world/gen.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <raylib.h>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <sys/stat.h>

namespace {

struct Shape {
    std::string name;
    Footprint parts;
};

/// The footprint turned a quarter clockwise `turns` times, mirrored first
/// if asked, and slid back so its box starts at {0, 0}.
Footprint turned(Footprint parts, int turns, bool mirror) {
    for (Vec2i& p : parts)
        if (mirror) p.x = -p.x;
    for (int t = 0; t < turns; t++)
        for (Vec2i& p : parts) p = {-p.y, p.x};
    Vec2i lo{1 << 20, 1 << 20};
    for (Vec2i p : parts) lo = {std::min(lo.x, p.x), std::min(lo.y, p.y)};
    for (Vec2i& p : parts) p = p - lo;
    std::sort(parts.begin(), parts.end(), [](Vec2i a, Vec2i b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return parts;
}

/// Every distinct turn and mirror of the shapes the plan names, and the
/// boxes there are now.
std::vector<Shape> shapes() {
    const std::vector<Shape> base = {
        {"L3", {{0, 0}, {0, 1}, {1, 1}}},
        {"L4", {{0, 0}, {0, 1}, {0, 2}, {1, 2}}},
        {"T4", {{0, 0}, {1, 0}, {2, 0}, {1, 1}}},
        {"S4", {{1, 0}, {2, 0}, {0, 1}, {1, 1}}},
    };
    std::vector<Shape> out = {{"1x1", boxFootprint({1, 1})}, {"2x1", boxFootprint({2, 1})},
                              {"2x2", boxFootprint({2, 2})}, {"3x1", boxFootprint({3, 1})},
                              {"4x1", boxFootprint({4, 1})}, {"3x3", boxFootprint({3, 3})}};
    for (const Shape& s : base) {
        std::set<std::vector<std::pair<int, int>>> seen;
        for (int m = 0; m < 2; m++)
            for (int t = 0; t < 4; t++) {
                Footprint f = turned(s.parts, t, m == 1);
                std::vector<std::pair<int, int>> key;
                for (Vec2i p : f) key.push_back({p.x, p.y});
                if (!seen.insert(key).second) continue;
                out.push_back({s.name + (m ? "m" : "") + "r" + std::to_string(t), f});
            }
    }
    return out;
}

const char* kindName(ChunkKind k) {
    switch (k) {
    case ChunkKind::Rooms: return "rooms";
    case ChunkKind::Maze: return "maze";
    case ChunkKind::WaterMaze: return "watermaze";
    case ChunkKind::Plaza: return "plaza";
    case ChunkKind::DataCentre: return "datacentre";
    default: return "other";
    }
}

/// What a generated chunk breaks, or empty if nothing.
std::string faults(const Chunk& c) {
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            Vec2i l{x, y};
            Tile t = c.get(l);
            if (!c.owns(l) && t != Tile::Void) return "a cell outside the footprint is not Void";
            if (c.owns(l) && !c.interior(l) && t != Tile::Wall) return "a hole in the border";
            if (c.interior(l) && t == Tile::Void) return "Void inside";
        }
    // Floor on one side of the water is all one, slam rooms through their gates.
    const std::vector<int> side = gen::sides(c);
    std::vector<int> label(size_t(c.w * c.h), -1);
    std::set<int> sidesSeen;
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            size_t i = size_t(y * c.w + x);
            if (c.get({x, y}) != Tile::Floor || label[i] != -1) continue;
            if (!sidesSeen.insert(side[i]).second) return "floor split on one side of the water";
            std::vector<Vec2i> stack{{x, y}};
            label[i] = 0;
            while (!stack.empty()) {
                Vec2i p = stack.back();
                stack.pop_back();
                for (Vec2i d : DIRS4) {
                    Vec2i n = p + d;
                    if (!c.inside(n) || !passable(c.get(n)) || label[size_t(n.y * c.w + n.x)] != -1) continue;
                    label[size_t(n.y * c.w + n.x)] = 0;
                    stack.push_back(n);
                }
            }
        }
    if (sidesSeen.empty()) return "no floor";
    return "";
}

/// Draws a chunk one pixel a cell; with a world, what stands and lies on
/// it too: statues white, tables brown, cakes pink, keys yellow.
void save(const Chunk& c, const std::string& path, const World* w = nullptr) {
    Image img = GenImageColor(c.w, c.h, BLACK);
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            Color col = BLACK;
            switch (c.get({x, y})) {
            case Tile::Floor: col = {200, 190, 170, 255}; break;
            case Tile::Wall: col = {70, 60, 80, 255}; break;
            case Tile::Water: col = {60, 120, 200, 255}; break;
            case Tile::DoorOpen: col = {230, 160, 40, 255}; break;
            case Tile::DoorLocked: col = {200, 60, 40, 255}; break;
            case Tile::Statue: col = {250, 250, 250, 255}; break;
            case Tile::Table: col = {140, 90, 50, 255}; break;
            case Tile::Basin: col = {60, 120, 200, 255}; break;
            case Tile::Vending: col = {120, 220, 200, 255}; break;
            case Tile::Rack: col = {40, 200, 80, 255}; break;
            case Tile::Console: col = {160, 230, 120, 255}; break;
            case Tile::Dispenser: col = {110, 200, 250, 255}; break;
            default: break;
            }
            if (w && w->hasCake(c.origin() + Vec2i{x, y})) col = {250, 80, 160, 255};
            if (w && w->hasKey(c.origin() + Vec2i{x, y})) col = {250, 230, 0, 255};
            ImageDrawPixel(&img, x, y, col);
        }
    ExportImage(img, path.c_str());
    UnloadImage(img);
}

/// Chunks a world grows to, crossing doors.
constexpr int WORLD_CHUNKS = 40;
/// Seeds for each kind and footprint grown as asked, beside the spawn.
constexpr int GIVEN_SEEDS = 5;
/// Data centres grown, and the machines in their corridors.
int centres = 0, centreMachines = 0;


std::string shapeName(const Chunk& c) {
    std::string s = std::to_string(c.slots.x) + "x" + std::to_string(c.slots.y);
    if (c.slotCount() != c.slots.x * c.slots.y) s += "-" + std::to_string(c.slotCount());
    return s;
}

/// Grows a world from its spawn by crossing, each time, the first door (in
/// a fixed order) that leads where nothing is yet. Returns how many chunks
/// broke a check.
int growWorld(uint64_t seed, std::map<std::string, Tally>& tally, bool png) {
    using P = SoakProbe;
    World w(seed);
    Rng looks(seed);
    w.makeSpawn(HumanLook::random(looks), seed);
    int failed = 0;
    while (int(w.chunkCount()) < WORLD_CHUNKS) {
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
        const Vec2i entry = via.cellsIn(into)[size_t(via.width / 2)];

        auto start = std::chrono::steady_clock::now();
        w.ensureGenerated(entry);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        const Chunk& c = *P::chunks(w).back();
        Tally& t = tally[shapeName(c)];
        t.chunks++;
        t.slowest = std::max(t.slowest, ms);

        std::string why = check(w, c, entry, t);
        if (c.kind == ChunkKind::DataCentre) {
            centres++;
            for (const Machine& m : w.machines()) centreMachines += w.chunkIdAt(m.at) == c.id;
        }
        if (png && c.kind == ChunkKind::DataCentre)
            save(c, "build/genprobe-png/world" + std::to_string(seed) + "-datacentre" + std::to_string(c.id) + ".png", &w);
        // Big plazas are rare, so any grown is drawn.
        if (png && c.kind == ChunkKind::Plaza && c.slotCount() >= 9)
            save(c, "build/genprobe-png/world" + std::to_string(seed) + "-plaza" + std::to_string(c.id) + ".png", &w);
        if (!why.empty()) {
            failed++;
            std::printf("FAIL  world %llu chunk %d (%s, kind %d): %s\n", (unsigned long long)seed, c.id,
                        shapeName(c).c_str(), int(c.kind), why.c_str());
        }
    }
    return failed;
}

} // namespace

int main(int argc, char** argv) {
    const int seeds = argc > 1 ? std::atoi(argv[1]) : 20;
    const bool png = argc > 2 && std::strcmp(argv[2], "png") == 0;
    SetTraceLogLevel(LOG_WARNING);
    if (png) mkdir("build/genprobe-png", 0755);
    int failed = 0, made = 0;
    for (const Shape& s : shapes())
        for (ChunkKind kind :
             {ChunkKind::Rooms, ChunkKind::Maze, ChunkKind::WaterMaze, ChunkKind::Plaza, ChunkKind::DataCentre}) {
            const Vec2i box = footprintBox(s.parts);
            if (kind == ChunkKind::Plaza && (box.x != box.y || s.parts.size() != size_t(box.x * box.y))) continue;
            double slowest = 0.0;
            for (int seed = 1; seed <= seeds; seed++) {
                Rng rng(uint64_t(seed) * 7919 + uint64_t(kind));
                Chunk c(0, kind, {0, 0}, s.parts);
                auto start = std::chrono::steady_clock::now();
                if (kind == ChunkKind::Rooms) {
                    gen::rooms(c, rng);
                    gen::connectRegions(c, 3);
                    gen::slamRooms(c, rng, c.slotCount());
                } else if (kind == ChunkKind::DataCentre) {
                    gen::dataCentre(c, rng);
                    gen::connectRegions(c, 3);
                } else if (kind == ChunkKind::Plaza) {
                    // Half with a fountain round an island the size of a grand statue's plinth.
                    gen::plaza(c, rng, seed % 2 ? Vec2i{14, 7} : Vec2i{});
                    gen::connectRegions(c, 3);
                } else {
                    gen::maze(c, rng);
                    gen::connectRegions(c, 3);
                    if (kind == ChunkKind::WaterMaze) gen::flood(c);
                }
                double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                slowest = std::max(slowest, ms);
                made++;
                std::string why = faults(c);
                if (!why.empty()) {
                    failed++;
                    std::printf("FAIL  %-8s %-9s seed %d: %s\n", s.name.c_str(), kindName(kind), seed, why.c_str());
                }
                if (png && seed == 1) save(c, "build/genprobe-png/" + s.name + "-" + kindName(kind) + ".png");
            }
            std::printf("%-8s %-9s slowest %6.1f ms\n", s.name.c_str(), kindName(kind), slowest);
        }
    std::printf("genprobe: %d chunks, %d failed\n", made, failed);

    std::map<std::string, Tally> tally;
    int worldFailed = 0;
    for (int seed = 1; seed <= seeds; seed++) worldFailed += growWorld(uint64_t(seed) * 7919, tally, png);
    int grown = 0;
    for (const auto& [shape, t] : tally) {
        grown += t.chunks;
        std::printf("world %-6s %4d chunks, per chunk %4.1f new doors %4.1f keys %4.1f cakes (%4.1f over water) "
                    "%4.2f machines %4.2f server rooms, slowest %5.1f ms\n",
                    shape.c_str(), t.chunks, double(t.doors) / t.chunks, double(t.keys) / t.chunks,
                    double(t.cakes) / t.chunks, double(t.overWater) / t.chunks, double(t.machines) / t.chunks,
                    double(t.servers) / t.chunks, t.slowest);
    }
    std::printf("world datacentre %d chunks, %4.2f machines each\n", centres,
                centres ? double(centreMachines) / centres : 0.0);
    std::printf("genprobe: %d worlds, %d chunks grown, %d failed\n", seeds, grown, worldFailed);
    failed += worldFailed;

    // Every kind the game grows on every footprint, grown beside the spawn
    // as tools/genview.cpp grows them, checked as in a world.
    int given = 0, givenFailed = 0;
    for (const Shape& s : shapes())
        for (ChunkKind kind :
             {ChunkKind::Rooms, ChunkKind::Maze, ChunkKind::WaterMaze, ChunkKind::Plaza, ChunkKind::DataCentre}) {
            if (!GenViewProbe::grows(kind, s.parts)) continue;
            for (int seed = 1; seed <= std::min(seeds, GIVEN_SEEDS); seed++) {
                World w(uint64_t(seed) * 7919);
                Rng looks{uint64_t(seed)};
                w.makeSpawn(HumanLook::random(looks), uint64_t(seed));
                const Vec2i entry = w.spawnEntry();
                const Chunk* c = GenViewProbe::grow(w, kind, s.parts);
                given++;
                Tally t;
                std::string why = !c ? "no room for it beside the spawn" : c->kind != kind ? "grown as another kind" : "";
                if (why.empty()) why = check(w, *c, entry, t);
                if (!why.empty()) {
                    givenFailed++;
                    std::printf("FAIL  given %-8s %-9s seed %d: %s\n", s.name.c_str(), kindName(kind), seed,
                                why.c_str());
                }
            }
        }
    std::printf("genprobe: %d chunks grown as asked, %d failed\n", given, givenFailed);
    failed += givenFailed;
    return failed ? 1 : 0;
}
