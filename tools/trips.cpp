// How the trips humans make for their needs go: each walk off for water,
// cake, a toilet, sleep or a bottle to fill, how long the way was, and
// whether it got there or turned off on the way, to what, and how far it
// had still to go. Turning back to the same need straight after counts as
// a flip-flop.
// Usage: build/trips [seeds=6] [minutes=15] [crowd=6] [examples=0]
//   The same worlds and crowds as tests/stuck_soak.cpp.
// Then, each time one gets thirsty: how far the nearest water it knows is,
// and how long since it last stood beside water; and what the thirsty who
// know no water spend their time on.
//   TRIPS_TRACE=seed:id prints that human's every change of goal or state,
//   with its needs, the way left and its last log line.
#include "../tests/probe.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <unordered_map>

using P = SoakProbe;
using G = Human::Goal;

static const char* GOAL[] = {"Explore", "FetchKey", "OpenDoor", "PassDoor", "Wander", "Meet", "Follow", "Company", "FetchCake", "Drink", "Relieve", "Fight",
                             "Flee", "Share", "CheckDoor", "Wash", "Avoid", "Escort", "Escorted", "Track", "GiveWay", "Loot", "Feed", "Sleep", "Rob", "Watch", "Rescue", "Fill", "Ask", "Pick", "Stash", "Vend", "Voice", "Inspect", "Bang", "Headbang", "Calm"};
static_assert(std::size(GOAL) == size_t(G::Calm) + 1);

static bool needGoal(G g) {
    return g == G::Drink || g == G::FetchCake || g == G::Relieve || g == G::Sleep || g == G::Fill || g == G::Pick ||
           g == G::Vend;
}

struct Trip {
    bool on = false;
    G goal{};
    size_t startLen = 0, lastLen = 0;
    int steps = 0;
    Vec2i lastCell{};
    float start = 0;
};

