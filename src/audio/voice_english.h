#pragma once
#include <cstdint>
#include <string>
#include <vector>

/// The English voice: words read as English (a lexicon and letter-to-sound
/// rules, word stress, reduced vowels), timed and intoned like speech, and
/// sung by a Klatt-style formant synth. Loudworks' Princess voice
/// (~/Trinkets/Loudworks/loudworks/voices/princess.py, english.py), copied
/// from Goblin Engineering's C++ port (voice_princess.cpp), kept close to
/// it, in its own style, so it can be copied again when Loudworks changes.
/// The Mall adds a shake and lets go of a word's last stop gently.
namespace voice::english {

/// The emotions it was made with, each a pitch, range, pace and breath.
enum class Emotion { Calm, Warm, Joyful, Stern, Worried, Sad, Commanding };

struct Params {
    Emotion emotion = Emotion::Calm;
    /// Hz at rest, before the emotion moves it.
    double pitch = 225.0;
    double speed = 1.0;
    /// Formant scale: above 1 a smaller speaker.
    double size = 1.0;
    /// How sharply consonants are made; how far the pitch moves; airy (1)
    /// to clear (0); vibrato on long vowels; a nasal, goblin twang.
    double clarity = 0.6, expression = 0.7, breath = 0.5, vibrato = 0.5, goblin = 0.3;
    /// A quick tremble in pitch and loudness, 0 to 1.
    double shake = 0.0;
    double volume = 0.7;
    uint64_t seed = 0;
};

/// The text said, at synth::SAMPLE_RATE, peak at volume. Empty when
/// there is nothing to say.
std::vector<float> render(const std::u32string& text, const Params& params);

} // namespace voice::english
