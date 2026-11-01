#pragma once
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

/// Sounds made from a few numbers, read from data/sounds.txt, and crushed
/// to a few bits at a low sample rate so the whole game sounds like an old
/// console. No raylib here: the game and tools/sound_sheet.cpp both render
/// through it.
namespace synth {

/// Samples a second of every rendered sound, before the crush.
inline constexpr int SAMPLE_RATE = 22050;

enum class Wave { Square, Triangle, Saw, Sine, Noise };

/// One voice of a sound: a wave sliding from one pitch to another under
/// an envelope, filtered, maybe played again a few times.
struct Layer {
    Wave wave = Wave::Square;
    /// Hz at the start and the end, slid evenly in pitch. Noise takes a new
    /// value twice a period, so a low one rumbles and a high one hisses.
    float from = 440.0f, to = 440.0f;
    /// A square's share of each period spent high.
    float duty = 0.5f;
    /// Seconds: waiting, rising, held, falling away.
    float delay = 0.0f, attack = 0.002f, hold = 0.0f, decay = 0.1f;
    float gain = 0.5f;
    /// Hz, 0 for none.
    float lowpass = 0.0f, highpass = 0.0f;
    /// Played this many times, gap seconds apart, each fade times the last.
    int repeat = 1;
    float gap = 0.1f, fade = 1.0f;
    /// How far the pitch wanders at random, 0 steady to 1 wild: crunch.
    float wobble = 0.0f;
};

struct Def {
    std::string name;
    std::vector<Layer> layers;
    float gain = 1.0f;
    /// The crush: bits per sample and samples a second.
    int bits = 5;
    float rate = 8000.0f;
};

/// Every sound in the file, in order. Lines it can't make out are reported
/// with source and line number, and skipped.
std::vector<Def> parse(std::istream& in, const std::string& source);
/// The sound as samples from -1 to 1 at SAMPLE_RATE, crushed. lowpass
/// (Hz, 0 for none) dulls it before the crush, as heard through a wall.
/// The same def always renders the same samples.
std::vector<float> render(const Def& def, float lowpass = 0.0f);
/// Samples at SAMPLE_RATE crushed in place to bits at rate, after gain,
/// dulled under lowpass Hz first when it isn't 0. render ends with it.
void crush(std::vector<float>& samples, int bits, float rate, float lowpass = 0.0f, float gain = 1.0f);

} // namespace synth
