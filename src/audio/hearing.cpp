#include "audio/hearing.h"
#include "audio/audio.h"
#include "audio/human_voice.h"
#include "human/human.h"
#include "render/human_sprites.h"
#include "render/world_view.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <raylib.h>

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
/// A line said aloud, at X1: how loud and how far it carries; frightened,
/// it carries as far as a cry, through walls.
constexpr float SAY_LOUD = 0.8f, SAY_REACH = 16.0f, SAY_FEAR_LOUD = 1.0f;
/// Lines made into voices each frame (the rest wait for the next), and
/// how many made voices are kept to say again.
constexpr size_t MADE_PER_FRAME = 3, MADE_KEPT = 48;

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
    {"knob", 0.6f, 10.0f, false},         // Knob
    {"spray", 0.8f, 14.0f, false},        // Spray
    {"drip", 0.25f, 6.0f, false},         // Drip
    {"bite", 0.8f, 12.0f, false},         // Bite
    {"swing", 0.5f, 10.0f, false},        // Swing
    {"piss", 0.5f, 10.0f, false},         // Piss
    {"plop", 0.6f, 10.0f, false},         // Feces
    {"gulp", 0.5f, 9.0f, false},          // Gulp
    {"sip", 0.4f, 8.0f, false},           // Sip
    {"collapse", 0.9f, 16.0f, false},     // Collapse
    {"vend_drop", 0.7f, 14.0f, false},    // VendDrop
    {"vend_empty", 0.6f, 12.0f, false},   // VendEmpty
    {"pick_key", 0.4f, 8.0f, false},      // PickKey
    {"pick_cake", 0.35f, 8.0f, false},    // PickCake
    {"pick_bottle", 0.4f, 8.0f, false},   // PickBottle
    {"bang", 0.9f, 22.0f, false},         // Bang
};
static_assert(std::size(NOISE_SOUNDS) == size_t(NoiseKind::Bang) + 1, "a sound for every noise");

/// A noise is played this many times its size, kept between these.
constexpr float NOISE_SIZE_MIN = 0.2f, NOISE_SIZE_MAX = 2.0f;
/// Piss and feces sound of what they land in: a toilet, water, or the floor.
constexpr const char* PISS_ON[] = {"piss", "piss_toilet", "piss_water"};
constexpr const char* PLOP_ON[] = {"plop", "plop_toilet", "plop_water"};

/// Sounds that go on while something lasts are played over and over, each
/// time before the last has quite faded (this share of its length), so
/// they run together: chewing, panting, snoring, flies, the hum, water.
constexpr float LOOP_OVERLAP = 0.85f;
/// While none is near enough, how often to look again, in seconds.
constexpr float LOOP_RECHECK = 0.5f;
/// Chewing, panting while winded, and snoring once deep asleep (this many
/// seconds after lying down): how loud and how far.
constexpr float CHEW_LOUD = 0.4f, CHEW_REACH = 9.0f;
constexpr float PANT_LOUD = 0.35f, PANT_REACH = 8.0f;
constexpr float SNORE_LOUD = 0.35f, SNORE_REACH = 10.0f, SNORE_AFTER = 3.0f;
/// A snore waits up to this much longer than its length, so sleepers
/// don't snore in step.
constexpr float SNORE_SPREAD = 1.2f;
/// Flies over the nearest rotting body or fresh feces, as loud as its smell
/// is strong.
constexpr float FLIES_LOUD = 0.45f, FLIES_REACH = 8.0f;
/// The server racks' hum and water lapping, from the nearest within reach.
constexpr float HUM_LOUD = 0.35f, HUM_REACH = 12.0f;
constexpr float WATER_LOUD = 0.3f, WATER_REACH = 8.0f;

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

