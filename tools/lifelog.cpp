// Prints the life logs of one soak world, as the soak builds it, and
// optionally what one human was doing every few seconds.
// Usage: build/lifelog seed [minutes=7] [crowd=4] [name] [every=5]
//   With a name, every `every` seconds: its goal, state, needs, health,
//   who it keeps company with, and whether it knows somewhere left to
//   explore, a locked door, water or a toilet it can get to.
#include "../tests/probe.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using P = SoakProbe;

static const char* GOAL[] = {"Explore", "FetchKey", "OpenDoor", "PassDoor", "Wander", "Meet", "Follow", "Company", "FetchCake", "Drink", "Relieve", "Fight",
                             "Flee", "Share", "CheckDoor", "Wash", "Avoid", "Escort", "Escorted", "Track", "GiveWay", "Loot", "Feed", "Sleep", "Rob", "Watch", "Rescue", "Fill", "Ask", "Pick", "Stash", "Vend", "Voice", "Inspect", "Bang", "Headbang", "Calm"};
static const char* STATE[] = {"Waking", "Idle", "Sitting", "Rising", "Walking", "Greeting", "Eating", "Drinking", "Relieving", "Punching", "Giving", "Washing", "Looting", "Feeding", "Sleeping", "Sipping", "Filling", "Pouring", "Stashing", "Vending", "Typing", "Banging", "Headbanging", "Calming", "Dead"};
static_assert(std::size(GOAL) == size_t(Human::Goal::Calm) + 1);
static_assert(std::size(STATE) == size_t(Human::State::Dead) + 1);

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s seed [minutes] [crowd] [name] [every]\n", argv[0]);
        return 1;
    }
    int s = std::atoi(argv[1]);
    float minutes = argc > 2 ? std::atof(argv[2]) : 7;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 4;
    const char* who = argc > 4 ? argv[4] : nullptr;
    float every = argc > 5 ? std::atof(argv[5]) : 5;
    const float dt = 1.0f / 30.0f;
    // The same world and crowd as tests/stuck_soak.cpp.
    World w(uint64_t(s) * 7919);
    Rng rng(s);
    Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
    for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
        Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
        if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
        P::addHuman(w, HumanLook::random(rng), c, rng.next());
        i++;
    }
    float t = 0, nextSample = 0;
    for (int f = 0; f < int(minutes * 60 / dt); f++) {
        w.update(dt);
        t += dt;
        if (!who || t < nextSample) continue;
        nextSample += every;
        for (const auto& h : w.humans()) {
            if (h->name != who || h->dead()) continue;
            int comp = P::companion(*h);
            std::printf("  %6.1f %-8s/%-8s @(%d,%d) hun%.2f thi%.2f hp%.2f keys%d comp=%s front%d door%d water%d toilet%d\n",
                        t, GOAL[int(h->goal)], STATE[int(h->state)], h->cell.x, h->cell.y, h->hunger, P::thirstOf(*h),
                        h->health, P::keysOf(*h), comp >= 0 ? w.human(comp).name.c_str() : "-",
                        P::knowsFrontier(w, *h), P::knowsLockedDoor(w, *h), P::knowsWater(w, *h),
                        P::knowsToilet(w, *h));
        }
    }
    auto nameOf = [&](int id) { return id >= 0 ? w.human(id).name : std::string("?"); };
    for (const auto& h : w.humans()) {
        if (who && h->name != who) continue;
        std::printf("== %s (#%d)%s\n", h->name.c_str(), h->id, h->dead() ? " dead" : "");
        for (const auto& e : h->log.entries()) {
            std::string line = e.what;
            if (auto at = line.find('@'); at != std::string::npos) line.replace(at, 1, nameOf(e.who));
            if (auto at = line.find('#'); at != std::string::npos) line.replace(at, 1, nameOf(e.whom));
            std::printf("  %6.1f %s%s\n", e.time, line.c_str(), e.times > 1 ? (" x" + std::to_string(e.times)).c_str() : "");
        }
    }
}
