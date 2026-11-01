// Grows the chunks round where the first human wakes and prints how they
// are lit: how many sources of each kind, how much of the floor is lit at
// each level, how long working out the light took, and a map round the
// waker.
//
// Usage: build/lightprobe [seed] [width] [height] [source]   (or: make lightprobe)
//   seed           the world's seed (1)
//   width, height  the map's size in cells round the waker (72 x 40)
//   source         centre the map on this light source instead, counted
//                  from 0 in the order they were put up
//
// In the map: # wall, + door, ~ water, other things as their first letter
// when dark; floor is ' ' dark, then . : * by level; a source's cell shows
// its kind: F fitting, n night light, V vending, S screen, R rack,
// D dispenser, L statue, T torch.
#include "core/rng.h"
#include "human/human.h"
#include "human/human_look.h"
#include "world/world.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace {

char sourceMark(LightKind k) {
    switch (k) {
    case LightKind::Fitting: return 'F';
    case LightKind::NightLight: return 'n';
    case LightKind::Vending: return 'V';
    case LightKind::Screen: return 'S';
    case LightKind::Rack: return 'R';
    case LightKind::Dispenser: return 'D';
    case LightKind::Statue: return 'L';
    case LightKind::Torch: return 'T';
    case LightKind::None: break;
    }
    return '?';
}

const char* kindName(LightKind k) {
    switch (k) {
    case LightKind::Fitting: return "fitting";
    case LightKind::NightLight: return "night light";
    case LightKind::Vending: return "vending";
    case LightKind::Screen: return "screen";
    case LightKind::Rack: return "rack";
    case LightKind::Dispenser: return "dispenser";
    case LightKind::Statue: return "statue";
    case LightKind::Torch: return "torch";
    case LightKind::None: break;
    }
    return "none";
}

char cellMark(const World& w, Vec2i c, const std::map<std::pair<int, int>, LightKind>& sources) {
    if (auto it = sources.find({c.x, c.y}); it != sources.end()) return sourceMark(it->second);
    Tile t = w.tile(c);
    int l = w.light(c);
    switch (t) {
    case Tile::Void: return ' ';
    case Tile::Wall: return '#';
    case Tile::DoorLocked:
    case Tile::DoorOpen: return '+';
    case Tile::Water:
    case Tile::Basin: return '~';
    case Tile::Floor: return " .:*"[l];
    default: return l > 0 ? 'o' : 'x';
    }
}

} // namespace

int main(int argc, char** argv) {
    uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
    int width = argc > 2 ? std::atoi(argv[2]) : 72, height = argc > 3 ? std::atoi(argv[3]) : 40;
    int on = argc > 4 ? std::atoi(argv[4]) : -1;

    auto start = std::chrono::steady_clock::now();
    World world(seed);
    Rng rng(seed);
    Vec2i wake = world.makeSpawn(HumanLook::random(rng), rng.next()).cell;
    Vec2i slot = World::slotOf(wake);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) world.ensureGenerated((slot + Vec2i{x, y}) * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

    std::map<LightKind, int> kinds;
    std::map<std::pair<int, int>, LightKind> sources;
    for (const LightSource& s : world.lights()) {
        kinds[s.kind]++;
        sources[{s.cell.x, s.cell.y}] = s.kind;
    }
    std::printf("seed %llu: %zu chunks, %zu light sources, made in %.1f ms\n", (unsigned long long)seed,
                world.chunkCount(), world.lights().size(), ms);
    for (auto [k, n] : kinds) std::printf("  %-12s %d\n", kindName(k), n);

    int levels[LIGHT_FULL + 1] = {};
    for (int y = (slot.y - 1) * CHUNK; y < (slot.y + 2) * CHUNK; y++)
        for (int x = (slot.x - 1) * CHUNK; x < (slot.x + 2) * CHUNK; x++)
            if (world.tile({x, y}) == Tile::Floor) levels[world.light({x, y})]++;
    int floor = 0;
    for (int n : levels) floor += n;
    std::printf("floor cells by light level:");
    for (int l = 0; l <= LIGHT_FULL; l++) std::printf("  %d: %.1f%%", l, 100.0 * levels[l] / std::max(floor, 1));
    std::printf("\n\n");

    Vec2i at = on >= 0 && on < int(world.lights().size()) ? world.lights()[size_t(on)].cell : wake;

    for (int y = at.y - height / 2; y < at.y + height / 2; y++) {
        for (int x = at.x - width / 2; x < at.x + width / 2; x++)
            std::putchar(Vec2i{x, y} == wake ? '@' : cellMark(world, {x, y}, sources));
        std::putchar('\n');
    }
}