/// The nearest cell within reach of from whose tile is wanted.
template <class Wanted>
std::optional<Vec2i> nearestTile(const World& world, Vec2i from, int reach, Wanted wanted) {
    std::optional<Vec2i> best;
    int bestD = reach * reach + 1;
    for (int dy = -reach; dy <= reach; dy++)
        for (int dx = -reach; dx <= reach; dx++) {
            int d = dx * dx + dy * dy;
            if (d < bestD && wanted(world.tile(from + Vec2i{dx, dy}))) best = from + Vec2i{dx, dy}, bestD = d;
        }
    return best;
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

std::optional<Hearing::Heard> Hearing::heardAs(const World& world, const Human& listener, Vec2i cell, float loud,
                                               float reach, bool urgent, int speed) {
    Vec2i d = cell - listener.cell;
    float dist = std::sqrt(float(d.x * d.x + d.y * d.y));
    float fade = 1.0f - dist / reach;
    if (fade <= 0.0f) return std::nullopt;
    float v = loud * fade * fade;
    int walls = dist > 1.5f ? wallsBetween(world, listener.cell, cell) : 0;
    if (walls > MAX_WALLS) return std::nullopt;
    v *= std::pow(WALL_KEEP, float(walls));
    if (listener.state == Human::State::Sleeping) v *= urgent ? ASLEEP_URGENT_KEEP : ASLEEP_KEEP;
    // Fast, the world would roar: what doesn't have to play grows quieter.
    if (!urgent) v /= std::sqrt(float(speed));
    if (v < QUIET) return std::nullopt;
    float pan = 0.5f - std::clamp(d.x / PAN_CELLS, -1.0f, 1.0f) * PAN_WIDTH;
    return Heard{nullptr, v, pan, 1.0f, walls > 0, urgent};
}

void Hearing::hear(const World& world, const Human& listener, Vec2i cell, const char* name, float loud, float reach,
                   bool urgent, int speed) {
    std::optional<Heard> h = heardAs(world, listener, cell, loud, reach, urgent, speed);
    if (!h) return;
    h->name = name;
    h->pitch = 1.0f + rng_.range(-PITCH_WOBBLE, PITCH_WOBBLE);
    queue_.push_back(*h);
}

// A step is heard as a foot comes down in the walk or run art, so the
// sound keeps time with it; fast, only some are.
void Hearing::footsteps(const World& world, const Human& listener, int speed) {
    for (const auto& h : world.humans()) {
        int now = h->dead() ? -1 : footfalls(poseOf(*h), h->animTime);
        auto [it, fresh] = feet_.try_emplace(h->id, now);
        int before = it->second;
        it->second = now;
        if (fresh || now < 0 || now <= before || (speed > 1 && !rng_.chance(1.0f / float(speed)))) continue;
        bool running = h->running();
        hear(world, listener, h->cell, stepSound(world, h->cell, running), running ? RUN_LOUD : STEP_LOUD, STEP_REACH,
             false, speed);
    }
}

// At X1 a new line is said aloud, in the speaker's voice, if the
// listener could hear it. Faster, one over a head pops, if the player can
// see the bubble, and one said in fear is a cry, heard bubble or not.
void Hearing::voices(const World& world, const Human& listener, int speed, bool reveal) {
    for (const auto& h : world.humans()) {
        const Human::Speech* s = h->dead() ? nullptr : h->speaking();
        std::string line = s ? s->text : std::string();
        auto [it, fresh] = lines_.try_emplace(h->id, line);
        if (fresh || it->second == line) continue;
        it->second = line;
        if (line.empty()) continue;
        bool fear = s->tone == talk::Tone::Fear;
        if (speed == 1) {
            std::optional<Heard> heard = heardAs(world, listener, h->cell, fear ? SAY_FEAR_LOUD : SAY_LOUD,
                                                 fear ? CRY_REACH : SAY_REACH, fear, speed);
            if (heard) said_.push_back({voice::of(*h, s->tone), line, heard->volume, heard->pan, heard->muffled});
        } else if (fear) {
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

// A line said again in the same voice is played from what was made.
void Hearing::speak() {
    for (size_t n = 0; n < MADE_PER_FRAME && !said_.empty(); n++) {
        const Said& s = said_.front();
        const voice::Voice& v = s.voice;
        uint64_t key = 1469598103934665603ULL;
        auto mix = [&](uint64_t x) { key = (key ^ x) * 1099511628211ULL; };
        for (unsigned char c : s.text) mix(c);
        for (float f : {v.pitch, v.speed, v.size, v.brightness, v.variation, v.chirp, v.articulation, v.vowelCue,
                        v.intonation, v.chop, v.shake, v.volume})
            mix(uint64_t(std::lround(double(f) * 1000.0)));
        mix(v.seed), mix(uint64_t(v.kind));
        auto it = made_.find(key);
        if (it == made_.end()) {
            it = made_.emplace(key, voice::speak(v, s.text)).first;
            madeOrder_.push_back(key);
            if (madeOrder_.size() > MADE_KEPT) made_.erase(madeOrder_.front()), madeOrder_.pop_front();
        }
        audio::speak(it->second, s.volume, s.pan, s.muffled);
        said_.pop_front();
    }
}

void Hearing::noises(const World& world, const Human& listener, int speed) {
    const auto& all = world.noises();
    uint64_t fresh = std::min<uint64_t>(world.noiseCount() - noisesHeard_, all.size());
    noisesHeard_ = world.noiseCount();
    for (size_t i = all.size() - size_t(fresh); i < all.size(); i++) {
        const Noise& n = all[i];
        const NoiseSound& ns = NOISE_SOUNDS[size_t(n.kind)];
        const char* name = ns.name;
        if (n.kind == NoiseKind::Piss || n.kind == NoiseKind::Feces) {
            Tile t = world.tile(n.cell);
            int on = t == Tile::Toilet ? 1 : pooled(t) ? 2 : 0;
            name = (n.kind == NoiseKind::Piss ? PISS_ON : PLOP_ON)[on];
        }
        float loud = ns.loud * std::clamp(n.size, NOISE_SIZE_MIN, NOISE_SIZE_MAX);
        hear(world, listener, n.cell, name, loud, ns.reach, ns.urgent, speed);
    }
}

bool Hearing::due(double& next, const char* name, float extra) {
    double now = GetTime();
    if (now < next) return false;
    float len = audio::length(name);
    next = now + (len > 0.0f ? len * LOOP_OVERLAP : LOOP_RECHECK) + extra;
    return true;
}

// Chewing while eating, panting while winded, snoring deep asleep: each
// human's own, played over and over while it lasts.
void Hearing::bodies(const World& world, const Human& listener, int speed) {
    for (const auto& h : world.humans()) {
        Loops& l = loops_[h->id];
        if (h->dead()) continue;
        if (h->state == Human::State::Eating) {
            if (due(l.chew, "chew")) hear(world, listener, h->cell, "chew", CHEW_LOUD, CHEW_REACH, false, speed);
        } else {
            l.chew = 0.0;
        }
        if (h->winded() && h->state != Human::State::Sleeping) {
            if (due(l.pant, "pant")) hear(world, listener, h->cell, "pant", PANT_LOUD, PANT_REACH, false, speed);
        } else {
            l.pant = 0.0;
        }
        if (h->state == Human::State::Sleeping && h->animTime >= SNORE_AFTER) {
            if (due(l.snore, "snore", rng_.range(0.0f, SNORE_SPREAD)))
                hear(world, listener, h->cell, "snore", SNORE_LOUD, SNORE_REACH, false, speed);
        } else {
            l.snore = 0.0;
        }
    }
}

// Flies over what rots, the racks' hum and water lapping, from the nearest
// of each, over and over.
void Hearing::places(const World& world, const Human& listener, int speed) {
    if (due(fliesAt_, "flies")) {
        const Smell* best = nullptr;
        float bestD = FLIES_REACH * FLIES_REACH;
        for (const Smell& s : world.smells()) {
            if (s.kind != SmellKind::Body && s.kind != SmellKind::Feces) continue;
            Vec2i d = s.cell - listener.cell;
            if (float dd = float(d.x * d.x + d.y * d.y); dd <= bestD) best = &s, bestD = dd;
        }
        if (best) hear(world, listener, best->cell, "flies", FLIES_LOUD * best->strength, FLIES_REACH, false, speed);
        else fliesAt_ = GetTime() + LOOP_RECHECK;
    }
    if (due(humAt_, "hum")) {
        if (auto rack = nearestTile(world, listener.cell, int(HUM_REACH), [](Tile t) { return t == Tile::Rack; }))
            hear(world, listener, *rack, "hum", HUM_LOUD, HUM_REACH, false, speed);
        else humAt_ = GetTime() + LOOP_RECHECK;
    }
    if (due(waterAt_, "water")) {
        if (auto water = nearestTile(world, listener.cell, int(WATER_REACH), pooled))
            hear(world, listener, *water, "water", WATER_LOUD, WATER_REACH, false, speed);
        else waterAt_ = GetTime() + LOOP_RECHECK;
    }
}

void Hearing::update(const World& world, const Human& listener, int speed, bool reveal) {
    queue_.clear();
    noises(world, listener, speed);
    bodies(world, listener, speed);
    places(world, listener, speed);
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
    speak();
}

void Hearing::skip(const World& world) {
    noisesHeard_ = world.noiseCount();
    said_.clear();
    for (const auto& h : world.humans()) {
        feet_[h->id] = h->dead() ? -1 : footfalls(poseOf(*h), h->animTime);
        const Human::Speech* s = h->dead() ? nullptr : h->speaking();
        lines_[h->id] = s ? s->text : std::string();
    }
}
