#pragma once
#include "audio/synth.h"
#include "core/rng.h"
#include <istream>
#include <string>
#include <vector>

/// The music of data/music.txt: songs written like a tiny tracker, notes in
/// rows, a column per channel, played on instruments made of synth layers,
/// and crushed like the sounds. No raylib here: the game, the sheet tool
/// and the sound editor all play songs through it.
namespace music {

/// Layers written at A4, moved to each note's pitch.
struct Instrument {
    std::string name;
    std::vector<synth::Layer> layers;
    float gain = 1.0f;
    /// Cents out of tune, the whole instrument.
    float detune = 0.0f;
    /// Held for as long as the note is written, not struck and let ring.
    bool sustain = false;
};

/// A note in a channel: from a row, for rows rows, at hz (0 for a hit at the
/// pitch its instrument was written at). cut when another note or a stop
/// ends it; otherwise it rings on past the section's end.
struct Note {
    int row = 0, rows = 1;
    float hz = 0.0f;
    bool cut = false;
};

struct Section {
    std::string name;
    int rows = 0;
    /// The notes of each channel, in order.
    std::vector<std::vector<Note>> channels;
    /// What may come next and how likely, by section index; none for the
    /// next in the file.
    std::vector<int> next;
    std::vector<float> weights;
};

struct Song {
    std::string name;
    /// Beats a minute, rows a beat.
    float tempo = 90.0f, rowsPerBeat = 4.0f;
    /// How far the old tape drifts out of tune, in cents.
    float wow = 0.0f;
    float gain = 1.0f;
    /// The crush, as the sounds have it.
    int bits = 5;
    float rate = 8000.0f;
    std::vector<Instrument> instruments;
    /// Each channel's instrument, by index, and its own cents out of tune.
    std::vector<int> channels;
    std::vector<float> channelDetune;
    std::vector<Section> sections;
    /// Each time a section starts, each channel not kept may drop out this
    /// often (never all of them).
    float drop = 0.0f;
    std::vector<bool> keep;

    float rowSeconds() const { return 60.0f / (tempo * rowsPerBeat); }
};

/// Every song in the file. Lines it can't make out are reported with source
/// and line number, and skipped.
std::vector<Song> parse(std::istream& in, const std::string& source);

/// A section rendered, each channel apart, uncrushed: length samples until
/// the next section starts, the channels longer by their last notes' tails.
struct Rendered {
    std::vector<std::vector<float>> channels;
    size_t length = 0;
    /// Samples until the longest channel's tail has rung out.
    size_t tail = 0;
};
Rendered render(const Song& song, int section);

/// A song played on and on: each section followed by one its flow picks,
/// channels dropping out now and then, drifting with the tape's wow, then
/// crushed. Its dice are its own, view only.
class Tape {
public:
    Tape(const Song& song, uint64_t seed);

    /// The next n samples at synth::SAMPLE_RATE, from -1 to 1.
    void fill(float* out, size_t n);
    const Song& song() const { return song_; }
    /// Where it is, to carry on from after the file is read again.
    const std::string& sectionName() const { return song_.sections[size_t(cur_.section)].name; }
    double position() const { return cur_.pos; }
    /// Carries on from a section of that name, at a position, if it has one.
    void seek(const std::string& section, double position);

private:
    /// A section playing: which, where in it, and which channels sound.
    struct Playing {
        int section = -1;
        double pos = 0.0;
        std::vector<bool> on;
    };

    Song song_;
    std::vector<Rendered> rendered_;
    Rng rng_;
    synth::Crush crush_;
    /// The section playing, and the one before it while its tail rings out.
    Playing cur_, prev_;
    /// Seconds played, for the wow.
    double time_ = 0.0;

    Playing start(int section);
    int following(int section);
    float sampleOf(const Playing& p) const;
};

} // namespace music
