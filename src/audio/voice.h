#pragma once
#include "audio/voice_english.h"
#include <cstdint>
#include <string_view>
#include <vector>

/// Humans' voices, ported from Loudworks' voice generator
/// (~/Trinkets/Loudworks/loudworks/voices/, by way of Goblin Engineering's
/// C++ copy). Three kinds:
/// - English (humans speak with it): the words read as English, with
///   stress and intonation; see voice_english.h.
/// - Formant: a buzzy source through moving vowel resonances, with a sound
///   of its own for each kind of consonant (a burst for p, t, k, hiss for
///   s, a hum for m, n, a rolled r), so the letters can almost be followed.
/// - Villager: a short pitched blip per letter, vowels coloured by their
///   formants, every consonant the same click.
/// The babbles read letter by letter, each vowel letter one pure vowel. The same
/// voice and text always give the same samples. No raylib here, so the
/// tools render through it too.
namespace voice {

enum class Kind { English, Formant, Villager };

/// How one human sounds saying one line: what it was born with, then
/// shaded by the line's tone and its mood.
struct Voice {
    Kind kind = Kind::English;
    /// Hz of the voice at rest.
    float pitch = 340.0f;
    /// Letters a second, 1 as made.
    float speed = 1.0f;
    /// Formant scale: above 1 a smaller throat, below 1 a bigger one.
    float size = 1.08f;
    /// Buzzy (1) to soft (0).
    float brightness = 0.55f;
    /// How far the pitch wanders at random, 0 to 1.
    float variation = 0.3f;
    /// Villager: how far each blip's pitch falls.
    float chirp = 0.28f;
    /// Formant: strength of the consonants' bursts and hiss; each vowel's
    /// own pitch; the phrase's fall and the question's rise; vowels dying
    /// away like blips.
    float articulation = 0.8f, vowelCue = 0.5f, intonation = 0.6f, chop = 0.25f;
    /// A quick tremble in pitch and loudness, 0 steady to 1 shaking.
    float shake = 0.0f;
    /// English: the emotion it speaks with; airy (1) to clear (0); vibrato
    /// on long vowels; a nasal twang. Its clarity is the articulation and
    /// its expression the intonation.
    english::Emotion emotion = english::Emotion::Calm;
    float breath = 0.5f, vibrato = 0.5f, twang = 0.3f;
    /// Loudest sample, 0 to 1.
    float volume = 0.7f;
    /// Changes the wobble without changing the voice.
    uint64_t seed = 0;
};

/// The line said in that voice, at synth::SAMPLE_RATE, -1 to 1, before the
/// crush. Empty for a line with nothing to say (only punctuation).
std::vector<float> speak(const Voice& voice, std::string_view text);

} // namespace voice
