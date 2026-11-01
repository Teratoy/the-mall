// Follows the human who wakes in the spawn room until it leaves the room (or
// time runs out) and draws the room with where it walked: so how it goes
// about looking round a room it can mostly see shows.
//
// Usage: build/exploreprobe [seed] [seconds]   (or: make exploreprobe)
//   WHY=1 in the environment also prints, for each target, where it stood,
//   how far it saw and which unknown cells beside the target made it one.
//
// In the map: # wall, + door, T table, S statue, @ where it woke, X where it
// left; a walked cell shows the tenth of the walk it was first stepped on
// in (0-9), and an Explore target it set out for shows as a letter (a, b,
// ... in order). Floor it never saw is ','; seen but not walked, ' '.
#include "core/rng.h"
#include "human/human.h"
#include "human/human_look.h"
#include "../tests/probe.h"
#include "world/world.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

using P = SoakProbe;

int main(int argc, char** argv) {
    uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1;
    float seconds = argc > 2 ? float(std::atof(argv[2])) : 300.0f;
    World world(uint64_t(seed) * 7919);
    Rng rng{seed};
    Human& h = world.makeSpawn(HumanLook::random(rng), rng.next());
    // The room is SPAWN_ROOM across round where it wakes; the map takes in its walls.
    const Vec2i wake = h.cell, lo = wake - Vec2i{29, 29}, hi = wake + Vec2i{30, 30};
    auto inRoom = [&](Vec2i c) { return c.x >= lo.x && c.y >= lo.y && c.x < hi.x && c.y < hi.y; };

    std::vector<Vec2i> walk{h.cell};
    std::vector<std::pair<Vec2i, float>> targets;
    std::map<std::pair<int, int>, int> first;
    std::map<int, float> goalTime;
    const float dt = 1.0f / 30.0f;
    float t = 0.0f, left = -1.0f;
    Human::Goal lastGoal = h.goal;
    Vec2i lastTarget{1 << 30, 0};
    while (t < seconds) {
        world.update(dt);
        t += dt;
        if (h.dead()) break;
        if (h.state == Human::State::Walking) goalTime[int(h.goal)] += dt;
        if (h.state == Human::State::Walking && h.goal == Human::Goal::Explore && P::target(h) != lastTarget) {
            lastTarget = P::target(h);
            targets.push_back({lastTarget, t});
            if (std::getenv("WHY")) {
                std::printf("  t=%.1f at (%d,%d) to (%d,%d) sight %d, unknown beside:", t, h.cell.x - wake.x,
                            h.cell.y - wake.y, lastTarget.x - wake.x, lastTarget.y - wake.y, h.sight.radius());
                for (Vec2i d : DIRS4)
                    if (h.memory.tile(lastTarget + d) == Tile::Void)
                        std::printf(" (%d,%d)=%d", d.x, d.y, int(world.tile(lastTarget + d)));
                std::printf("\n");
            }
        }
        lastGoal = h.goal;
        if (h.cell != walk.back()) walk.push_back(h.cell);
        if (!inRoom(h.cell)) {
            left = t;
            break;
        }
    }
    (void)lastGoal;
    for (size_t i = 0; i < walk.size(); i++) first.emplace(std::pair{walk[i].x, walk[i].y}, int(i * 10 / walk.size()));
    // Walls are seen only when lit or close: unlit, every floor cell along one
    // stays a frontier until it is walked near.
    int walls = 0, litWalls = 0;
    for (int y = lo.y; y < hi.y; y++)
        for (int x = lo.x; x < hi.x; x++)
            if (world.tile({x, y}) == Tile::Wall) {
                bool inner = false;
                for (Vec2i d : DIRS4) inner |= world.tile(Vec2i{x, y} + d) == Tile::Floor;
                walls += inner;
                litWalls += inner && world.light({x, y}) > 0;
            }
    std::printf("inner wall cells lit: %d of %d\n", litWalls, walls);
    int seen = 0, floor = 0;
    for (int y = lo.y; y < hi.y; y++)
        for (int x = lo.x; x < hi.x; x++)
            if (world.tile({x, y}) == Tile::Floor) {
                floor++;
                seen += h.memory.tile({x, y}) != Tile::Void;
            }
    std::printf("seed %llu: %s after %.1f s, %zu steps, %zu explore targets, saw %d of %d floor cells (%d walked)\n",
                (unsigned long long)seed, left >= 0 ? "left the room" : "still in the room", left >= 0 ? left : t,
                walk.size(), targets.size(), seen, floor, int(first.size()));
    std::printf("walking time by goal:");
    static const char* names[] = {"Explore", "FetchKey", "OpenDoor", "PassDoor", "Wander", "Meet", "Follow", "Company",
                                  "FetchCake", "Drink", "Relieve", "Fight", "Flee", "Share", "CheckDoor", "Wash",
                                  "Avoid", "Escort", "Escorted", "Track", "GiveWay", "Loot", "Feed", "Sleep", "Rob",
                                  "Watch", "Rescue", "Fill", "Ask", "Pick", "Stash", "Vend", "Voice", "Inspect"};
    for (auto [g, s] : goalTime) std::printf("  %s %.0fs", names[g], s);
    std::printf("\n");
    std::map<std::pair<int, int>, char> marks;
    for (size_t i = 0; i < targets.size(); i++) {
        Vec2i c = targets[i].first;
        marks[{c.x, c.y}] = char(i < 26 ? 'a' + i : 'A' + (i - 26) % 26);
        std::printf("  %c (%d,%d) at %.1fs\n", marks[{c.x, c.y}], c.x - wake.x, c.y - wake.y, targets[i].second);
    }
    for (int y = lo.y; y < hi.y; y++) {
        for (int x = lo.x; x < hi.x; x++) {
            Vec2i c{x, y};
            char ch;
            Tile tl = world.tile(c);
            if (c == wake) ch = '@';
            else if (left >= 0 && c == walk.back()) ch = 'X';
            else if (auto m = marks.find({x, y}); m != marks.end()) ch = m->second;
            else if (tl == Tile::Wall || tl == Tile::Void) ch = '#';
            else if (tl == Tile::DoorLocked || tl == Tile::DoorOpen) ch = '+';
            else if (world.statue(c)) ch = 'S';
            else if (world.table(c)) ch = 'T';
            else if (auto f = first.find({x, y}); f != first.end()) ch = char('0' + f->second);
            else ch = h.memory.tile(c) == Tile::Void ? ',' : ' ';
            std::putchar(ch);
        }
        std::putchar('\n');
    }
}
