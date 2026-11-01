#include "audio/human_voice.h"
#include "human/human.h"
#include <algorithm>
#include <cmath>

namespace voice {

namespace {

/// Hz of the smallest voice at rest, and how far it is lowered by a bigger
/// body (each sprite pixel of torso width, height and leg above the
/// smallest) and by a beard, as a share of the pitch. High and small, for
/// a cartoon rather than a person.
constexpr float PITCH = 420.0f, PITCH_PER_PIXEL = 0.06f, BEARD_LOWER = 0.18f;
/// The throat's size for the smallest body, and how much bigger each
/// pixel makes it (a smaller number is a bigger throat).
constexpr float SIZE = 1.28f, SIZE_PER_PIXEL = 0.035f;
/// A little quicker than speech, and its clarity, expression and breath
/// before the traits.
constexpr float SPEED = 1.15f, CLARITY = 0.6f, EXPRESSION = 0.7f, BREATH = 0.5f;
/// Below this much hope, a plain line is said sadly.
constexpr float GLOOM_SAD = 0.5f;
/// What the traits do, from the middle (0.5) to either end: brave lowers
/// the pitch and steadies it; sociable speeds and clears it; greedy
/// hardens the consonants and clears it; curious lifts the phrase.
constexpr float BRAVE_LOWER = 0.16f, BRAVE_STEADY = 0.35f;
constexpr float SOCIABLE_FASTER = 0.3f, SOCIABLE_BRIGHTER = 0.3f;
constexpr float GREEDY_HARDER = 0.3f, GREEDY_BRIGHTER = 0.2f;
constexpr float CURIOUS_LILT = 0.5f;
/// The seeded wobble on top, so two of a build don't sound alike: octaves
/// of pitch either way, and speed either way.
constexpr float PITCH_SPREAD = 0.2f, SPEED_SPREAD = 0.1f;

/// What each tone does to the voice: the emotion it speaks with (which
/// moves pitch, range and pace itself), then pitch and speed times, shake,
/// breath and clarity added, expression times, loudness times.
struct ToneShade {
    english::Emotion emotion;
    float pitch, speed, shake, breath, clarity, lilt, loud;
};
constexpr ToneShade TONE_SHADES[] = {
    {english::Emotion::Calm, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},          // Plain
    {english::Emotion::Warm, 1.02f, 0.97f, 0.0f, 0.1f, -0.1f, 1.1f, 0.9f},       // Warm
    {english::Emotion::Stern, 0.95f, 1.08f, 0.0f, -0.15f, 0.2f, 1.0f, 1.15f},    // Rude
    {english::Emotion::Worried, 1.04f, 0.9f, 0.15f, 0.1f, 0.0f, 1.2f, 0.95f},    // Plead
    {english::Emotion::Worried, 1.15f, 1.2f, 0.6f, 0.1f, 0.1f, 1.3f, 1.25f},     // Fear
    {english::Emotion::Commanding, 0.92f, 0.95f, 0.0f, -0.2f, 0.2f, 1.0f, 1.2f}, // Threat
};
/// Dread at full makes the voice tremble this much; all hope gone makes
/// it this much slower, lower and quieter.
constexpr float DREAD_SHAKE = 0.35f, GLOOM_SLOWER = 0.25f, GLOOM_LOWER = 0.12f, GLOOM_QUIETER = 0.3f;
/// Loudness of a plain line, and the loudest any line may be.
constexpr float VOLUME = 0.7f, VOLUME_MAX = 0.95f;

uint64_t hashOf(const std::string& s, uint64_t h) {
    for (unsigned char c : s) h ^= c, h *= 1099511628211ULL;
    return h;
}

/// -1 to 1, the same every time for this seed and salt.
float spread(uint64_t seed, uint64_t salt) {
    uint64_t x = seed ^ (salt * 0x9E3779B97F4A7C15ULL);
    x ^= x >> 33, x *= 0xff51afd7ed558ccdULL, x ^= x >> 33, x *= 0xc4ceb9fe1a85ec53ULL, x ^= x >> 33;
    return float(x >> 11) / float(1ULL << 53) * 2.0f - 1.0f;
}

} // namespace

Voice of(const Human& human, talk::Tone tone) {
    const HumanLook& look = human.look;
    const Traits& t = human.traits;
    Voice v;
    v.speed = SPEED, v.articulation = CLARITY, v.intonation = EXPRESSION, v.breath = BREATH;
    v.seed = hashOf(human.name, 1469598103934665603ULL ^ uint64_t(human.id));
    // The smallest body is a torso 5 by 4 on legs of 3.
    float pixels = float(std::max(0, look.torsoW - 5) + std::max(0, look.torsoH - 4) + std::max(0, look.legH - 3));
    v.pitch = PITCH * (1.0f - PITCH_PER_PIXEL * pixels) * (look.beard ? 1.0f - BEARD_LOWER : 1.0f);
    v.size = SIZE - SIZE_PER_PIXEL * pixels;
    v.pitch *= 1.0f - BRAVE_LOWER * (t.brave - 0.5f) * 2.0f;
    v.vibrato *= 1.0f - BRAVE_STEADY * (t.brave - 0.5f) * 2.0f;
    v.speed *= 1.0f + SOCIABLE_FASTER * (t.sociable - 0.5f);
    v.breath -= SOCIABLE_BRIGHTER * (t.sociable - 0.5f) + GREEDY_BRIGHTER * (t.greedy - 0.5f);
    v.articulation += GREEDY_HARDER * (t.greedy - 0.5f);
    v.intonation *= 1.0f + CURIOUS_LILT * (t.curious - 0.5f);
    v.pitch *= std::exp2(PITCH_SPREAD * spread(v.seed, 1));
    v.speed *= 1.0f + SPEED_SPREAD * spread(v.seed, 2);

    const ToneShade& s = TONE_SHADES[size_t(tone)];
    v.emotion = s.emotion;
    v.pitch *= s.pitch, v.speed *= s.speed, v.shake += s.shake;
    v.breath += s.breath, v.articulation += s.clarity, v.intonation *= s.lilt;
    float loud = VOLUME * s.loud;

    float dread = std::clamp(human.dread(), 0.0f, 1.0f), gloom = std::clamp(human.gloom(), 0.0f, 1.0f);
    v.shake += DREAD_SHAKE * dread;
    v.speed *= 1.0f - GLOOM_SLOWER * gloom;
    v.pitch *= 1.0f - GLOOM_LOWER * gloom;
    loud *= 1.0f - GLOOM_QUIETER * gloom;
    if (tone == talk::Tone::Plain && gloom > GLOOM_SAD) v.emotion = english::Emotion::Sad;

    v.shake = std::clamp(v.shake, 0.0f, 1.0f);
    v.breath = std::clamp(v.breath, 0.0f, 1.0f);
    v.articulation = std::clamp(v.articulation, 0.0f, 1.0f);
    v.intonation = std::clamp(v.intonation, 0.0f, 1.0f);
    v.volume = std::min(loud, VOLUME_MAX);
    return v;
}

} // namespace voice
