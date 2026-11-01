#pragma once
#include "core/rng.h"
#include "core/vec.h"
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class World;
class Human;

/// What the player hears of the world: what the followed human could hear
/// from where it stands. Footsteps, a pop as each bubble it can see opens,
/// a cry from anyone frightened, and the world's noises (World::noises).
/// Quieter the farther off, muffled through walls, next to nothing while
/// it sleeps, and thinned out the faster the world runs. It only listens:
/// nothing it does reaches the sim.
class Hearing {
public:
    /// Plays what was made since the last call. listener is the followed
    /// human, speed how many times faster than life the world runs, reveal
    /// whether the screen shows everything.
    void update(const World& world, const Human& listener, int speed, bool reveal);
    /// Forgets what it heard, so nothing piled up while away (paused, or
    /// another human followed) plays all at once.
    void skip(const World& world);

private:
    /// A sound waiting to be played this frame, and whether it must play.
    struct Heard {
        const char* name;
        float volume, pan, pitch;
        bool muffled, urgent;
    };
    std::vector<Heard> queue_;
    /// How many times each human's feet had come down when last heard
    /// (footfalls), and the line it was saying.
    std::unordered_map<int, int> feet_;
    std::unordered_map<int, std::string> lines_;
    uint64_t noisesHeard_ = 0;
    /// When each sound that goes on is next played again, in seconds of
    /// real time (GetTime): each human's, and the places'.
    struct Loops {
        double chew = 0.0, pant = 0.0, snore = 0.0;
    };
    std::unordered_map<int, Loops> loops_;
    double fliesAt_ = 0.0, humAt_ = 0.0, waterAt_ = 0.0;
    /// The wobble on each sound, for the ear only, never the sim's dice.
    Rng rng_{0x5a17};

    /// Queues a sound made at cell, loud as loud, reaching reach cells.
    void hear(const World& world, const Human& listener, Vec2i cell, const char* name, float loud, float reach,
              bool urgent, int speed);
    void footsteps(const World& world, const Human& listener, int speed);
    void voices(const World& world, const Human& listener, int speed, bool reveal);
    void noises(const World& world, const Human& listener, int speed);
    void bodies(const World& world, const Human& listener, int speed);
    void places(const World& world, const Human& listener, int speed);
    /// Whether a sound that goes on is due again, and if so when next:
    /// most of its length from now, and extra seconds more.
    bool due(double& next, const char* name, float extra = 0.0f);
};
