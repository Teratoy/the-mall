#include "audio/synth.h"
#include "core/rng.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace synth {

namespace {

/// How often a wobbling pitch picks a new place to wander to, a second.
constexpr float WOBBLE_RATE = 60.0f;
/// Pitch is kept inside what can be heard and drawn at SAMPLE_RATE.
constexpr float MIN_HZ = 1.0f, MAX_HZ = SAMPLE_RATE / 2.0f;
constexpr float TAU = 6.28318530718f;

void warn(const std::string& source, int line, const std::string& what) {
    std::fprintf(stderr, "%s:%d: %s\n", source.c_str(), line, what.c_str());
}

bool waveNamed(const std::string& s, Wave& w) {
    static const std::pair<const char*, Wave> NAMES[] = {
        {"square", Wave::Square}, {"triangle", Wave::Triangle}, {"saw", Wave::Saw},
        {"sine", Wave::Sine},     {"noise", Wave::Noise},
    };
    for (const auto& [name, wave] : NAMES)
        if (s == name) return w = wave, true;
    return false;
}

/// Reads "key value" pairs into a layer; false names the first bad key.
bool layerOption(Layer& l, const std::string& key, float v) {
    if (key == "gain") l.gain = v;
    else if (key == "duty") l.duty = v;
    else if (key == "delay") l.delay = v;
    else if (key == "attack") l.attack = v;
    else if (key == "hold") l.hold = v;
    else if (key == "decay") l.decay = v;
    else if (key == "lowpass") l.lowpass = v;
    else if (key == "highpass") l.highpass = v;
    else if (key == "repeat") l.repeat = std::max(1, int(v));
    else if (key == "gap") l.gap = v;
    else if (key == "fade") l.fade = v;
    else if (key == "wobble") l.wobble = v;
    else return false;
    return true;
}

/// A first-order filter's step towards the input, for cutoff hz.
float smoothing(float hz) { return 1.0f - std::exp(-TAU * hz / SAMPLE_RATE); }

/// A seed from the name, so each sound has its own noise, the same every time.
uint64_t seedOf(const std::string& name) {
    uint64_t h = 1469598103934665603ull;
    for (char c : name) h = (h ^ uint8_t(c)) * 1099511628211ull;
    return h;
}

float waveAt(Wave w, float phase, float duty, float noise) {
    switch (w) {
    case Wave::Square: return phase < duty ? 1.0f : -1.0f;
    case Wave::Triangle: return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
    case Wave::Saw: return 2.0f * phase - 1.0f;
    case Wave::Sine: return std::sin(TAU * phase);
    case Wave::Noise: return noise;
    }
    return 0.0f;
}

/// Adds one play of the layer into out from sample start, gain loud.
void addLayer(std::vector<float>& out, const Layer& l, size_t start, float loud, Rng& rng) {
    float length = l.attack + l.hold + l.decay;
    size_t n = size_t(length * SAMPLE_RATE);
    float phase = 0.0f, noise = rng.range(-1.0f, 1.0f), wander = 0.0f, wanderTo = 0.0f;
    float low = 0.0f, high = 0.0f;
    float lowStep = l.lowpass > 0.0f ? smoothing(l.lowpass) : 1.0f;
    float highStep = l.highpass > 0.0f ? smoothing(l.highpass) : 0.0f;
    int perWobble = std::max(1, int(SAMPLE_RATE / WOBBLE_RATE));
    for (size_t i = 0; i < n && start + i < out.size(); i++) {
        float t = float(i) / SAMPLE_RATE;
        float env = t < l.attack ? t / std::max(l.attack, 1e-6f)
                    : t < l.attack + l.hold ? 1.0f
                                            : 1.0f - (t - l.attack - l.hold) / std::max(l.decay, 1e-6f);
        // Falling away along a curve sounds struck, not switched off.
        if (t >= l.attack + l.hold) env *= env;
        if (l.wobble > 0.0f && int(i) % perWobble == 0) wanderTo = rng.range(-1.0f, 1.0f);
        wander += (wanderTo - wander) * 0.01f;
        float hz = l.from * std::pow(l.to / l.from, t / std::max(length, 1e-6f));
        hz = std::clamp(hz * (1.0f + l.wobble * wander), MIN_HZ, MAX_HZ);
        float next = phase + hz / SAMPLE_RATE;
        // Noise takes a new value every half period.
        if (std::floor(next * 2.0f) != std::floor(phase * 2.0f)) noise = rng.range(-1.0f, 1.0f);
        phase = next - std::floor(next);
        float s = waveAt(l.wave, phase, l.duty, noise);
        low += (s - low) * lowStep;
        s = low;
        if (l.highpass > 0.0f) {
            high += (s - high) * highStep;
            s -= high;
        }
        out[start + i] += s * env * loud;
    }
}

} // namespace

