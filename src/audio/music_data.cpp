#include "audio/music_data.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace music {

namespace {

/// A note cut short by the next fades out over this, so it doesn't click.
constexpr float CUT_FADE = 0.01f;
/// The tape's wow: two slow wanders in pitch, a second, mixed in these shares.
constexpr double WOW_HZ_A = 0.23, WOW_HZ_B = 0.071, WOW_SHARE_A = 0.7;
constexpr double TAU = 6.283185307179586;

void warn(const std::string& source, int line, const std::string& what) {
    std::fprintf(stderr, "%s:%d: %s\n", source.c_str(), line, what.c_str());
}

/// A pitch as a tracker writes it, C-4 or Db4, in Hz. Flats only: # starts a
/// comment.
bool pitchOf(const std::string& s, float& hz) {
    static const int STEPS[] = {9, 11, 0, 2, 4, 5, 7}; // A to G, from C
    if (s.size() < 3 || s[0] < 'A' || s[0] > 'G') return false;
    int step = STEPS[s[0] - 'A'];
    if (s[1] == 'b') step--;
    else if (s[1] != '-') return false;
    int octave;
    try {
        size_t used;
        octave = std::stoi(s.substr(2), &used);
        if (used != s.size() - 2) return false;
    } catch (...) {
        return false;
    }
    int midi = 12 * (octave + 1) + step;
    hz = 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
    return true;
}

bool isHold(const std::string& s) { return s.find_first_not_of('.') == std::string::npos; }

int indexOf(const std::vector<Section>& sections, const std::string& name) {
    for (size_t i = 0; i < sections.size(); i++)
        if (sections[i].name == name) return int(i);
    return -1;
}

/// What a song's lines name before they can be looked up, kept until the
/// song ends.
struct Pending {
    std::vector<std::string> channels;
    std::vector<std::string> keep;
    struct Flow {
        std::string from;
        std::vector<std::pair<std::string, float>> to;
        int line;
    };
    std::vector<Flow> flows;
    int channelsLine = 0;
};

/// Looks up what the song's lines named, now all of it is read.
void finish(Song& song, Pending& p, const std::string& source) {
    for (const std::string& spec : p.channels) {
        size_t colon = spec.find(':');
        std::string name = spec.substr(0, colon);
        float cents = 0.0f;
        if (colon != std::string::npos) cents = std::strtof(spec.c_str() + colon + 1, nullptr);
        int found = -1;
        for (size_t i = 0; i < song.instruments.size(); i++)
            if (song.instruments[i].name == name) found = int(i);
        if (found < 0) warn(source, p.channelsLine, "no instrument " + name + " in song " + song.name);
        song.channels.push_back(found);
        song.channelDetune.push_back(cents);
        song.keep.push_back(std::find(p.keep.begin(), p.keep.end(), name) != p.keep.end());
    }
    for (const Pending::Flow& f : p.flows) {
        int from = indexOf(song.sections, f.from);
        if (from < 0) {
            warn(source, f.line, "no section " + f.from);
            continue;
        }
        for (const auto& [name, weight] : f.to) {
            int to = indexOf(song.sections, name);
            if (to < 0) warn(source, f.line, "no section " + name);
            else song.sections[size_t(from)].next.push_back(to), song.sections[size_t(from)].weights.push_back(weight);
        }
    }
    // Each note lasts until the next in its channel, a stop, or the end.
    for (Section& s : song.sections)
        for (auto& notes : s.channels)
            for (size_t i = 0; i < notes.size(); i++)
                if (i + 1 == notes.size() && !notes[i].cut) notes[i].rows = s.rows - notes[i].row;
}

} // namespace

