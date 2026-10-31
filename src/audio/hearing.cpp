#include "audio/hearing.h"
#include "audio/audio.h"
#include "human/human.h"
#include "render/world_view.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

/// Each wall between keeps this much of a sound, muffled; past
/// MAX_WALLS nothing gets through.
constexpr float WALL_KEEP = 0.45f;
constexpr int MAX_WALLS = 3;
/// Asleep it hears next to nothing; a cry, a slam or a death a little more.
constexpr float ASLEEP_KEEP = 0.08f, ASLEEP_URGENT_KEEP = 0.3f;
/// Cells to the side at which a sound is as far left or right as it goes,
/// and how far that is from the middle (0.5 all the way): gentle, as the
/// phone is narrow.
constexpr float PAN_CELLS = 12.0f, PAN_WIDTH = 0.3f;
/// Each play's pitch is nudged up or down by up to this, so thirty steps
/// don't sound like a machine.
constexpr float PITCH_WOBBLE = 0.06f;
/// At most this many sounds start in one frame, loudest first, but every
/// urgent one plays.
constexpr size_t MAX_PER_FRAME = 6;
/// Sounds too faint to bother with.
constexpr float QUIET = 0.01f;

/// Footsteps: how loud, walking and running, and how far they carry.
constexpr float STEP_LOUD = 0.3f, RUN_LOUD = 0.5f, STEP_REACH = 9.0f;
/// Feet in fresh piss or poured water this fresh splash; in feces, squelch.
constexpr float WET_FRESH = 0.3f, FILTH_FRESH = 0.4f;
/// A bubble's pop: how loud, how far, and the spread of voices' pitches.
constexpr float POP_LOUD = 0.45f, POP_REACH = 16.0f, POP_PITCH_LOW = 0.85f, POP_PITCH_SPREAD = 0.35f;
/// A frightened line is a cry, heard through walls.
constexpr float CRY_LOUD = 0.9f, CRY_REACH = 22.0f;

/// How each of the world's noises sounds: its sound, how loud, how far it
/// carries in cells, and whether it always plays.
struct NoiseSound {
    const char* name;
    float loud, reach;
    bool urgent;
};
constexpr NoiseSound NOISE_SOUNDS[] = {
    {"unlock", 0.7f, 12.0f, false},       // Unlock
    {"airlock_open", 0.6f, 12.0f, false}, // AirlockOpen
    {"airlock_shut", 0.7f, 14.0f, false}, // AirlockShut
    {"slam", 1.0f, 30.0f, true},          // Slam
    {"spikes", 1.0f, 18.0f, true},        // Spikes
    {"dart_fire", 0.6f, 12.0f, false},    // DartFire
    {"dart_hit", 0.8f, 14.0f, false},     // DartHit
    {"hit", 0.9f, 16.0f, false},          // Hit
    {"death", 1.0f, 24.0f, true},         // Death
};

/// The cells that block sight strictly between a and b, along a line.
int wallsBetween(const World& world, Vec2i a, Vec2i b) {
    int dx = std::abs(b.x - a.x), dy = -std::abs(b.y - a.y);
    int sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
    int err = dx + dy, walls = 0;
    Vec2i p = a;
    while (true) {
        int e2 = 2 * err;
        if (e2 >= dy) err += dy, p.x += sx;
        if (e2 <= dx) err += dx, p.y += sy;
        if (p == b) return walls;
        if (opaque(world.tile(p)) && ++walls > MAX_WALLS) return walls;
    }
}

/// Which footstep sound a step onto cell makes.
const char* stepSound(const World& world, Vec2i cell, bool running) {
    if (world.blood().wet(cell)) return "step_blood";
    if (const std::vector<int>* here = world.mess().at(cell)) {
        float now = world.time();
        for (int i : *here) {
            const Dropping& d = world.mess().all()[size_t(i)];
            float fresh = Mess::freshness(d, now);
            if (d.kind == MessKind::Feces && fresh > FILTH_FRESH) return "step_filth";
            if ((d.kind == MessKind::Piss || d.kind == MessKind::Water) && fresh > WET_FRESH) return "step_wet";
        }
    }
    return running ? "run" : "step";
}

} // namespace

