// Headless: how big the life logs grow, now that they keep everything. Runs
// soak-like worlds for a long time and prints, every so often, the entries
// and bytes of the biggest log and of all logs together: in memory (each
// entry plus its text on the heap) and as the obituary would save it (a
// "LOG\t<time>\t<line>\n" row per entry).
//
// Usage: build/log_size [seeds=4] [minutes=120] [crowd=6] [every=15] [first=1]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../tests/probe.h"

using P = SoakProbe;

/// Bytes one entry takes in memory: the entry itself, and its text when it
/// is too long for the string's own buffer (15 characters in libstdc++).
static size_t memBytes(const LifeLog::Entry& e) {
    return sizeof(LifeLog::Entry) + (e.what.size() > 15 ? e.what.capacity() + 1 : 0);
}

/// Bytes one entry takes in the obituary: names stand in for @ and # (taken
/// as 8 letters), the time as about 7 characters.
static size_t saveBytes(const LifeLog::Entry& e) {
    size_t n = 4 + 7 + 1 + e.what.size() + 1;
    for (char c : e.what)
        if (c == '@' || c == '#') n += 7;
    if (e.times > 1) n += 5;
    return n;
}

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 4;
    float minutes = argc > 2 ? std::atof(argv[2]) : 120;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 6;
    float every = argc > 4 ? std::atof(argv[4]) : 15;
    int firstSeed = argc > 5 ? std::atoi(argv[5]) : 1;
    const float dt = 1.0f / 30.0f;
    for (int s = firstSeed; s < firstSeed + seeds; s++) {
        World w(uint64_t(s) * 7919);
        Rng rng(s);
        Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
        for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
            Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
            if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
            P::addHuman(w, HumanLook::random(rng), c, rng.next());
            i++;
        }
        std::printf("seed %d\n  %5s %6s %5s  %7s %7s %7s  %7s %7s %7s\n", s, "min", "humans", "alive", "biggest",
                    "mem", "save", "all", "mem", "save");
        float next = every * 60;
        for (int f = 0; f < int(minutes * 60 / dt); f++) {
            w.update(dt);
            if (w.time() < next && f + 1 < int(minutes * 60 / dt)) continue;
            next += every * 60;
            size_t big = 0, bigMem = 0, bigSave = 0, all = 0, allMem = 0, allSave = 0, alive = 0;
            for (const auto& hp : w.humans()) {
                const auto& es = hp->log.entries();
                size_t mem = 0, save = 0;
                for (const auto& e : es) mem += memBytes(e), save += saveBytes(e);
                if (es.size() > big) big = es.size(), bigMem = mem, bigSave = save;
                all += es.size(), allMem += mem, allSave += save;
                alive += !hp->dead();
            }
            std::printf("  %5.0f %6zu %5zu  %7zu %6zuK %6zuK  %7zu %6zuK %6zuK\n", w.time() / 60,
                        w.humans().size(), alive, big, bigMem / 1024, bigSave / 1024, all, allMem / 1024,
                        allSave / 1024);
            std::fflush(stdout);
        }
    }
}
