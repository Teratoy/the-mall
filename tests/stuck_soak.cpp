// Headless soak: watches every human for getting stuck, looping, and
// blocking each other.
// Usage: build/stuck_soak [seeds=6] [minutes=20] [verbose=1] [crowd=0]
//   Each seed runs in its own process, one per core, and prints in seed
//   order, so the output is the same as running them one after another.
//   crowd: extra humans packed round the spawn, to stress doorways.
//   TRACE=seed:id:from:to prints one human second by second.
//   LOGLINES=path writes every distinct log line written, with its count, for
//   tools/coverage.py (make coverage).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include "probe.h"

using P = SoakProbe;

static const char* GOAL[] = {"Explore", "FetchKey", "OpenDoor", "PassDoor", "Wander", "Meet", "Follow", "Company", "FetchCake", "Drink", "Relieve", "Fight",
                             "Flee", "Share", "CheckDoor", "Wash", "Avoid", "Escort", "Escorted", "Track", "GiveWay", "Loot", "Feed", "Sleep", "Rob", "Watch", "Rescue", "Fill", "Ask", "Pick", "Stash"};
static const char* STATE[] = {"Waking", "Idle", "Sitting", "Rising", "Walking", "Greeting", "Eating", "Drinking", "Relieving", "Punching", "Giving", "Washing", "Looting", "Feeding", "Sleeping", "Sipping", "Filling", "Pouring", "Stashing", "Dead"};
// A goal or state added to Human needs its name here too.
static_assert(std::size(GOAL) == size_t(Human::Goal::Stash) + 1);
static_assert(std::size(STATE) == size_t(Human::State::Dead) + 1);

struct Track {
    Vec2i lastCell{};
    float stillFor = 0;       // same cell, not sitting/dead
    float blockedFor = 0;     // Walking and waiting on someone
    std::deque<std::pair<float, Vec2i>> steps; // time of each cell change
    std::deque<std::pair<float, Vec2i>> targets;
    int prevStuck = 0;
    Human::Goal prevGoal = Human::Goal::Explore;
    float reportedStill = -1e9, reportedLoop = -1e9, reportedFlip = -1e9, reportedBlock = -1e9;
    std::map<int, float> goalTime;
};

struct Totals {
    double humanSeconds = 0, asleepSeconds = 0;
    double blockedSeconds = 0;
    int giveUps = 0;
    int giveWays = 0;
    // Going begun, and begun in sight of someone else, erratic or bursting.
    int goes = 0, goesWatched = 0, goesWatchedBursting = 0;
    int mutual = 0;     // frames-episodes where two block each other
    int longBlocks = 0; // blocked >= 5 s in a row
    int stills = 0;     // unmoved >= 30 s, not sitting
    int loops = 0;      // many steps, few cells
    int flips = 0;      // target alternating
    std::map<std::string, int> stillBy, loopBy, blockBy, flipBy;
    std::map<int, double> goalTime, goalBlocked;
    std::set<int> statesSeen;
    // Every log line written, by its text, over all seeds.
    std::map<std::string, int> logLines;
};

/// Writes a seed's totals for the parent to add up. Doubles go as hex so not a bit is lost.
static void writeTotals(FILE* f, const Totals& T) {
    std::fprintf(f, "%a %a %a %d %d %d %d %d %d %d %d %d %d\n", T.humanSeconds, T.asleepSeconds, T.blockedSeconds,
                 T.giveUps, T.giveWays, T.goes, T.goesWatched, T.goesWatchedBursting, T.mutual, T.longBlocks, T.stills,
                 T.loops, T.flips);
    const std::map<std::string, int>* by[] = {&T.stillBy, &T.loopBy, &T.blockBy, &T.flipBy, &T.logLines};
    for (int i = 0; i < int(std::size(by)); i++)
        for (auto& [k, v] : *by[i]) std::fprintf(f, "S%d\t%d\t%s\n", i, v, k.c_str());
    for (auto& [g, v] : T.goalTime) std::fprintf(f, "G %d %a\n", g, v);
    for (auto& [g, v] : T.goalBlocked) std::fprintf(f, "B %d %a\n", g, v);
    for (int st : T.statesSeen) std::fprintf(f, "T %d\n", st);
}

