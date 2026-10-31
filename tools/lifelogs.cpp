// Headless: runs a few worlds and writes every human's life log, with a
// status line every few seconds (goal, state, needs, cell), to
// build/lives/seed-N.txt. For reading lives through by eye, looking for
// humans that miss what is plainly in front of them.
//
// Usage: build/lifelogs [seeds=4] [minutes=20] [crowd=4] [every=10]
// Build: g++ -std=c++20 -O2 -Isrc tools/lifelogs.cpp $(objects but main) -lraylib
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include "../tests/probe.h"

using P = SoakProbe;

static const char* GOAL[] = {"Explore", "FetchKey", "OpenDoor", "PassDoor", "Wander", "Meet", "Follow", "Company", "FetchCake", "Drink", "Relieve", "Fight",
                             "Flee", "Share", "CheckDoor", "Wash", "Avoid", "Escort", "Escorted", "Track", "GiveWay", "Loot", "Feed", "Sleep", "Rob", "Watch", "Rescue", "Fill", "Ask", "Pick", "Stash", "Vend", "Voice", "Inspect"};
static const char* STATE[] = {"Waking", "Idle", "Sitting", "Rising", "Walking", "Greeting", "Eating", "Drinking", "Relieving", "Punching", "Giving", "Washing", "Looting", "Feeding", "Sleeping", "Sipping", "Filling", "Pouring", "Stashing", "Vending", "Dead"};
static_assert(std::size(GOAL) == size_t(Human::Goal::Inspect) + 1);
static_assert(std::size(STATE) == size_t(Human::State::Dead) + 1);

static std::string line(const World& w, const LifeLog::Entry& e) {
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

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 4;
    float minutes = argc > 2 ? std::atof(argv[2]) : 20;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 4;
    float every = argc > 4 ? std::atof(argv[4]) : 10;
    const float dt = 1.0f / 30.0f;
    std::filesystem::create_directories("build/lives");
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
        std::map<int, std::vector<std::pair<float, std::string>>> status;
        float next = 0;
        for (int f = 0; f < int(minutes * 60 / dt); f++) {
            w.update(dt);
            if (w.time() < next) continue;
            next += every;
            for (const auto& hp : w.humans()) {
                const Human& h = *hp;
                if (h.dead()) continue;
                char buf[200];
                std::snprintf(buf, sizeof buf, "   . %-9s %-9s hun %.2f thi %.2f tir %.2f bl %.2f bo %.2f hp %.2f cell %d,%d foe %d",
                              GOAL[int(h.goal)], STATE[int(h.state)], h.hunger, h.thirst, h.tiredness, h.bladder,
                              h.bowels, h.health, h.cell.x, h.cell.y, P::foe(h));
                status[h.id].push_back({w.time(), buf});
            }
        }
        std::string path = "build/lives/seed-" + std::to_string(s) + ".txt";
        FILE* out = std::fopen(path.c_str(), "w");
        for (const auto& hp : w.humans()) {
            const Human& h = *hp;
            std::fprintf(out, "### %s (id %d) %s\n", h.name.c_str(), h.id, h.dead() ? "DEAD" : "ALIVE");
            const auto& es = h.log.entries();
            const auto& st = status[h.id];
            size_t a = 0, b = 0;
            while (a < es.size() || b < st.size()) {
                if (b >= st.size() || (a < es.size() && es[a].time <= st[b].first)) {
                    std::fprintf(out, "%7.0f  %s\n", es[a].time, line(w, es[a]).c_str());
                    a++;
                } else {
                    std::fprintf(out, "%7.0f %s\n", st[b].first, st[b].second.c_str());
                    b++;
                }
            }
            std::fprintf(out, "\n");
        }
        std::fclose(out);
        std::printf("seed %d: %zu humans, log in %s\n", s, w.humans().size(), path.c_str());
    }
}
