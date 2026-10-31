// Times the sim alone: one soak seed's world, built the way the soak builds
// it, run in this process with none of the soak's watching, so that the
// time it prints, and gprof's profile of it, are the sim's own.
// Usage: build/sim_profile [seed=4] [minutes=15] [crowd=6]
//   make simprofile times it; make gprof profiles it to build/gprof.txt.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "../tests/probe.h"

using P = SoakProbe;

int main(int argc, char** argv) {
    int s = argc > 1 ? std::atoi(argv[1]) : 4;
    float minutes = argc > 2 ? std::atof(argv[2]) : 15;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 6;
    const float dt = 1.0f / 30.0f;
    World w(uint64_t(s) * 7919);
    Rng rng(s);
    Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
    for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
        Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
        if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
        P::addHuman(w, HumanLook::random(rng), c, rng.next());
        i++;
    }
    int ticks = int(minutes * 60 / dt);
    // Where everyone ends up, so two builds can be seen to run the same.
    uint64_t sum = 0;
    auto start = std::chrono::steady_clock::now();
    for (int f = 0; f < ticks; f++) w.update(dt);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    for (const auto& h : w.humans()) sum = sum * 31 + uint64_t(uint32_t(h->cell.x * 7919 + h->cell.y)) + (h->dead() ? 1 : 0);
    std::printf("seed %d: %d ticks in %.2f s, %.3f ms a tick, %zu humans, %zu chunks, ends %016llx\n", s, ticks, secs,
                secs * 1000.0 / ticks, w.humans().size(), w.chunkCount(), (unsigned long long)sum);
}