/// Reads a seed's totals back and adds them to T.
static void addTotals(FILE* f, Totals& T) {
    Totals o;
    if (std::fscanf(f, "%la %la %la %d %d %d %d %d %d %d %d %d %d\n", &o.humanSeconds, &o.asleepSeconds,
                    &o.blockedSeconds, &o.giveUps, &o.giveWays, &o.goes, &o.goesWatched, &o.goesWatchedBursting,
                    &o.mutual, &o.longBlocks, &o.stills, &o.loops, &o.flips) != 13) {
        std::fprintf(stderr, "stuck_soak: a seed's totals were cut short\n");
        std::exit(1);
    }
    T.humanSeconds += o.humanSeconds, T.asleepSeconds += o.asleepSeconds, T.blockedSeconds += o.blockedSeconds;
    T.giveUps += o.giveUps, T.giveWays += o.giveWays, T.goes += o.goes, T.goesWatched += o.goesWatched;
    T.goesWatchedBursting += o.goesWatchedBursting, T.mutual += o.mutual, T.longBlocks += o.longBlocks;
    T.stills += o.stills, T.loops += o.loops, T.flips += o.flips;
    std::map<std::string, int>* by[] = {&T.stillBy, &T.loopBy, &T.blockBy, &T.flipBy, &T.logLines};
    char line[4096];
    while (std::fgets(line, sizeof line, f)) {
        line[std::strcspn(line, "\n")] = 0;
        int i = 0, n = 0, g = 0;
        double v = 0;
        char* key = nullptr;
        if (line[0] == 'S' && std::sscanf(line, "S%d\t%d\t", &i, &n) == 2 && i >= 0 && i < int(std::size(by)) &&
            (key = std::strchr(line + 1, '\t')) && (key = std::strchr(key + 1, '\t')))
            (*by[i])[key + 1] += n;
        else if (std::sscanf(line, "G %d %la", &g, &v) == 2)
            T.goalTime[g] += v;
        else if (std::sscanf(line, "B %d %la", &g, &v) == 2)
            T.goalBlocked[g] += v;
        else if (std::sscanf(line, "T %d", &g) == 1)
            T.statesSeen.insert(g);
    }
}

/// Runs seeds in child processes, as many at once as there are cores. Each child prints into its own file and
/// leaves its totals in another; they are copied out and added up in seed order, so the output doesn't depend on
/// which seed finishes first.
struct SeedPool {
    struct Child {
        pid_t pid;
        FILE* out;
        FILE* totals;
    };
    std::deque<Child> running;
    size_t cores = std::max(1L, sysconf(_SC_NPROCESSORS_ONLN));
    bool child = false;
    Child mine{};

    /// In the parent, forks a child for the next seed and returns false. In the child, returns true with T
    /// emptied and stdout going to the child's file.
    bool start(Totals& T) {
        while (running.size() >= cores) collectOne(T);
        std::fflush(stdout);
        Child c{0, std::tmpfile(), std::tmpfile()};
        if (!c.out || !c.totals || (c.pid = fork()) < 0) {
            std::perror("stuck_soak");
            std::exit(1);
        }
        if (c.pid == 0) {
            child = true;
            mine = c;
            T = Totals{};
            dup2(fileno(c.out), STDOUT_FILENO);
            return true;
        }
        running.push_back(c);
        return false;
    }

    /// In the child, hands its totals over and ends it.
    [[noreturn]] void finish(const Totals& T) {
        std::fflush(stdout);
        writeTotals(mine.totals, T);
        std::fflush(mine.totals);
        _exit(0);
    }

    /// Waits for the oldest child, prints what it printed and adds its totals to T.
    void collectOne(Totals& T) {
        Child c = running.front();
        running.pop_front();
        int status = 0;
        waitpid(c.pid, &status, 0);
        std::rewind(c.out);
        char buf[1 << 16];
        for (size_t n; (n = std::fread(buf, 1, sizeof buf, c.out)) > 0;) std::fwrite(buf, 1, n, stdout);
        std::fflush(stdout);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            std::fprintf(stderr, "stuck_soak: a seed's process failed (status %d)\n", status);
            std::exit(1);
        }
        std::rewind(c.totals);
        addTotals(c.totals, T);
        std::fclose(c.out);
        std::fclose(c.totals);
    }

    void collectAll(Totals& T) {
        while (!running.empty()) collectOne(T);
    }
};

