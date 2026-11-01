#include "audio/voice.h"
#include "audio/synth.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace voice {

namespace {

constexpr double PI = 3.14159265358979323846;
/// The shake's tremble, in Hz, and how far it moves the pitch at full
/// shake, in semitones, and the loudness.
constexpr double SHAKE_HZ = 9.0, SHAKE_SEMIS = 1.4, SHAKE_LOUD = 0.35;

/// A small generator of its own (PCG32), so a voice never touches the
/// sim's dice and sounds the same on every machine.
class Dice {
public:
    explicit Dice(uint64_t seed) {
        next();
        state_ += seed;
        next();
    }
    uint32_t next() {
        uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t shifted = uint32_t(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = uint32_t(old >> 59u);
        return (shifted >> rot) | (shifted << ((32u - rot) & 31u));
    }
    double unit() { return next() * (1.0 / 4294967296.0); }
    double range(double a, double b) { return a + (b - a) * unit(); }

private:
    uint64_t state_ = 0;
};

uint64_t fnv(const std::u32string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (char32_t c : s)
        for (int b = 0; b < 4; b++) h ^= (uint64_t(c) >> (8 * b)) & 0xffu, h *= 1099511628211ULL;
    return h;
}

uint64_t seedOf(const std::u32string& text, uint64_t seed) { return fnv(text) ^ (seed * 0x9E3779B97F4A7C15ULL); }

/// The line as letters, lowered: UTF-8 read up to three bytes.
std::u32string letters(std::string_view text) {
    std::u32string out;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = (unsigned char)text[i];
        char32_t u = c;
        size_t more = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
        if (more) u = c & (0x3f >> more);
        for (size_t k = 1; k <= more && i + k < text.size(); k++) u = (u << 6) | ((unsigned char)text[i + k] & 0x3f);
        i += more + 1;
        if ((u >= U'A' && u <= U'Z') || (u >= 0xc0 && u <= 0xde && u != 0xd7)) u += 32;
        out.push_back(u);
    }
    return out;
}

bool isSpace(char32_t c) { return c == U' ' || c == U'\n' || c == U'\t' || c == U'\r'; }
bool isDigit(char32_t c) { return c >= U'0' && c <= U'9'; }
bool isAlpha(char32_t c) {
    return (c >= U'a' && c <= U'z') || (c >= 0xc0 && c <= 0x24f && c != 0xd7 && c != 0xf7);
}

/// A vowel letter: the villager's pitch offset in octaves, and which pure
/// vowel it is (a e i o u y).
struct VowelInfo {
    double rel = 0.0;
    char key = 0;
};

bool vowelInfo(char32_t c, VowelInfo& out) {
    switch (c) {
    case U'a': case 0xe0: case 0xe1: case 0xe2: out = {0.0, 'a'}; return true;
    case 0xe4: out = {0.05, 'a'}; return true;
    case U'e': case 0xe8: case 0xea: case 0xeb: out = {0.12, 'e'}; return true;
    case 0xe9: out = {0.14, 'e'}; return true;
    case U'i': case 0xec: case 0xed: case 0xee: case 0xef: out = {0.24, 'i'}; return true;
    case U'o': case 0xf2: case 0xf3: case 0xf4: out = {-0.08, 'o'}; return true;
    case 0xf6: out = {-0.05, 'o'}; return true;
    case U'u': case 0xf9: case 0xfa: case 0xfb: out = {-0.16, 'u'}; return true;
    case 0xfc: out = {-0.12, 'y'}; return true;
    case U'y': out = {0.18, 'y'}; return true;
    default: return false;
    }
}

bool endsSentence(char32_t c) { return c == U'.' || c == U'!' || c == U'?' || c == 0x2026; }

// ------------------------------------------------------------------ villager

enum class BlipKind { Vowel, Consonant, Pause };

struct Blip {
    int start = 0, duration = 1;
    double freq = 0.0, amp = 0.0;
    BlipKind kind = BlipKind::Pause;
    double chirp = 0.0;
    char vowel = '?';
};

struct Formants {
    double f1, a1, f2, a2, f3, a3;
};

/// Average formants and their loudness, for the villager's vowels.
Formants villagerFormants(char key) {
    switch (key) {
    case 'a': return {730, 1.0, 1090, 0.85, 2440, 0.35};
    case 'e': return {530, 0.9, 1840, 1.0, 2480, 0.40};
    case 'i': return {270, 0.75, 2290, 1.0, 3010, 0.45};
    case 'o': return {570, 1.0, 840, 0.70, 2410, 0.30};
    case 'u': return {300, 0.95, 870, 0.55, 2240, 0.25};
    case 'y': return {300, 0.8, 1850, 0.95, 2500, 0.40};
    default: return {500, 0.9, 1500, 0.75, 2500, 0.30};
    }
}

/// The line as a row of blips and pauses, one per letter.
std::vector<Blip> blipsOf(const std::u32string& text, const Voice& v, Dice& dice) {
    constexpr int sr = synth::SAMPLE_RATE;
    std::vector<Blip> out;
    const double glyph = 0.085 / std::clamp(double(v.speed), 0.25, 3.0);
    int t = 0;
    auto pause = [&](double factor) {
        Blip b;
        b.start = t;
        b.duration = std::max(1, int(sr * glyph * factor));
        out.push_back(b);
        t += b.duration;
    };
    for (char32_t ch : text) {
        if (isSpace(ch)) {
            pause(ch == U' ' ? 0.55 : 1.1);
            continue;
        }
        if (endsSentence(ch)) {
            pause(3.2);
            continue;
        }
        if (ch == U',' || ch == U';' || ch == U':' || ch == 0x2014 || ch == U'-') {
            pause(1.6);
            continue;
        }
        if (!isAlpha(ch) && !isDigit(ch)) {
            t += int(sr * glyph * 0.25);
            continue;
        }
        Blip b;
        double rel, seconds;
        VowelInfo vi;
        if (vowelInfo(ch, vi)) {
            b.kind = BlipKind::Vowel;
            rel = vi.rel;
            b.vowel = vi.key;
            seconds = glyph * (1.35 + 0.35 * dice.unit());
            b.amp = 0.62 + 0.18 * dice.unit();
        } else if (isDigit(ch)) {
            b.kind = BlipKind::Vowel;
            rel = (double(ch - U'0') / 9.0) * 0.3 - 0.1;
            seconds = glyph * 1.1;
            b.amp = 0.5;
        } else {
            b.kind = BlipKind::Consonant;
            rel = (double(ch % 11) / 10.0) * 0.35 - 0.1;
            seconds = glyph * (0.35 + 0.2 * dice.unit());
            b.amp = 0.28 + 0.15 * dice.unit();
        }
        double wobble = (dice.unit() * 2.0 - 1.0) * v.variation * 0.22;
        b.freq = std::clamp(v.pitch * std::pow(2.0, rel + wobble), 70.0, 1800.0);
        b.duration = std::max(1, int(sr * seconds));
        b.start = t;
        b.chirp = v.chirp * (0.7 + 0.6 * dice.unit());
        out.push_back(b);
        t += std::max(1, int(sr * glyph * (b.kind == BlipKind::Consonant ? 0.72 : 1.05)));
    }
    return out;
}

void addBlip(const Blip& b, double bright, Dice& noise, float* out) {
    const double sr = synth::SAMPLE_RATE;
    const int attack = std::max(1, int(0.006 * sr));
    Formants f = villagerFormants(b.vowel);
    double scale = std::clamp(b.freq / 340.0, 0.85, 1.35), top = sr * 0.48;
    double f1 = std::min(f.f1 * scale, top), f2 = std::min(f.f2 * scale, top), f3 = std::min(f.f3 * scale, top);
    double phase = 0.0;
    for (int n = 0; n < b.duration; n++) {
        double env = n < attack ? double(n) / attack
                                : std::exp(-2.8 * (n - attack) / double(std::max(1, b.duration - attack)));
        if (b.kind == BlipKind::Consonant) env *= 0.85;
        double freq = b.freq * (1.0 - b.chirp * (double(n) / b.duration) * 0.65), tone;
        if (b.kind == BlipKind::Vowel) {
            double glottal = std::sin(2.0 * PI * phase);
            glottal = glottal * glottal * (glottal > 0.0 ? 1.0 : 0.35);
            double formants = f.a1 * std::sin(2.0 * PI * f1 * n / sr) +
                              f.a2 * (0.75 + 0.25 * bright) * std::sin(2.0 * PI * f2 * n / sr) +
                              f.a3 * (0.45 + 0.55 * bright) * std::sin(2.0 * PI * f3 * n / sr);
            tone = (0.22 * std::sin(2.0 * PI * phase) + 0.78 * formants * (0.35 + 0.65 * glottal)) * 0.55;
        } else {
            double square = phase < 0.5 ? 1.0 : -1.0;
            tone = square * (0.35 + 0.45 * bright) + std::sin(2.0 * PI * phase) * (0.55 - 0.25 * bright);
            tone = tone * 0.45 + noise.range(-1.0, 1.0) * 0.55;
        }
        out[n] += float(tone * env * b.amp);
        phase += freq / sr;
        phase -= std::floor(phase);
    }
}

std::vector<float> villager(const std::u32string& text, const Voice& v) {
    Dice dice(seedOf(text, v.seed));
    std::vector<Blip> blips = blipsOf(text, v, dice);
    int length = 0;
    for (const Blip& b : blips)
        if (b.kind != BlipKind::Pause) length = std::max(length, b.start + b.duration);
    if (length == 0) return {};
    std::vector<float> out(size_t(length + int(0.02 * synth::SAMPLE_RATE)), 0.0f);
    Dice noise(seedOf(text, v.seed) ^ 0xA5A5A5A5ULL);
    for (const Blip& b : blips)
        if (b.kind != BlipKind::Pause) addBlip(b, v.brightness, noise, out.data() + b.start);
    // The shake, then the loudness asked for.
    float peak = 0.0f;
    for (size_t i = 0; i < out.size(); i++) {
        double t = double(i) / synth::SAMPLE_RATE;
        out[i] *= float(1.0 - v.shake * SHAKE_LOUD * (0.5 + 0.5 * std::sin(2.0 * PI * SHAKE_HZ * t)));
        peak = std::max(peak, std::abs(out[i]));
    }
    float gain = peak > 0.0f ? 0.85f * v.volume / peak : 0.0f;
    for (float& x : out) x *= gain;
    return out;
}

// ------------------------------------------------------------------- formant

struct F3 {
    double f1, f2, f3;
};

F3 vowelFormants(char key) {
    switch (key) {
    case 'a': return {850, 1450, 2800};
    case 'e': return {500, 2100, 2800};
    case 'i': return {320, 2650, 3250};
    case 'o': return {520, 950, 2800};
    case 'u': return {350, 800, 2700};
    case 'y': return {400, 2000, 2700};
    default: return {550, 1550, 2700}; // neutral: digits, unknown letters
    }
}

/// Each vowel's own pitch in semitones, the Animalese-like cue.
double vowelPitch(char key) {
    switch (key) {
    case 'e': return 2.0;
    case 'i': return 4.0;
    case 'o': return -1.5;
    case 'u': return -3.0;
    case 'y': return 3.0;
    default: return 0.0;
    }
}

/// Places in the mouth a consonant is made: where the formants glide from
/// and to.
constexpr F3 LAB{300, 900, 2300}, ALV{300, 1750, 2700}, PAL{300, 2050, 2700}, VEL{300, 2150, 2500},
    DEN{300, 1500, 2600}, ROUND{300, 650, 2200};

enum class SegKind { Vowel, Stop, Fric, Nasal, Liquid, Glide, Aspir, Pause };

/// One sound of the line: a vowel, a part of a consonant, or a pause.
struct Seg {
    SegKind kind = SegKind::Pause;
    /// Seconds at speed 1, and never shorter, so the cue survives fast speech.
    double dur = 0.05, minDur = 0.02;
    bool hasFormants = false, hasLocus = false, hasBurst = false;
    F3 formants{0, 0, 0}, locus{0, 0, 0};
    double voice = 0.0, fric = 0.0, fricCf = 4000.0, fricBw = 3000.0, asp = 0.0;
    double burstCf = 0.0, burstBw = 0.0, burstAmp = 0.0;
    double nasalZero = 0.0;
    bool trill = false, tap = false;
    double semitones = 0.0;
    char vowel = 0;
    char32_t punct = 0;
};

using Segs = std::vector<Seg>;

Segs operator+(Segs a, const Segs& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Segs stop(double cf, double bw, double amp, F3 locus, bool voiced) {
    Seg s;
    s.kind = SegKind::Stop;
    s.dur = 0.045, s.minDur = 0.016;
    s.hasLocus = true, s.locus = locus;
    s.voice = voiced ? 0.22 : 0.0;
    s.hasBurst = true, s.burstCf = cf, s.burstBw = bw, s.burstAmp = amp * (voiced ? 0.6 : 1.0);
    Segs out{s};
    if (!voiced) {
        Seg a;
        a.kind = SegKind::Aspir;
        a.dur = 0.032, a.minDur = 0.012, a.asp = 0.45;
        out.push_back(a);
    }
    return out;
}

Segs fric(double cf, double bw, double amp, F3 locus, bool voiced, double dur) {
    Seg s;
    s.kind = SegKind::Fric;
    s.dur = dur, s.minDur = 0.035;
    s.hasLocus = true, s.locus = locus;
    s.voice = voiced ? 0.6 : 0.0;
    s.fric = amp, s.fricCf = cf, s.fricBw = bw;
    return {s};
}

Segs nasal(F3 formants, double zero, F3 locus, double semitones) {
    Seg s;
    s.kind = SegKind::Nasal;
    s.dur = 0.07, s.minDur = 0.03;
    s.hasFormants = true, s.formants = formants;
    s.hasLocus = true, s.locus = locus;
    s.voice = 0.55, s.nasalZero = zero, s.semitones = semitones;
    return {s};
}

Segs liquid(F3 formants, bool trill, bool tap) {
    Seg s;
    s.kind = SegKind::Liquid;
    s.dur = 0.065, s.minDur = 0.03;
    s.hasFormants = true, s.formants = formants;
    s.voice = 0.75, s.trill = trill, s.tap = tap;
    return {s};
}

Segs glide(F3 formants) {
    Seg s;
    s.kind = SegKind::Glide;
    s.dur = 0.05, s.minDur = 0.022;
    s.hasFormants = true, s.formants = formants;
    s.voice = 0.8;
    return {s};
}

Segs vowelSeg(char key, double dur) {
    Seg v;
    v.kind = SegKind::Vowel;
    v.dur = dur, v.minDur = 0.04;
    v.hasFormants = true, v.formants = vowelFormants(key);
    v.voice = 1.0, v.vowel = key;
    return {v};
}

bool consonant(char32_t c, Segs& out) {
    switch (c) {
    case U'p': out = stop(1000, 1200, 0.55, LAB, false); return true;
    case U'b': out = stop(900, 1000, 0.55, LAB, true); return true;
    case U't': out = stop(4500, 3000, 0.6, ALV, false); return true;
    case U'd': out = stop(4000, 2500, 0.6, ALV, true); return true;
    case U'k': case U'c': out = stop(2000, 1000, 0.65, VEL, false); return true;
    case U'g': out = stop(1900, 900, 0.65, VEL, true); return true;
    case U'q': out = stop(2000, 1000, 0.65, VEL, false) + glide(ROUND); return true;
    case U'x': out = stop(2000, 1000, 0.65, VEL, false) + fric(6500, 3500, 0.9, ALV, false, 0.07); return true;
    case U'j': out = stop(4000, 2500, 0.5, PAL, true) + fric(3200, 2200, 0.6, PAL, true, 0.06); return true;
    case U's': out = fric(6500, 3500, 0.9, ALV, false, 0.085); return true;
    case U'z': out = fric(6000, 3000, 0.5, ALV, true, 0.075); return true;
    case U'f': out = fric(4500, 6000, 0.35, LAB, false, 0.08); return true;
    case U'v': out = fric(4000, 5000, 0.25, LAB, true, 0.065); return true;
    case U'h': {
        Seg a;
        a.kind = SegKind::Aspir;
        a.dur = 0.06, a.minDur = 0.025, a.asp = 0.55;
        out = {a};
        return true;
    }
    case U'm': out = nasal({250, 900, 2400}, 900, LAB, -2.0); return true;
    case U'n': out = nasal({300, 1700, 2700}, 1900, ALV, 1.0); return true;
    case U'l': out = liquid({360, 1350, 3000}, false, true); return true;
    case U'r': out = liquid({450, 1300, 1700}, true, false); return true;
    case U'w': out = glide(ROUND); return true;
    default: return false;
    }
}

/// Two letters read as one sound: sh, ch, th, ng, ph, ck, qu.
bool digraph(char32_t a, char32_t b, Segs& out) {
    if (a == U's' && b == U'h') out = fric(3200, 2200, 0.85, PAL, false, 0.085);
    else if (a == U'c' && b == U'h') {
        Segs t = stop(4000, 2500, 0.5, PAL, false);
        t.resize(1); // closure and burst, no breath
        out = t + fric(3200, 2200, 0.7, PAL, false, 0.06);
    } else if (a == U't' && b == U'h') out = fric(6000, 7000, 0.22, DEN, false, 0.07);
    else if (a == U'n' && b == U'g') out = nasal({280, 2000, 2600}, 2600, VEL, 0.0);
    else if (a == U'p' && b == U'h') out = fric(4500, 6000, 0.35, LAB, false, 0.08);
    else if (a == U'c' && b == U'k') out = stop(2000, 1000, 0.65, VEL, false);
    else if (a == U'q' && b == U'u') out = stop(2000, 1000, 0.65, VEL, false) + glide(ROUND);
    else return false;
    return true;
}

bool pauseLength(char32_t c, double& dur) {
    switch (c) {
    case U' ': dur = 0.05; return true;
    case U'\n': case U'\t': dur = 0.1; return true;
    case U',': case U';': case U':': case 0x2014: dur = 0.14; return true;
    case U'-': dur = 0.1; return true;
    case U'.': case U'!': case U'?': dur = 0.27; return true;
    case 0x2026: dur = 0.3; return true;
    default:
        if (!isSpace(c)) return false;
        dur = 0.05;
        return true;
    }
}

/// The line as sounds, letter by letter.
Segs parse(const std::u32string& text) {
    Segs out;
    const size_t n = text.size();
    for (size_t i = 0; i < n; i++) {
        char32_t ch = text[i], next = i + 1 < n ? text[i + 1] : 0;
        double pause;
        if (pauseLength(ch, pause)) {
            Seg p;
            p.dur = pause, p.punct = ch;
            out.push_back(p);
            continue;
        }
        Segs segs;
        VowelInfo vi, nextVi;
        if (next != 0 && digraph(ch, next, segs)) {
            i++;
        } else if (ch == U'y' && next != 0 && vowelInfo(next, nextVi)) {
            segs = glide(vowelFormants('i')); // y before a vowel glides
        } else if (vowelInfo(ch, vi)) {
            segs = vowelSeg(vi.key, next == 0 || !isAlpha(next) ? 0.14 : 0.11);
        } else if (ch == U'c' && (next == U'e' || next == U'i' || next == U'y')) {
            consonant(U's', segs); // soft c: city, ice
        } else if (consonant(ch, segs)) {
            if (next == ch) { // doubled: one longer sound
                for (Seg& s : segs)
                    if (s.kind == SegKind::Stop || s.kind == SegKind::Fric || s.kind == SegKind::Nasal ||
                        s.kind == SegKind::Liquid)
                        s.dur *= 1.7;
                i++;
            }
        } else if (isDigit(ch) || isAlpha(ch)) {
            segs = vowelSeg('@', 0.1);
        } else {
            continue;
        }
        out.insert(out.end(), segs.begin(), segs.end());
    }
    return out;
}

struct Timed {
    Seg seg;
    double a, b;
};

/// A value moving in straight lines between points in time, held flat
/// beyond its ends.
class Track {
public:
    explicit Track(double fallback = 0.0) : fallback_(fallback) {}
    void add(double t, double v) { points_.emplace_back(t, v); }
    void finish() {
        std::stable_sort(points_.begin(), points_.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    }
    double at(double t) const {
        if (points_.empty()) return fallback_;
        if (t <= points_.front().first) return points_.front().second;
        if (t >= points_.back().first) return points_.back().second;
        auto hi = std::upper_bound(points_.begin(), points_.end(), t, [](double v, const auto& p) { return v < p.first; });
        auto lo = hi - 1;
        double span = hi->first - lo->first;
        if (span <= 1e-12) return hi->second;
        return lo->second + (hi->second - lo->second) * (t - lo->first) / span;
    }

private:
    double fallback_;
    std::vector<std::pair<double, double>> points_;
};

/// Two-pole resonator, unity gain at DC (Klatt's cascade).
struct Resonator {
    double a1 = 0.0, a2 = 0.0, b0 = 1.0, y1 = 0.0, y2 = 0.0;
    void set(double freq, double bw, double sr) {
        freq = std::min(freq, 0.45 * sr);
        double r = std::exp(-PI * bw / sr);
        a1 = -2.0 * r * std::cos(2.0 * PI * freq / sr);
        a2 = r * r;
        b0 = 1.0 + a1 + a2;
    }
    double process(double x) {
        double y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1, y1 = y;
        return y;
    }
};

/// A resonator turned inside out, unity gain at DC: the nose's notch.
struct AntiResonator {
    double a1 = 0.0, a2 = 0.0, g = 1.0, x1 = 0.0, x2 = 0.0;
    void set(double freq, double bw, double sr) {
        freq = std::min(freq, 0.45 * sr);
        double r = std::exp(-PI * bw / sr);
        a1 = -2.0 * r * std::cos(2.0 * PI * freq / sr);
        a2 = r * r;
        g = 1.0 / std::max(1e-9, 1.0 + a1 + a2);
    }
    double process(double x) {
        double y = (x + a1 * x1 + a2 * x2) * g;
        x2 = x1, x1 = x;
        return y;
    }
};

/// Band-pass with a 0 dB peak (RBJ), for hiss and bursts.
struct BandPass {
    double b0 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
    void set(double freq, double q, double sr) {
        freq = std::clamp(freq, 20.0, 0.45 * sr);
        q = std::max(q, 0.1);
        double w = 2.0 * PI * freq / sr, alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
        b0 = alpha / a0, b2 = -alpha / a0;
        a1 = -2.0 * std::cos(w) / a0, a2 = (1.0 - alpha) / a0;
    }
    double process(double x) {
        double y = b0 * x + z1;
        z1 = -a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/// Smooths a saw's jump, so it buzzes without aliasing.
double blep(double t, double dt) {
    if (t < dt) {
        double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt) {
        double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

std::vector<float> formant(const std::u32string& text, const Voice& v) {
    const double sr = synth::SAMPLE_RATE, speed = std::clamp(double(v.speed), 0.25, 3.0);
    std::vector<Timed> line;
    double t0 = 0.0, lastEnd = -1.0;
    for (const Seg& s : parse(text)) {
        double d = std::max(s.minDur, s.dur / speed);
        line.push_back({s, t0, t0 + d});
        if (s.kind != SegKind::Pause) lastEnd = t0 + d;
        t0 += d;
    }
    if (lastEnd < 0.0) return {};
    const double end = lastEnd + 0.04;
    const size_t n = size_t(end * sr);
    Dice dice(seedOf(text, v.seed));
    const double art = v.articulation, size = v.size;

    // Points to move between: targets inside voiced sounds, the mouth's
    // place at the edges of consonants.
    Track f[3] = {Track(550.0), Track(1550.0), Track(2700.0)};
    Track voiced, asp, hissing, nasalAmt, cf(4000.0), bw(3000.0), zero(1500.0), pitch;
    struct Mod {
        double a, b;
        bool trill;
    };
    std::vector<Mod> mods;
    struct Burst {
        double at, cf, bw, amp;
    };
    std::vector<Burst> bursts;
    for (const Timed& tm : line) {
        const Seg& g = tm.seg;
        double a = tm.a, b = tm.b, d = b - a, ramp = std::min(0.004, d / 4.0);
        double fv[3] = {g.formants.f1, g.formants.f2, g.formants.f3}, lv[3] = {g.locus.f1, g.locus.f2, g.locus.f3};
        if (g.hasFormants) {
            if (g.kind == SegKind::Glide) {
                for (int k = 0; k < 3; k++) f[k].add(a + d * 0.3, fv[k]);
            } else {
                double edge = std::min(0.025, d / 3.0);
                for (int k = 0; k < 3; k++) f[k].add(a + edge, fv[k]), f[k].add(b - edge, fv[k]);
            }
        } else if (g.hasLocus) {
            for (int k = 0; k < 3; k++) f[k].add(a, lv[k]), f[k].add(b, lv[k]);
        }
        if (g.semitones != 0.0) pitch.add(a + ramp, g.semitones), pitch.add(b - ramp, g.semitones);
        if (g.tap) mods.push_back({a + d * 0.35, a + d * 0.35 + std::min(0.018, d * 0.4), false});
        if (g.kind == SegKind::Vowel) {
            pitch.add((a + b) / 2.0, vowelPitch(g.vowel) * v.vowelCue);
            voiced.add(a + ramp, 1.0), voiced.add(b - ramp, 1.0 - 0.75 * v.chop);
        } else {
            voiced.add(a + ramp, g.voice), voiced.add(b - ramp, g.voice);
        }
        asp.add(a + ramp, g.asp * art), asp.add(b - ramp, g.asp * art);
        double fa = g.fric * (0.4 + 0.6 * art);
        hissing.add(a + ramp, fa), hissing.add(b - ramp, fa);
        double na = g.kind == SegKind::Nasal ? 1.0 : 0.0;
        nasalAmt.add(a + ramp, na), nasalAmt.add(b - ramp, na);
        if (g.fric > 0.0) cf.add(a, g.fricCf), cf.add(b, g.fricCf), bw.add(a, g.fricBw), bw.add(b, g.fricBw);
        if (g.nasalZero > 0.0) zero.add(a, g.nasalZero), zero.add(b, g.nasalZero);
        if (g.trill) mods.push_back({a, b, true});
        if (g.hasBurst) bursts.push_back({b, g.burstCf, g.burstBw, g.burstAmp});
    }
    for (Track* tr : {&f[0], &f[1], &f[2], &voiced, &asp, &hissing, &nasalAmt, &cf, &bw, &zero, &pitch}) tr->finish();

    // Pitch: each sound's own, the phrase falling, a question rising, a
    // shout lifted, and a slow wander.
    std::vector<double> semis(n);
    for (size_t i = 0; i < n; i++) semis[i] = pitch.at(double(i) / sr);
    double phraseStart = 0.0;
    std::vector<Timed> phrases = line;
    Seg last;
    last.punct = U'.';
    phrases.push_back({last, end, end});
    for (const Timed& tm : phrases) {
        bool closes = (tm.seg.kind == SegKind::Pause && endsSentence(tm.seg.punct)) || tm.b >= end;
        if (!closes) continue;
        double span = std::max(1e-3, tm.b - phraseStart);
        size_t i0 = size_t(std::max(0.0, std::ceil(phraseStart * sr)));
        size_t i1 = std::min(n, size_t(std::max(0.0, std::ceil(tm.b * sr))));
        for (size_t i = i0; i < i1; i++) {
            double frac = (double(i) / sr - phraseStart) / span, shape = -2.5 * frac;
            if (tm.seg.punct == U'?') {
                double r = std::clamp((frac - 0.65) / 0.35, 0.0, 1.0);
                shape += 6.0 * r * r;
            } else if (tm.seg.punct == U'!') {
                shape += 2.0;
            }
            semis[i] += shape * v.intonation;
        }
        phraseStart = tm.b;
    }
    const double wander = v.variation * 1.5, ph0 = dice.range(0.0, 2.0 * PI), ph1 = dice.range(0.0, 2.0 * PI);

    std::vector<double> y(n, 0.0);
    Resonator r1, r2, r3, r4, r5, nasalPole;
    AntiResonator nasalZero;
    BandPass bp1, bp2;
    const double soft = 0.85 * (1.0 - v.brightness);
    double low = 0.0, phase = 0.0, lastCf = -1.0, lastBw = -1.0, lastZero = -1.0;
    r4.set(3400.0 * size, 250.0, sr);
    r5.set(4200.0 * size, 320.0, sr);
    nasalPole.set(270.0, 100.0, sr);
    for (size_t i = 0; i < n; i++) {
        double t = double(i) / sr, st = semis[i];
        if (wander > 0.0)
            st += wander * (0.6 * std::sin(2.0 * PI * 3.1 * t + ph0) + 0.4 * std::sin(2.0 * PI * 5.3 * t + ph1));
        double tremble = std::sin(2.0 * PI * SHAKE_HZ * t);
        st += v.shake * SHAKE_SEMIS * tremble;
        double f0 = v.pitch * std::pow(2.0, st / 12.0);
        if (i % 8 == 0) {
            r1.set(f[0].at(t) * size, 80.0 + 0.12 * f0, sr);
            r2.set(f[1].at(t) * size, 110.0 + 0.12 * f0, sr);
            r3.set(f[2].at(t) * size, 160.0 + 0.1 * f0, sr);
        }
        if (i % 32 == 0) {
            double c = cf.at(t), w = bw.at(t), z = zero.at(t);
            if (c != lastCf || w != lastBw) {
                double q = c / std::max(1.0, 1.34 * w);
                bp1.set(c, q, sr), bp2.set(c, q, sr);
                lastCf = c, lastBw = w;
            }
            if (z != lastZero) nasalZero.set(z * size, 150.0, sr), lastZero = z;
        }
        // A buzzy source: a band-limited saw, softened.
        double dt = f0 / sr;
        phase += dt;
        phase -= std::floor(phase);
        low = (1.0 - soft) * (2.0 * phase - 1.0 - blep(phase, dt)) + soft * low;
        double amp = voiced.at(t) * (1.0 - v.shake * SHAKE_LOUD * (0.5 + 0.5 * tremble));
        for (const Mod& m : mods) {
            if (t < m.a || t >= m.b) continue;
            if (m.trill) amp *= 1.0 - 0.8 * (0.5 + 0.5 * std::cos(2.0 * PI * 28.0 * (t - m.a))); // rolled r
            else amp *= 1.0 - 0.6 * std::sin(PI * (t - m.a) / std::max(1e-6, m.b - m.a)); // l letting go
        }
        double excitation = low * amp * 0.5 + dice.range(-1.0, 1.0) * asp.at(t) * 0.6;
        double s = r5.process(r4.process(r3.process(r2.process(r1.process(excitation)))));
        double na = nasalAmt.at(t);
        s = (1.0 - na) * s + na * 0.35 * nasalZero.process(nasalPole.process(s));
        y[i] = s + bp2.process(bp1.process(dice.range(-1.0, 1.0) * hissing.at(t)));
    }
    for (const Burst& b : bursts) { // stops letting go
        size_t start = size_t(b.at * sr), length = size_t(0.012 * sr);
        if (start >= n) continue;
        BandPass bp;
        bp.set(b.cf, std::max(0.5, b.cf / std::max(1.0, b.bw)), sr);
        for (size_t k = 0; k < length && start + k < n; k++)
            y[start + k] += bp.process(dice.range(-1.0, 1.0) * std::exp(-double(k) / (0.004 * sr))) * b.amp * art * 2.0;
    }
    double peak = 0.0;
    for (double s : y) peak = std::max(peak, std::abs(s));
    double gain = peak > 0.0 ? 0.85 * v.volume / peak : 0.0;
    std::vector<float> out(n);
    for (size_t i = 0; i < n; i++) out[i] = float(y[i] * gain);
    return out;
}

} // namespace

std::vector<float> speak(const Voice& voice, std::string_view text) {
    std::u32string l = letters(text);
    if (voice.kind == Kind::Villager) return villager(l, voice);
    if (voice.kind == Kind::Formant) return formant(l, voice);
    english::Params p;
    p.emotion = voice.emotion;
    p.pitch = voice.pitch, p.speed = voice.speed, p.size = voice.size;
    p.clarity = voice.articulation, p.expression = voice.intonation;
    p.breath = voice.breath, p.vibrato = voice.vibrato, p.goblin = voice.twang;
    p.shake = voice.shake, p.volume = voice.volume, p.seed = voice.seed;
    return english::render(l, p);
}

} // namespace voice