void Hearing::hear(const World& world, const Human& listener, Vec2i cell, const char* name, float loud, float reach,
                   bool urgent, int speed) {
    Vec2i d = cell - listener.cell;
    float dist = std::sqrt(float(d.x * d.x + d.y * d.y));
    float fade = 1.0f - dist / reach;
    if (fade <= 0.0f) return;
    float v = loud * fade * fade;
    int walls = dist > 1.5f ? wallsBetween(world, listener.cell, cell) : 0;
    if (walls > MAX_WALLS) return;
    v *= std::pow(WALL_KEEP, float(walls));
    if (listener.state == Human::State::Sleeping) v *= urgent ? ASLEEP_URGENT_KEEP : ASLEEP_KEEP;
    // Fast, the world would roar: what doesn't have to play grows quieter.
    if (!urgent) v /= std::sqrt(float(speed));
    if (v < QUIET) return;
    float pan = 0.5f - std::clamp(d.x / PAN_CELLS, -1.0f, 1.0f) * PAN_WIDTH;
    float pitch = 1.0f + rng_.range(-PITCH_WOBBLE, PITCH_WOBBLE);
    queue_.push_back({name, v, pan, pitch, walls > 0, urgent});
}

// A step is heard as a human comes onto a new cell; fast, only some are.
void Hearing::footsteps(const World& world, const Human& listener, int speed) {
    for (const auto& h : world.humans()) {
        auto [it, fresh] = cells_.try_emplace(h->id, h->cell);
        if (fresh || it->second == h->cell) continue;
        it->second = h->cell;
        if (h->dead() || (speed > 1 && !rng_.chance(1.0f / float(speed)))) continue;
        bool running = h->running();
        hear(world, listener, h->cell, stepSound(world, h->cell, running), running ? RUN_LOUD : STEP_LOUD, STEP_REACH,
             false, speed);
    }
}

// A new line over a head pops, if the player can see the bubble; one said
// in fear is a cry, heard bubble or not.
void Hearing::voices(const World& world, const Human& listener, int speed, bool reveal) {
    for (const auto& h : world.humans()) {
        const Human::Speech* s = h->dead() ? nullptr : h->speaking();
        std::string line = s ? s->text : std::string();
        auto [it, fresh] = lines_.try_emplace(h->id, line);
        if (fresh || it->second == line) continue;
        it->second = line;
        if (line.empty()) continue;
        if (s->tone == talk::Tone::Fear) {
            hear(world, listener, h->cell, "cry", CRY_LOUD, CRY_REACH, true, speed);
        } else if (shown(listener, *h, reveal)) {
            size_t before = queue_.size();
            hear(world, listener, h->cell, "pop", POP_LOUD, POP_REACH, false, speed);
            // Each voice pops at its own pitch.
            if (queue_.size() > before)
                queue_.back().pitch *= POP_PITCH_LOW + POP_PITCH_SPREAD * float(h->id * 37 % 17) / 17.0f;
        }
    }
}

void Hearing::noises(const World& world, const Human& listener, int speed) {
    const auto& all = world.noises();
    uint64_t fresh = std::min<uint64_t>(world.noiseCount() - noisesHeard_, all.size());
    noisesHeard_ = world.noiseCount();
    for (size_t i = all.size() - size_t(fresh); i < all.size(); i++) {
        const Noise& n = all[i];
        const NoiseSound& ns = NOISE_SOUNDS[size_t(n.kind)];
        hear(world, listener, n.cell, ns.name, ns.loud, ns.reach, ns.urgent, speed);
    }
}

void Hearing::update(const World& world, const Human& listener, int speed, bool reveal) {
    queue_.clear();
    noises(world, listener, speed);
    voices(world, listener, speed, reveal);
    footsteps(world, listener, speed);
    // The urgent first, then the loudest, as many as fit.
    std::stable_sort(queue_.begin(), queue_.end(), [](const Heard& a, const Heard& b) {
        return a.urgent != b.urgent ? a.urgent : a.volume > b.volume;
    });
    for (size_t i = 0; i < queue_.size(); i++) {
        const Heard& h = queue_[i];
        if (i >= MAX_PER_FRAME && !h.urgent) break;
        audio::play(h.name, h.volume, h.pan, h.pitch, h.muffled);
    }
}

void Hearing::skip(const World& world) {
    noisesHeard_ = world.noiseCount();
    for (const auto& h : world.humans()) {
        cells_[h->id] = h->cell;
        const Human::Speech* s = h->dead() ? nullptr : h->speaking();
        lines_[h->id] = s ? s->text : std::string();
    }
}