struct Tally {
    int trips = 0, done = 0, dropped = 0, flipBack = 0;
    long stepsDone = 0, stepsDropped = 0, lenDone = 0, lenDropped = 0;
    std::map<std::string, int> droppedFor;
};

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    float minutes = argc > 2 ? std::atof(argv[2]) : 15;
    int crowd = argc > 3 ? std::atoi(argv[3]) : 6;
    int examples = argc > 4 ? std::atoi(argv[4]) : 0;
    const float dt = 1.0f / 30.0f;
    int tseed = 0, tid = -1;
    if (const char* tr = std::getenv("TRIPS_TRACE")) std::sscanf(tr, "%d:%d", &tseed, &tid);
    std::map<std::string, Tally> by;
    double walkSec = 0, needWalkSec = 0, aliveSec = 0;
    // Getting thirsty: by steps to water (none, <50, <150, <300, more), and
    // how many of those had stood by water in the minute before.
    int onset[5] = {}, passedBy[5] = {};
    std::map<std::string, double> dryGoals;
    double drySec = 0;
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
        std::unordered_map<int, Trip> trip;
        std::unordered_map<int, std::pair<G, float>> lastDropped;
        std::pair<int, int> traced{-1, -1};
        std::unordered_map<int, float> byWaterAt;
        std::unordered_map<int, bool> wasThirsty;
        std::unordered_map<int, bool> knowsNone, boxedIn;
        float t = 0;
        for (int f = 0; f < int(minutes * 60 / dt); f++) {
            w.update(dt);
            t += dt;
            for (const auto& hp : w.humans()) {
                Human& h = *hp;
                Trip& k = trip[h.id];
                if (s == tseed && h.id == tid && !h.dead() && traced != std::pair{int(h.goal), int(h.state)}) {
                    traced = {int(h.goal), int(h.state)};
                    std::printf("    %6.1f %-9s st%-2d way%-4zu @(%d,%d) hun%.2f thi%.2f tir%.2f bl%.2f water%d | %s\n", t,
                                GOAL[int(h.goal)], int(h.state), P::path(h).size(), h.cell.x, h.cell.y, h.hunger,
                                P::thirstOf(h), h.tiredness, h.bladder, P::waterSteps(w, h),
                                h.log.entries().empty() ? "" : h.log.entries().back().what.c_str());
                }
                bool walking = !h.dead() && h.state == Human::State::Walking;
                if (!h.dead()) aliveSec += dt;
                if (!h.dead() && P::besideWater(h)) byWaterAt[h.id] = t;
                bool thirsty = !h.dead() && h.thirsty();
                if (thirsty && !wasThirsty[h.id]) {
                    int n = P::waterSteps(w, h);
                    int b = n < 0 ? 0 : n < 50 ? 1 : n < 150 ? 2 : n < 300 ? 3 : 4;
                    onset[b]++;
                    auto it = byWaterAt.find(h.id);
                    passedBy[b] += it != byWaterAt.end() && t - it->second < 60;
                    knowsNone[h.id] = n < 0;
                }
                if (!thirsty) knowsNone[h.id] = false;
                wasThirsty[h.id] = thirsty;
                // Rechecked every 5 s while thirsty: still none known.
                if (thirsty && f % 150 == 0) {
                    knowsNone[h.id] = P::waterSteps(w, h) < 0;
                    boxedIn[h.id] = knowsNone[h.id] && !P::knowsFrontier(w, h);
                }
                if (thirsty && knowsNone[h.id]) {
                    drySec += dt;
                    std::string g = h.state == Human::State::Walking ? GOAL[int(h.goal)] : "(not walking)";
                    dryGoals[boxedIn[h.id] ? "boxed in, " + g : g] += dt;
                }
                if (walking && h.moving) {
                    walkSec += dt;
                    if (needGoal(h.goal)) needWalkSec += dt;
                }
                bool same = walking && k.on && h.goal == k.goal;
                if (k.on && !same) {
                    // Ended: got there if the way had all but run out.
                    Tally& y = by[GOAL[int(k.goal)]];
                    y.trips++;
                    if (k.lastLen <= 1) {
                        y.done++, y.stepsDone += k.steps, y.lenDone += long(k.startLen);
                    } else {
                        std::string to = h.dead() ? "dead" : walking ? GOAL[int(h.goal)] : "stopped";
                        y.dropped++, y.stepsDropped += k.steps, y.lenDropped += long(k.startLen);
                        y.droppedFor[to]++;
                        lastDropped[h.id] = {k.goal, t};
                        if (examples > 0) {
                            examples--;
                            std::printf("  s%d t=%6.1f #%d %s dropped %zu/%zu cells left -> %s hun%.2f thi%.2f | %s\n", s, t,
                                        h.id, GOAL[int(k.goal)], k.lastLen, k.startLen, to.c_str(), h.hunger,
                                        P::thirstOf(h), h.log.entries().empty() ? "" : h.log.entries().back().what.c_str());
                        }
                    }
                    k.on = false;
                }
                if (walking && !k.on && needGoal(h.goal)) {
                    // Back to a need it dropped within the last half minute.
                    if (auto it = lastDropped.find(h.id);
                        it != lastDropped.end() && it->second.first == h.goal && t - it->second.second < 30)
                        by[GOAL[int(h.goal)]].flipBack++;
                    k = {true, h.goal, P::path(h).size(), P::path(h).size(), 0, h.cell, t};
                }
                if (k.on) {
                    k.lastLen = P::path(h).size();
                    if (h.cell != k.lastCell) k.steps++, k.lastCell = h.cell;
                }
            }
        }
    }
    std::printf("time walking: %.0f%% of human-seconds; walking for needs: %.0f%% of that\n",
                100.0 * walkSec / aliveSec, 100.0 * needWalkSec / walkSec);
    const char* bucket[] = {"none known", "<50", "50-150", "150-300", "300+"};
    std::printf("getting thirsty, by steps to known water (and how many stood by water in the minute before):\n ");
    for (int b = 0; b < 5; b++) std::printf(" %s %d (%d)", bucket[b], onset[b], passedBy[b]);
    std::printf("\nthirsty knowing no water, %.0f s; spent on:", drySec);
    for (auto& [g, sec] : dryGoals) std::printf(" %s %.0f%%", g.c_str(), 100.0 * sec / drySec);
    std::printf("\n");
    std::printf("%-10s %6s %6s %7s %8s %9s %9s %10s\n", "goal", "trips", "done", "dropped", "flipback", "len done",
                "len drop", "steps lost");
    for (auto& [g, y] : by) {
        std::printf("%-10s %6d %6d %7d %8d %9.1f %9.1f %10ld\n", g.c_str(), y.trips, y.done, y.dropped, y.flipBack,
                    y.done ? double(y.lenDone) / y.done : 0.0, y.dropped ? double(y.lenDropped) / y.dropped : 0.0,
                    y.stepsDropped);
        std::printf("           dropped for:");
        for (auto& [to, n] : y.droppedFor) std::printf(" %s %d", to.c_str(), n);
        std::printf("\n");
    }
}