std::vector<Def> parse(std::istream& in, const std::string& source) {
    std::vector<Def> defs;
    int bits = Def{}.bits;
    float rate = Def{}.rate;
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
        if (head == "sound") {
            Def d;
            d.bits = bits, d.rate = rate;
            if (!(words >> d.name)) {
                warn(source, number, "a sound wants a name");
                continue;
            }
            std::string key;
            float v;
            while (words >> key >> v) {
                if (key == "gain") d.gain = v;
                else if (key == "bits") d.bits = int(v);
                else if (key == "rate") d.rate = v;
                else warn(source, number, "no sound option " + key);
            }
            defs.push_back(std::move(d));
            continue;
        }
        Layer l;
        if (!waveNamed(head, l.wave)) {
            warn(source, number, "no wave " + head);
            continue;
        }
        if (defs.empty()) {
            warn(source, number, "a layer before any sound");
            continue;
        }
        if (!(words >> l.from)) {
            warn(source, number, "a layer wants a pitch");
            continue;
        }
        l.to = l.from;
        std::string key;
        if (words >> key) {
            // A second number is the pitch it slides to.
            try {
                size_t used;
                float to = std::stof(key, &used);
                if (used == key.size()) l.to = to, key.clear();
            } catch (...) {
            }
            float v;
            if (!key.empty() && words >> v && !layerOption(l, key, v)) warn(source, number, "no layer option " + key);
            while (words >> key >> v)
                if (!layerOption(l, key, v)) warn(source, number, "no layer option " + key);
        }
        l.from = std::max(l.from, MIN_HZ), l.to = std::max(l.to, MIN_HZ);
        defs.back().layers.push_back(l);
    }
    return defs;
}

std::vector<float> render(const Def& def, float lowpass) {
    float length = 0.0f;
    for (const Layer& l : def.layers)
        length = std::max(length, l.delay + (l.repeat - 1) * l.gap + l.attack + l.hold + l.decay);
    std::vector<float> out(size_t(length * SAMPLE_RATE) + 1, 0.0f);
    Rng rng(seedOf(def.name));
    for (const Layer& l : def.layers) {
        float loud = l.gain;
        for (int k = 0; k < l.repeat; k++, loud *= l.fade)
            addLayer(out, l, size_t((l.delay + k * l.gap) * SAMPLE_RATE), loud, rng);
    }
    crush(out, def.bits, def.rate, lowpass, def.gain);
    return out;
}

// Through a wall, then crushed: held for a whole crushed sample, and
// rounded to a few levels.
void crush(std::vector<float>& samples, int bits, float rate, float lowpass, float gain) {
    float lowStep = lowpass > 0.0f ? smoothing(lowpass) : 1.0f, low = 0.0f;
    float levels = float(1 << std::max(1, bits - 1));
    float every = SAMPLE_RATE / std::max(rate, 1.0f), held = 0.0f, since = every;
    for (float& s : samples) {
        low += (std::clamp(s * gain, -1.0f, 1.0f) - low) * lowStep;
        if ((since += 1.0f) >= every) since -= every, held = std::round(low * levels) / levels;
        s = std::clamp(held, -1.0f, 1.0f);
    }
}

} // namespace synth
