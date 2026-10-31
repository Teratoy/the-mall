// Every trap a human sets off in the soak worlds, with the cell, whether it
// knew that cell already, and its health and hunger: tells repeats on one
// plate from new plates of the same kind.
//   build/trap_probe [seeds] [minutes] [crowd]
#include "../tests/probe.h"
#include <cstdio>
#include <cstdlib>
#include <map>

using P = SoakProbe;

static const char* KIND[] = {"spikes", "slam", "darts"};

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    float minutes = argc > 2 ? std::atof(argv[2]) : 15;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 4;
    const float dt = 1.0f / 30.0f;
    for (int s = 1; s <= seeds; s++) {
        World w(uint64_t(s) * 7919);
        Rng rng(s);
        Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
        for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
            Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
            if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
            P::addHuman(w, HumanLook::random(rng), c, rng.next());
            i++;
        }
        // Who hit which trap cell before, by id and cell.
        std::map<std::pair<int, std::pair<int, int>>, int> hits;
        std::map<int, size_t> seen;
        float t = 0;
        for (int f = 0; f < int(minutes * 60 / dt); f++) {
            // What each human knew of traps before this tick.
            std::map<int, std::unordered_map<Vec2i, TrapKind, Vec2iHash>> knew;
            for (const auto& hp : w.humans()) knew[hp->id] = hp->memory.traps;
            std::map<Vec2i, bool, bool (*)(Vec2i, Vec2i)> armed(
                [](Vec2i a, Vec2i b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
            for (const auto& [c, tr] : w.traps()) armed[c] = tr.armed();
            w.update(dt);
            t += dt;
            for (const auto& [c, tr] : w.traps()) {
                if (!armed[c] || tr.armed()) continue;
                // Gone off this tick: by whom? The one whose log says so.
                for (const auto& hp : w.humans()) {
                    const Human& h = *hp;
                    const auto& e = h.log.entries();
                    bool fresh = false;
                    for (auto it = e.rbegin(); it != e.rend() && it->time >= t - dt * 1.5f; ++it)
                        fresh |= it->what == "SET OFF DARTS" || it->what == "STEPPED ON SPIKES" ||
                                 it->what == "SHUT IN A SLAM ROOM";
                    if (!fresh || cheb(h.cell, c) > 1) continue;
                    auto key = std::make_pair(h.id, std::make_pair(c.x, c.y));
                    int n = ++hits[key];
                    bool known = knew[h.id].count(c);
                    int kinds = 0;
                    for (const auto& [kc, kk] : knew[h.id]) kinds += kk == tr.kind;
                    std::printf("s%d t=%6.1f #%d %-10s %-6s at(%d,%d) cell(%d,%d) goal=%d hit#%d knewCell=%d knewSameKind=%d "
                                "health=%.2f hunger=%.2f starving=%d keys=%d\n",
                                s, t, h.id, h.name.c_str(), KIND[int(tr.kind)], c.x, c.y, h.cell.x, h.cell.y,
                                int(h.goal), n, known, kinds, h.health, h.hunger, h.starving(), h.keys);
                }
            }
        }
    }
}
