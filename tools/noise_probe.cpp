// Counts the noises the sim makes for the view to hear (World::noise), by
// kind, over soak-like worlds: to see that each is made at all, and how
// often, before listening for it in the game.
// Usage: build/noise_probe [seeds=6] [minutes=15] [crowd=6]   (make noiseprobe)
#include <array>
#include <cstdio>
#include <cstdlib>
#include "../tests/probe.h"

using P = SoakProbe;

namespace {

constexpr const char* NAMES[] = {"Unlock", "AirlockOpen", "AirlockShut", "Slam", "Spikes", "DartFire", "DartHit",
                                 "Hit", "Death", "Knob", "Spray", "Drip", "Bite", "Swing", "Piss", "Feces", "Gulp",
                                 "Sip", "Collapse", "VendDrop", "VendEmpty", "PickKey", "PickCake", "PickBottle", "Bang"};
constexpr size_t KINDS = std::size(NAMES);
static_assert(KINDS == size_t(NoiseKind::Bang) + 1, "a name for every noise");

} // namespace

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    float minutes = argc > 2 ? float(std::atof(argv[2])) : 15.0f;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 6;
    const float dt = 1.0f / 30.0f;
    std::array<long, KINDS> total{};
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
        uint64_t heard = 0;
        for (int f = 0, ticks = int(minutes * 60 / dt); f < ticks; f++) {
            w.update(dt);
            const auto& all = w.noises();
            uint64_t fresh = std::min<uint64_t>(w.noiseCount() - heard, all.size());
            for (size_t i = all.size() - size_t(fresh); i < all.size(); i++) total[size_t(all[i].kind)]++;
            heard = w.noiseCount();
        }
    }
    for (size_t k = 0; k < KINDS; k++) std::printf("%-12s %ld\n", NAMES[k], total[k]);
}