std::vector<Song> parse(std::istream& in, const std::string& source) {
    std::vector<Song> songs;
    Pending pending;
    int bits = Song{}.bits;
    float rate = Song{}.rate;
    enum class In { Nothing, Instrument, Section } in_ = In::Nothing;
    // Per channel, the note still open in the section being read: it ends
    // at the next note or stop.
    std::vector<int> open;
    std::string line;
    for (int number = 1; std::getline(in, line); number++) {
        if (size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        std::istringstream words(line);
        std::string head;
        if (!(words >> head)) continue;
        if (head == "crush") {
            if (!(words >> bits >> rate)) warn(source, number, "crush wants bits and a rate");
            continue;
        }
        if (head == "song") {
            if (!songs.empty()) finish(songs.back(), pending, source);
            pending = {};
            Song s;
            s.bits = bits, s.rate = rate;
            if (!(words >> s.name)) {
                warn(source, number, "a song wants a name");
                continue;
            }
            std::string key;
            float v;
            while (words >> key >> v) {
                if (key == "tempo") s.tempo = std::max(1.0f, v);
                else if (key == "rows") s.rowsPerBeat = std::max(1.0f, v);
                else if (key == "wow") s.wow = v;
                else if (key == "gain") s.gain = v;
                else if (key == "bits") s.bits = int(v);
                else if (key == "rate") s.rate = v;
                else warn(source, number, "no song option " + key);
            }
            songs.push_back(std::move(s));
            in_ = In::Nothing;
            continue;
        }
        if (songs.empty()) {
            warn(source, number, head + " before any song");
            continue;
        }
        Song& song = songs.back();
        if (head == "instrument") {
            Instrument ins;
            if (!(words >> ins.name)) {
                warn(source, number, "an instrument wants a name");
                continue;
            }
            std::string key;
            while (words >> key) {
                float v = 0.0f;
                if (key == "sustain") ins.sustain = true;
                else if (key == "gain" && words >> v) ins.gain = v;
                else if (key == "detune" && words >> v) ins.detune = v;
                else warn(source, number, "no instrument option " + key);
            }
            song.instruments.push_back(std::move(ins));
            in_ = In::Instrument;
        } else if (head == "channels") {
            pending.channels.clear();
            for (std::string c; words >> c;) pending.channels.push_back(c);
            pending.channelsLine = number;
        } else if (head == "section") {
            Section s;
            if (!(words >> s.name)) {
                warn(source, number, "a section wants a name");
                continue;
            }
            if (pending.channels.empty()) warn(source, number, "a section before the song's channels");
            s.channels.resize(pending.channels.size());
            open.assign(pending.channels.size(), -1);
            song.sections.push_back(std::move(s));
            in_ = In::Section;
        } else if (head == "flow") {
            Pending::Flow f{{}, {}, number};
            words >> f.from;
            std::string to;
            float weight;
            while (words >> to >> weight) f.to.push_back({to, weight});
            if (f.to.empty()) warn(source, number, "flow wants a section and what may follow it, with weights");
            else pending.flows.push_back(std::move(f));
            in_ = In::Nothing;
        } else if (head == "vary") {
            std::string key;
            while (words >> key) {
                if (key == "drop") words >> song.drop;
                else if (key == "keep")
                    for (std::string k; words >> k;) pending.keep.push_back(k);
                else warn(source, number, "no vary option " + key);
            }
            in_ = In::Nothing;
        } else if (in_ == In::Instrument) {
            synth::Layer l;
            if (synth::readLayer(head, words, l, source, number)) song.instruments.back().layers.push_back(l);
        } else if (in_ == In::Section) {
            // A row: a note, a hit (x), a hold (.) or a stop (==) per channel.
            Section& s = song.sections.back();
            std::vector<std::string> cells{head};
            for (std::string c; words >> c;) cells.push_back(c);
            if (cells.size() != s.channels.size()) {
                warn(source, number, "a row wants " + std::to_string(s.channels.size()) + " cells");
                cells.resize(s.channels.size(), ".");
            }
            for (size_t c = 0; c < cells.size(); c++) {
                const std::string& cell = cells[c];
                if (isHold(cell)) continue;
                float hz = 0.0f;
                bool stop = cell == "==", hit = cell == "x";
                if (!stop && !hit && !pitchOf(cell, hz)) {
                    warn(source, number, "no note " + cell);
                    continue;
                }
                if (open[c] >= 0) {
                    Note& last = s.channels[c][size_t(open[c])];
                    last.rows = s.rows - last.row, last.cut = true;
                    open[c] = -1;
                }
                if (!stop) {
                    s.channels[c].push_back({s.rows, 1, hz, false});
                    open[c] = int(s.channels[c].size()) - 1;
                }
            }
            s.rows++;
        } else {
            warn(source, number, "no " + head + " here");
        }
    }
    if (!songs.empty()) finish(songs.back(), pending, source);
    return songs;
}

Rendered render(const Song& song, int section) {
    const Section& s = song.sections[size_t(section)];
    const float row = song.rowSeconds();
    Rendered r;
    r.length = size_t(s.rows * row * synth::SAMPLE_RATE);
    r.channels.resize(s.channels.size());
    for (size_t c = 0; c < s.channels.size(); c++) {
        std::vector<float>& out = r.channels[c];
        out.assign(r.length, 0.0f);
        if (c >= song.channels.size() || song.channels[c] < 0) continue;
        const Instrument& ins = song.instruments[size_t(song.channels[c])];
        for (size_t n = 0; n < s.channels[c].size(); n++) {
            const Note& note = s.channels[c][n];
            float cents = ins.detune + song.channelDetune[c];
            float shift = (note.hz > 0.0f ? note.hz / 440.0f : 1.0f) * std::pow(2.0f, cents / 1200.0f);
            float seconds = note.rows * row;
            synth::Def d;
            d.layers = ins.layers;
            for (synth::Layer& l : d.layers) {
                l.from *= shift, l.to *= shift;
                if (ins.sustain) l.hold = std::max(l.hold, seconds - l.attack);
            }
            // Each note its own noise, the same every time it is rendered.
            uint64_t seed = (uint64_t(section) << 40) ^ (uint64_t(c) << 24) ^ uint64_t(n) ^ 0x6d757a616bULL;
            std::vector<float> samples = synth::layers(d, seed);
            if (note.cut) {
                size_t end = std::min(samples.size(), size_t(seconds * synth::SAMPLE_RATE));
                size_t fade = std::min(end, size_t(CUT_FADE * synth::SAMPLE_RATE));
                for (size_t i = 0; i < fade; i++) samples[end - fade + i] *= 1.0f - float(i) / float(fade);
                samples.resize(end);
            }
            size_t at = size_t(note.row * row * synth::SAMPLE_RATE);
            if (out.size() < at + samples.size()) out.resize(at + samples.size(), 0.0f);
            for (size_t i = 0; i < samples.size(); i++) out[at + i] += samples[i] * ins.gain;
        }
    }
    for (const auto& ch : r.channels) r.tail = std::max(r.tail, ch.size());
    return r;
}

Tape::Tape(const Song& song, uint64_t seed) : song_(song), rng_(seed), crush_(song.bits, song.rate) {
    for (size_t i = 0; i < song_.sections.size(); i++) rendered_.push_back(render(song_, int(i)));
    if (!song_.sections.empty()) cur_ = start(0);
}

Tape::Playing Tape::start(int section) {
    Playing p;
    p.section = section;
    size_t n = song_.channels.size();
    p.on.assign(n, true);
    // The song starts as written.
    if (time_ == 0.0 || song_.drop <= 0.0f) return p;
    for (size_t c = 0; c < n; c++)
        if (!song_.keep[c] && rng_.chance(song_.drop)) p.on[c] = false;
    if (std::none_of(p.on.begin(), p.on.end(), [](bool b) { return b; })) p.on[size_t(rng_.range(0, int(n) - 1))] = true;
    return p;
}

int Tape::following(int section) {
    const Section& s = song_.sections[size_t(section)];
    if (s.next.empty()) return (section + 1) % int(song_.sections.size());
    return s.next[size_t(rng_.weighted(s.weights))];
}

float Tape::sampleOf(const Playing& p) const {
    if (p.section < 0) return 0.0f;
    const Rendered& r = rendered_[size_t(p.section)];
    size_t i = size_t(p.pos);
    float t = float(p.pos - double(i)), s = 0.0f;
    for (size_t c = 0; c < r.channels.size(); c++) {
        if (!p.on[c]) continue;
        const std::vector<float>& ch = r.channels[c];
        if (i + 1 < ch.size()) s += ch[i] + (ch[i + 1] - ch[i]) * t;
    }
    return s;
}

void Tape::fill(float* out, size_t n) {
    for (size_t k = 0; k < n; k++) {
        if (cur_.section < 0) {
            out[k] = 0.0f;
            continue;
        }
        float s = (sampleOf(cur_) + sampleOf(prev_)) * song_.gain;
        out[k] = crush_(std::clamp(s, -1.0f, 1.0f));
        // The old tape drifts a little faster and slower.
        double cents = song_.wow * (WOW_SHARE_A * std::sin(TAU * WOW_HZ_A * time_) +
                                    (1.0 - WOW_SHARE_A) * std::sin(TAU * WOW_HZ_B * time_ + 1.0));
        double step = std::pow(2.0, cents / 1200.0);
        time_ += 1.0 / synth::SAMPLE_RATE;
        cur_.pos += step;
        if (prev_.section >= 0) {
            prev_.pos += step;
            if (prev_.pos >= double(rendered_[size_t(prev_.section)].tail)) prev_ = {};
        }
        if (cur_.pos >= double(rendered_[size_t(cur_.section)].length)) {
            double over = cur_.pos - double(rendered_[size_t(cur_.section)].length);
            prev_ = cur_;
            cur_ = start(following(cur_.section));
            cur_.pos = over;
        }
    }
}

void Tape::seek(const std::string& section, double position) {
    int i = indexOf(song_.sections, section);
    if (i < 0) return;
    cur_ = start(i);
    cur_.pos = std::min(position, double(rendered_[size_t(i)].length));
    prev_ = {};
}

} // namespace music