static std::string label(const Human& h) {
    char b[200];
    std::snprintf(b, sizeof b, "#%d %s/%s @(%d,%d) tgt(%d,%d) path%zu partner%d leader%d foe%d keys%d cakes%d hunger%.2f",
                  h.id, STATE[int(h.state)], GOAL[int(h.goal)], h.cell.x, h.cell.y, P::target(h).x, P::target(h).y,
                  P::path(h).size(), P::partner(h), P::leader(h), P::foe(h), h.keys, h.cakes, h.hunger);
    return b;
}

int main(int argc, char** argv) {
    int seeds = argc > 1 ? std::atoi(argv[1]) : 6;
    float minutes = argc > 2 ? std::atof(argv[2]) : 20;
    int verbose = argc > 3 ? std::atoi(argv[3]) : 1;
    int crowd = argc > 4 ? std::atoi(argv[4]) : 0;
    const char* trace = std::getenv("TRACE");
    int tseed = 0, tid = 0;
    float tfrom = 0, tto = 0;
    if (trace) std::sscanf(trace, "%d:%d:%f:%f", &tseed, &tid, &tfrom, &tto);
    const float dt = 1.0f / 30.0f;
    Totals T;
    SeedPool pool;
    for (int s = 1; s <= seeds; s++) {
        if (!pool.start(T)) continue;
        World w(uint64_t(s) * 7919);
        Rng rng(s);
        Human& first = w.makeSpawn(HumanLook::random(rng), rng.next());
        for (int i = 0, tries = 0; i < crowd && tries < 1000; tries++) {
            Vec2i c = first.cell + Vec2i{rng.range(-8, 8), rng.range(-8, 8)};
            if (w.tile(c) != Tile::Floor || w.occupant(c) || w.table(c) || w.statue(c)) continue;
            P::addHuman(w, HumanLook::random(rng), c, rng.next());
            i++;
        }
        std::unordered_map<int, Track> tr;
        float t = 0;
        int examples = 0;
        std::map<std::pair<int, int>, float> mutualAt;
        auto note = [&](const char* what, const Human& h, const std::string& extra) {
            if (verbose && examples < 25) {
                std::printf("  s%d t=%6.1f %-7s %s %s\n", s, t, what, label(h).c_str(), extra.c_str());
                examples++;
            }
        };
        for (int f = 0; f < int(minutes * 60 / dt); f++) {
            w.update(dt);
            t += dt;
            for (const auto& hp : w.humans()) {
                Human& h = *hp;
                T.statesSeen.insert(int(h.state));
                if (h.dead()) continue;
                Track& k = tr[h.id];
                T.humanSeconds += dt;
                T.asleepSeconds += h.state == Human::State::Sleeping ? dt : 0;
                T.goalTime[int(h.goal)] += dt;
                if (h.state == Human::State::Relieving && h.animTime == 0.0f) {
                    T.goes++;
                    bool seen = false;
                    for (const auto& o : w.humans()) seen |= o.get() != &h && !o->dead() && o->sight.sees(h.cell);
                    T.goesWatched += seen;
                    T.goesWatchedBursting += seen && (h.isErratic() || std::max(h.bladder, h.bowels) >= 0.99f);
                }
                if (h.cell != k.lastCell || f == 0) {
                    k.steps.push_back({t, h.cell});
                    k.lastCell = h.cell;
                    k.stillFor = 0;
                } else if (h.state != Human::State::Waking && h.state != Human::State::Sitting &&
                           h.state != Human::State::Rising &&
                           h.state != Human::State::Washing && h.state != Human::State::Drinking && h.state != Human::State::Relieving &&
                           h.state != Human::State::Looting && h.state != Human::State::Sipping &&
                           h.state != Human::State::Filling && h.state != Human::State::Pouring &&
                           h.state != Human::State::Stashing &&
                           h.state != Human::State::Feeding && h.state != Human::State::Sleeping) {
                    k.stillFor += dt;
                } else {
                    k.stillFor = 0;
                }
                while (!k.steps.empty() && k.steps.front().first < t - 60) k.steps.pop_front();
                if (k.targets.empty() || k.targets.back().second != P::target(h)) k.targets.push_back({t, P::target(h)});
                while (!k.targets.empty() && k.targets.front().first < t - 30) k.targets.pop_front();

                // Waiting on someone in the way.
                bool blocked = h.state == Human::State::Walking && !h.moving && !P::path(h).empty();
                Human* by = blocked ? w.occupant(P::path(h).front()) : nullptr;
                if (by == &h) by = nullptr;
                blocked = blocked && by;
                if (blocked) {
                    k.blockedFor += dt;
                    T.blockedSeconds += dt;
                    T.goalBlocked[int(h.goal)] += dt;
                    if (by->state == Human::State::Walking && !by->moving && !P::path(*by).empty() &&
                        P::path(*by).front() == h.cell && h.id < by->id && k.blockedFor <= dt * 1.5f) {
                        T.mutual++;
                        float& last = mutualAt[{h.id, by->id}];
                        if (t - last > 60) note("MUTUAL", h, "<-> " + label(*by));
                        last = t;
                    }
                    if (k.blockedFor >= 5 && t - k.reportedBlock > 30) {
                        k.reportedBlock = t;
                        T.longBlocks++;
                        T.blockBy[std::string(GOAL[int(h.goal)]) + " by " + STATE[int(by->state)] + "/" + GOAL[int(by->goal)]]++;
                        note("BLOCK5s", h, "by " + label(*by));
                    }
                } else {
                    k.blockedFor = 0;
                }
                if (k.prevStuck >= 4 && P::stuck(h) == 0 && h.state == Human::State::Idle && !h.moving) T.giveUps++;
                k.prevStuck = P::stuck(h);
                if (trace && s == tseed && h.id == tid && t >= tfrom && t <= tto &&
                    (h.goal != k.prevGoal || h.cell != k.steps.back().second || f % 15 == 0))
                    std::printf("    T %.2f %s rethink%d stuck%d grime%.2f blood%d dirt%d piss%d | %s\n", t,
                                label(h).c_str(), P::rethink(h), P::stuck(h), h.grime(), h.stains.any(), h.dirt.any(),
                                h.soaked.any(), h.log.entries().empty() ? "" : h.log.entries().back().what.c_str());
                if (h.goal == Human::Goal::GiveWay && k.prevGoal != Human::Goal::GiveWay) T.giveWays++;
                k.prevGoal = h.goal;

                if (k.stillFor >= 30 && t - k.reportedStill > 60) {
                    k.reportedStill = t;
                    T.stills++;
                    T.stillBy[std::string(STATE[int(h.state)]) + "/" + GOAL[int(h.goal)]]++;
                    note("STILL30", h, "");
                }
                if (k.steps.size() >= 25 && t > 60) {
                    std::set<std::pair<int, int>> distinct;
                    for (auto& [_, c] : k.steps) distinct.insert({c.x, c.y});
                    if (distinct.size() <= 8 && t - k.reportedLoop > 60) {
                        k.reportedLoop = t;
                        T.loops++;
                        T.loopBy[GOAL[int(h.goal)]]++;
                        note("LOOP", h, std::to_string(k.steps.size()) + " steps over " + std::to_string(distinct.size()) + " cells");
                    }
                }
                if (k.targets.size() >= 8) {
                    std::set<std::pair<int, int>> distinct;
                    for (auto& [_, c] : k.targets) distinct.insert({c.x, c.y});
                    if (distinct.size() <= 3 && t - k.reportedFlip > 60) {
                        k.reportedFlip = t;
                        T.flips++;
                        T.flipBy[GOAL[int(h.goal)]]++;
                        note("FLIP", h, std::to_string(k.targets.size()) + " target changes among " + std::to_string(distinct.size()));
                    }
                }
            }
        }
        int alive = 0, looted = 0, empty = 0, hoardLessons = 0, keyLessons = 0, starved = 0;
        // Thirst: parched spells, drinks begun, and deaths by cause.
        // Going: pisses, shits, and how many of them anyone else could see.
        // Going unwashed: turned dirty, washed for it, washed clean, dirty at the end.
        int turnedDirty = 0, washedDirt = 0, washedClean = 0, dirtyEnd = 0;
        // Smell: started to smell, smelt death unseen, followed it, walked off to eat.
        int stank = 0, smeltDeath = 0, followedStench = 0, putOff = 0;
        int pisses = 0, shits = 0, stepped = 0, filthy = 0, washedShit = 0, foulSick = 0, foulGulps = 0, toiletUses = 0, toiletMisses = 0, toiletWaits = 0;
        // Accidents (bursting, fright), going seen, going in water seen, and mocking.
        // Guarded water: let drink free, for a cake, turned away, paid, cheated.
        // Water tips: told, over spikes, asked of the group.
        int waterTold = 0, waterLied = 0, waterAsked = 0, ledToDrink = 0, nearWater = 0;
        int waterFree = 0, waterPrice = 0, waterNo = 0, waterPaid = 0, waterCheat = 0, homesByWater = 0;
        int wet = 0, soiled = 0, frightWet = 0, seenGoing = 0, seenAccident = 0, inWater = 0, mocked = 0;
        // Sleep: chosen, dropped where it stood, woken by a blow.
        int sleeps = 0, collapses = 0, wokeHurt = 0, exhausted = 0, safeLessons = 0;
        // Danger asleep: robbed unseen, caught robbing, blamed, hit, woken by noise, died.
        int robbed = 0, caught = 0, blamed = 0, hitAsleep = 0, wokeNoise = 0, diedAsleep = 0;
        // Watch: taken, asked (and refused, paid), alarms, turns, slept on it, robbed on it, blamed for it.
        int watches = 0, asked = 0, refused = 0, paid = 0, alarms = 0, turns = 0, sleptOnWatch = 0, betrayed = 0, failedWatch = 0;
        int parched = 0, drinks = 0, diedThirst = 0, diedStarved = 0, diedBled = 0, diedOther = 0;
        // Flesh: meals of it, rotten ones, hunts started and killed, the
        // steady brave and the erratic among the eaters, minds broken by
        // guilt, minds snapped, and who saw it.
        int meals = 0, rottenMeals = 0, hunts = 0, kills = 0, braveEaters = 0, erraticEaters = 0, broke = 0,
            snapped = 0, sawEat = 0;
        // Kills: by a last blow, of a beating bled out in sight, and found
        // later by the one whose beating did it.
        int killedBlow = 0, killedBled = 0, killFound = 0;
        for (auto& h : w.humans()) {
            alive += !h->dead();
            for (auto& e : h->log.entries()) T.logLines[e.what] += e.times;
            for (auto& e : h->log.entries())
                if (e.what.find("'S BODY") != std::string::npos && e.what.rfind("ATE ", 0) != 0 &&
                    e.what.find("HALF EATEN") == std::string::npos) {
                    looted += e.times;
                    empty += e.what.find("NOTHING") != std::string::npos ? e.times : 0;
                }
            for (auto& e : h->log.entries()) {
                hoardLessons += e.what == "LEARNT TO KEEP MORE CAKES" ? e.times : 0;
                keyLessons += e.what == "LEARNT TO TAKE KEYS FIRST" ? e.times : 0;
                starved += e.what == "STARVING" ? e.times : 0;
                meals += e.what.rfind("ATE FROM", 0) == 0 ? e.times : 0;
                rottenMeals += e.what == "ATE FROM @'S ROTTEN BODY" ? e.times : 0;
                hunts += e.what == "WENT FOR @ TO EAT THEM" ? e.times : 0;
                kills += e.what == "KILLED @ TO EAT" ? e.times : 0;
                killedBlow += e.what == "KILLED BY @" ? e.times : 0;
                killedBled += e.what == "KILLED @" || e.what == "KILLED @ TO EAT" ? e.times : 0;
                killFound += e.what == "FOUND @ DEAD. MY BEATING DID IT" ? e.times : 0;
                broke += e.what == "THE GUILT IS TOO MUCH" ? e.times : 0;
                snapped += e.what == "SOMETHING IN ME SNAPPED" ? e.times : 0;
                sawEat += e.what == "SAW @ EATING #" ? e.times : 0;
                parched += e.what == "PARCHED" ? e.times : 0;
                sleeps += e.what == "FELL ASLEEP" ? e.times : 0;
                collapses += e.what == "COLLAPSED FROM EXHAUSTION" ? e.times : 0;
                wokeHurt += e.what == "WOKE UP HURT" ? e.times : 0;
                exhausted += e.what == "EXHAUSTED" ? e.times : 0;
                safeLessons += e.what == "LEARNT TO FIND A SAFE PLACE TO SLEEP IN TIME" ? e.times : 0;
                robbed += e.what == "ROBBED @ IN THEIR SLEEP" ? e.times : 0;
                caught += e.what == "@ WOKE UP AS I ROBBED THEM" ? e.times : 0;
                blamed += e.what == "@ MUST HAVE TAKEN IT" ? e.times : 0;
                hitAsleep += e.what == "HIT @ IN THEIR SLEEP" ? e.times : 0;
                wokeNoise += e.what == "WOKE UP TO A NOISE" ? e.times : 0;
                watches += e.what == "KEEPING WATCH OVER @" ? e.times : 0;
                asked += e.what == "ASKED @ TO KEEP WATCH" ? e.times : 0;
                refused += e.what == "WOULDN'T KEEP WATCH OVER @" ? e.times : 0;
                paid += e.what == "GAVE @ A CAKE TO KEEP WATCH" ? e.times : 0;
                alarms += e.what == "WOKE THE OTHERS: @ WAS COMING" ? e.times : 0;
                turns += e.what == "WOKE @ TO TAKE A TURN ON WATCH" ? e.times : 0;
                sleptOnWatch += e.what == "FELL ASLEEP ON WATCH" ? e.times : 0;
                betrayed += e.what == "ROBBED @ ON MY WATCH" || e.what == "@ ROBBED ME ON THEIR WATCH" ? e.times : 0;
                failedWatch += e.what == "@ WAS MEANT TO KEEP WATCH" ? e.times : 0;
                drinks += e.what == "DRANK WATER" ? e.times : 0;
                pisses += e.what == "PISSED" ? e.times : 0;
                shits += e.what == "TOOK A SHIT" ? e.times : 0;
                stepped += e.what == "STEPPED IN SHIT" ? e.times : 0;
                washedShit += e.what == "WASHED OFF SHIT" ? e.times : 0;
                turnedDirty += e.what == "TURNED DIRTY" ? e.times : 0;
                washedDirt += e.what == "WASHED OFF DIRT" ? e.times : 0;
                washedClean += e.what == "WASHED CLEAN" ? e.times : 0;
                stank += e.what == "STARTED TO SMELL" ? e.times : 0;
                smeltDeath += e.what == "SMELT DEATH" ? e.times : 0;
                followedStench += e.what == "FOLLOWED THE SMELL OF DEATH" ? e.times : 0;
                putOff += e.what == "MOVED AWAY FROM A SMELL TO EAT" ? e.times : 0;
                foulSick += e.what == "THE FOUL WATER MADE ME SICK" ? e.times : 0;
                foulGulps += e.what == "DRANK FOUL WATER" ? e.times : 0;
                toiletUses += e.what == "USED A TOILET" ? e.times : 0;
                toiletMisses += e.what == "MISSED THE TOILET" ? e.times : 0;
                toiletWaits += e.what == "WAITED FOR THE TOILET" ? e.times : 0;
                wet += e.what == "WET MYSELF" ? e.times : 0;
                waterFree += e.what == "LET @ DRINK MY WATER" ? e.times : 0;
                waterTold += e.what == "TOLD @ THE WAY TO WATER" ? e.times : 0;
                ledToDrink += e.what == "TOOK @ TO WATER TO DRINK" ? e.times : 0;
                nearWater += e.what == "LEARNT TO KEEP NEAR WATER" ? e.times : 0;
                waterLied += e.what == "TOLD @ A WAY TO WATER OVER SPIKES" ? e.times : 0;
                waterAsked += e.what.find("WATER? ANY IDEA") != std::string::npos ||
                                      e.what.find("KNOW WHERE THERE'S WATER?") != std::string::npos ||
                                      e.what.find("SEEN ANY WATER") != std::string::npos ||
                                      e.what.find("WATER... PLEASE") != std::string::npos
                                  ? e.times
                                  : 0;
                waterPrice += e.what == "ASKED @ A CAKE FOR MY WATER" ? e.times : 0;
                waterNo += e.what == "TURNED @ AWAY FROM MY WATER" ? e.times : 0;
                waterPaid += e.what == "PAID @ A CAKE FOR WATER" ? e.times : 0;
                waterCheat += e.what == "DRANK @'S WATER AND DIDN'T PAY" ? e.times : 0;
                homesByWater += e.what == "GAVE UP ON A WAY OUT. MADE A HOME BY THE WATER" ? e.times : 0;
                soiled += e.what == "SOILED MYSELF" ? e.times : 0;
                frightWet += e.what == "WET MYSELF IN FRIGHT" ? e.times : 0;
                seenGoing += e.what == "SOMEONE SAW ME GO" ? e.times : 0;
                seenAccident += e.what == "SOMEONE SAW MY ACCIDENT" ? e.times : 0;
                inWater += e.what == "SAW @ GO IN THE WATER" ? e.times : 0;
                mocked += e.what == "LAUGHED AT @ FOR WETTING THEMSELVES" ? e.times : 0;
            }
            filthy += !h->dead() && h->filth() >= 0.15f;
            dirtyEnd += !h->dead() && h->dirty();
            diedAsleep += h->dead() && h->diedAsleep();
            if (h->dead()) {
                bool thirst = false, starve = false, bled = false;
                for (auto& e : h->log.entries()) {
                    thirst |= e.what == "DIED OF THIRST";
                    starve |= e.what == "STARVED TO DEATH";
                    bled |= e.what == "BLED TO DEATH";
                }
                (thirst ? diedThirst : starve ? diedStarved : bled ? diedBled : diedOther)++;
            }
            if (P::ateFlesh(*h)) (h->appetite() > h->guilt() ? erraticEaters : braveEaters)++;
        }
        std::printf("seed %d: %zu humans (%d alive), %zu chunks, %d bodies gone through (%d empty), "
                    "starving spells %d, lessons: %d hoard, %d keys first\n",
                    s, w.humans().size(), alive, w.chunkCount(), looted, empty, starved, hoardLessons, keyLessons);
        std::printf("   flesh: %d meals (%d rotten), %d hunts, %d killed to eat, eaters %d guilty / %d craving, "
                    "%d broken by guilt, %d snapped, %d saw it\n",
                    meals, rottenMeals, hunts, kills, braveEaters, erraticEaters, broke, snapped, sawEat);
        std::printf("   kills: %d by a last blow, %d known at once, %d learnt at the body\n", killedBlow,
                    killedBled, killFound);
        std::printf("   thirst: %d parched spells, %d drinks; died of thirst %d, starved %d, bled %d, other %d\n", parched,
                    drinks, diedThirst, diedStarved, diedBled, diedOther);
        std::printf("   sleep: %d slept, %d collapsed, %d exhausted spells, %d woken by a blow, %d learnt to sleep safe\n",
                    sleeps, collapses, exhausted, wokeHurt, safeLessons);
        std::printf("   asleep: %d robbed unseen (%d blamed someone), %d caught robbing, %d hit, %d woken by noise, %d died\n",
                    robbed, blamed, caught, hitAsleep, wokeNoise, diedAsleep);
        std::printf("   watch: %d kept, %d asked (%d refused, %d paid), %d alarms, %d turns, %d slept on it, %d robbed on it, %d blamed\n",
                    watches, asked, refused, paid, alarms, turns, sleptOnWatch, betrayed, failedWatch);
        std::printf("   going: %d pissed, %d shat, %zu left lying; %d stepped in it, %d filthy at the end, "
                    "%d washed it off, %d sick from foul water\n",
                    pisses, shits, w.mess().all().size(), stepped, filthy, washedShit, foulSick);
        std::printf("   dirty: %d turned dirty, %d washed for it, %d washed clean, %d dirty at the end\n", turnedDirty,
                    washedDirt, washedClean, dirtyEnd);
        std::printf("   smell: %d started to smell, %d smelt death unseen, %d followed it, %d walked off to eat\n", stank,
                    smeltDeath, followedStench, putOff);
        {
            // How foul the water got: cells foul at all, past refusing, past sickness, and the worst.
            std::map<Vec2i, float, bool (*)(Vec2i, Vec2i)> f([](Vec2i a, Vec2i b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
            for (auto& [c, v] : w.mess().fouled()) f[c] += v;
            for (auto& [c, v] : w.blood().fouled()) f[c] += v;
            int any = 0, refuse = 0, sick = 0;
            float most = 0;
            for (auto& [c, v] : f) any++, refuse += v >= 0.3f, sick += v >= 0.5f, most = std::max(most, v);
            float dirtiest = 0;
            for (auto& [c, t] : w.toilets()) dirtiest = std::max(dirtiest, t.dirt);
            std::printf("   water tips: %d told, %d over spikes, %d asks of the group said, %d led to drink; %d learnt to "
                        "keep near water\n",
                        waterTold, waterLied, waterAsked, ledToDrink, nearWater);
            std::printf("   guarded water: %d homes by water, %d let drink, %d asked a cake (%d paid, %d cheated), %d turned "
                        "away\n",
                        homesByWater, waterFree, waterPrice, waterPaid, waterCheat, waterNo);
            std::printf("   accidents: %d wet, %d soiled, %d wet in fright; seen going %d, accident seen %d, "
                        "going in water seen %d, mocked %d\n",
                        wet, soiled, frightWet, seenGoing, seenAccident, inWater, mocked);
            std::printf("   toilets: %zu, %d uses, %d misses, dirtiest %.2f, %d waits\n", w.toilets().size(), toiletUses,
                        toiletMisses, dirtiest, toiletWaits);
            std::printf("   water: %d cells foul, %d >= 0.3, %d >= 0.5, worst %.2f; %d foul gulps\n", any, refuse, sick,
                        most, foulGulps);
        }
        pool.finish(T);
    }
    pool.collectAll(T);
    std::printf("\n== %.0f human-minutes ==\n", T.humanSeconds / 60);
    std::printf("asleep %.1f%% of the time\n", 100 * T.asleepSeconds / T.humanSeconds);
    std::printf("made way %d times; blocked %.2f%% of time, %d give-ups, %d mutual blocks, %d blocks >=5s\n",
                T.giveWays, 100 * T.blockedSeconds / T.humanSeconds, T.giveUps, T.mutual, T.longBlocks);
    std::printf("went %d times, %d in someone's sight (%d of those erratic or bursting)\n", T.goes, T.goesWatched,
                T.goesWatchedBursting);
    std::printf("unmoved >=30s (not sitting): %d   loops (25+ steps on <=8 cells in 60s): %d   target flips: %d\n",
                T.stills, T.loops, T.flips);
    auto dump = [](const char* n, const std::map<std::string, int>& m) {
        if (m.empty()) return;
        std::printf("%s:", n);
        for (auto& [k, v] : m) std::printf("  %s=%d", k.c_str(), v);
        std::printf("\n");
    };
    dump("still by", T.stillBy);
    dump("loop by", T.loopBy);
    dump("flip by", T.flipBy);
    dump("long block by", T.blockBy);
    std::printf("goals never seen:");
    for (int g = 0; g < int(std::size(GOAL)); g++)
        if (!T.goalTime.count(g)) std::printf(" %s", GOAL[g]);
    std::printf("\nstates never seen:");
    for (int st = 0; st < int(std::size(STATE)); st++)
        if (!T.statesSeen.count(st)) std::printf(" %s", STATE[st]);
    std::printf("\n");
    std::printf("goal time (blocked share):");
    for (auto& [g, v] : T.goalTime)
        std::printf("  %s %.1f%%(%.0f%%)", GOAL[g], 100 * v / T.humanSeconds, 100 * T.goalBlocked[g] / v);
    std::printf("\n");
    if (const char* path = std::getenv("LOGLINES")) {
        if (FILE* f = std::fopen(path, "w")) {
            for (auto& [what, n] : T.logLines) std::fprintf(f, "%d\t%s\n", n, what.c_str());
            std::fclose(f);
        }
    }
}
