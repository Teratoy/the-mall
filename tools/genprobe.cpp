// Grows chunk interiors on every footprint, turned every way, and checks
// that generation keeps to the footprint: no floor outside it, the border
// whole round it, and the floor on each side of the water all one.
//
// Usage: build/genprobe [seeds] [png]   (or: make genprobe)
//   seeds   how many seeds per footprint and kind (20)
//   png     also save build/genprobe-png/<footprint>-<kind>.png of seed 1,
//           one pixel a cell
//
// It runs the generators alone, without a World: no doors, statues,
// toilets or tables yet. Exits 1 if any check fails.
#include "core/rng.h"
#include "world/gen.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <raylib.h>
#include <set>
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

void save(const Chunk& c, const std::string& path) {
    Image img = GenImageColor(c.w, c.h, BLACK);
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            Color col = BLACK;
            switch (c.get({x, y})) {
            case Tile::Floor: col = {200, 190, 170, 255}; break;
            case Tile::Wall: col = {70, 60, 80, 255}; break;
            case Tile::Water: col = {60, 120, 200, 255}; break;
            case Tile::DoorOpen: col = {230, 160, 40, 255}; break;
            default: break;
            }
            ImageDrawPixel(&img, x, y, col);
        }
    ExportImage(img, path.c_str());
    UnloadImage(img);
}

} // namespace

int main(int argc, char** argv) {
    const int seeds = argc > 1 ? std::atoi(argv[1]) : 20;
    const bool png = argc > 2 && std::strcmp(argv[2], "png") == 0;
    SetTraceLogLevel(LOG_WARNING);
    if (png) mkdir("build/genprobe-png", 0755);
    int failed = 0, made = 0;
    for (const Shape& s : shapes())
        for (ChunkKind kind : {ChunkKind::Rooms, ChunkKind::Maze, ChunkKind::WaterMaze, ChunkKind::Plaza}) {
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
    return failed ? 1 : 0;
}
