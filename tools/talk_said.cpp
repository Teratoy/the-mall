// Headless: runs soak-like worlds and prints every line a human said, from
// the life logs, for tools/talk_counts.py to count which moment follows
// which (the DIAGRAM tab of the lines editor shows the counts).
//
// Usage: build/talk_said [seeds=6] [minutes=15] [crowd=6]   (make talkcounts)
// Prints tab-separated rows: seed, seconds, speaker id, listener id (-1 for
// nobody), the speaker's cell x and y, the words said. The logs are read each
// tick, so each line is put where its speaker stood when it was said.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../tests/probe.h"

using P = SoakProbe;

/// How a said line starts in the life log; it ends TO @ when said to someone.
static const std::string SAID = "SAID \"";

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    float minutes = argc > 2 ? float(std::atof(argv[2])) : 15.0f;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 6;
    const float dt = 1.0f / 30.0f;
    for (int s = 1; s <= seeds; s++) {
        World w(uint64_t(s) * 7919);
        Rng rng{uint64_t(s)};
        Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
        for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
            Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
            if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
            P::addHuman(w, HumanLook::random(rng), c, rng.next());
            i++;
        }
        // How many entries of each human's log are already read; logs only grow.
        std::vector<size_t> read;
        for (int f = 0, ticks = int(minutes * 60 / dt); f < ticks; f++) {
            w.update(dt);
            read.resize(w.humans().size(), 0);
            for (const auto& hp : w.humans()) {
                const auto& es = hp->log.entries();
                for (size_t& i = read[size_t(hp->id)]; i < es.size(); i++) {
                    const auto& e = es[i];
                    if (e.what.rfind(SAID, 0) != 0) continue;
                    size_t end = e.what.rfind('"');
                    if (end <= SAID.size()) continue;
                    bool to = e.what.size() > end + 1;
                    std::string words = e.what.substr(SAID.size(), end - SAID.size());
                    std::printf("%d\t%.2f\t%d\t%d\t%d\t%d\t%s\n", s, e.time, hp->id, to ? e.who : -1, hp->cell.x,
                                hp->cell.y, words.c_str());
                }
            }
        }
    }
}
