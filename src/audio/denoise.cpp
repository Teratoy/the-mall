#include "audio/denoise.h"
#include <cmath>
#include <raylib.h>

namespace {

/// The mix's samples a second. raylib opens the device at its own rate and
/// doesn't say which; nearly every one (PulseAudio, PipeWire, Android) runs
/// at this, and a 44100 one only moves the cut-off down a tenth.
constexpr float MIX_RATE = 48000.0f;
/// The low-pass's cut-off in Hz: just under half the crush's 8000 samples a
/// second, where the steps' hiss starts.
constexpr float CUT_HZ = 3800.0f;
/// The gate is open while the mix's level is above this: under it is only
/// the last flickering steps of crushed tails.
constexpr float GATE_OPEN = 0.004f;
/// How fast the gate follows the level up and lets it go, and how fast it
/// opens and closes, in seconds: quick to open so no click is lost, slow to
/// close so nothing is chopped.
constexpr float LEVEL_RISE = 0.001f, LEVEL_FALL = 0.08f, GATE_OPENS = 0.002f, GATE_CLOSES = 0.12f;
/// raylib mixes in stereo.
constexpr int CHANNELS = 2;

/// One second-order section of the low-pass, a channel's worth of state.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[CHANNELS]{}, z2[CHANNELS]{};

    void lowpass(float hz, float q) {
        float w = 2.0f * PI * hz / MIX_RATE, alpha = std::sin(w) / (2.0f * q), cosw = std::cos(w);
        float a0 = 1.0f + alpha;
        b0 = (1.0f - cosw) / 2.0f / a0, b1 = (1.0f - cosw) / a0, b2 = b0;
        a1 = -2.0f * cosw / a0, a2 = (1.0f - alpha) / a0;
    }

    float run(float x, int c) {
        float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
};

/// Two sections make a fourth-order Butterworth: flat below the cut-off,
/// falling steeply above it.
Biquad low[2];
float level = 0.0f, gate = 0.0f;

/// Each step's share of the way toward where it is going, for a time in seconds.
float follow(float seconds) { return 1.0f - std::exp(-1.0f / (seconds * MIX_RATE)); }

// On raylib's audio thread: frames of interleaved stereo floats.
void process(void* buffer, unsigned int frames) {
    static const float rise = follow(LEVEL_RISE), fall = follow(LEVEL_FALL);
    static const float opens = follow(GATE_OPENS), closes = follow(GATE_CLOSES);
    float* s = static_cast<float*>(buffer);
    for (unsigned int f = 0; f < frames; f++, s += CHANNELS) {
        float peak = 0.0f;
        for (int c = 0; c < CHANNELS; c++) {
            s[c] = low[1].run(low[0].run(s[c], c), c);
            peak = std::fmax(peak, std::fabs(s[c]));
        }
        level += (peak - level) * (peak > level ? rise : fall);
        float want = level > GATE_OPEN ? 1.0f : 0.0f;
        gate += (want - gate) * (want > gate ? opens : closes);
        for (int c = 0; c < CHANNELS; c++) s[c] *= gate;
    }
}

} // namespace

namespace denoise {

void attach() {
    low[0].lowpass(CUT_HZ, 0.5412f);
    low[1].lowpass(CUT_HZ, 1.3066f);
    AttachAudioMixedProcessor(process);
}

void detach() { DetachAudioMixedProcessor(process); }

} // namespace denoise
