// Renders sample voices to .wav files in a folder, to listen to, crushed
// as the game crushes them: a few lines said in each kind of voice
// (english, formant, villager) by a few kinds of speaker, then a few humans grown
// as the game grows them (human_N.wav), saying the lines in each tone.
// Prints each file, its length, and who the human is.
//   build/voice_sheet [out dir] [humans]
#include "audio/human_voice.h"
#include "audio/synth.h"
#include "audio/voice.h"
#include "core/rng.h"
#include "human/human.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

/// The crush the game's voices get (audio.cpp).
constexpr int CRUSH_BITS = 8;
constexpr float CRUSH_RATE = 16000.0f;

void put16(std::ostream& out, uint16_t v) { out.put(char(v & 0xff)).put(char(v >> 8)); }
void put32(std::ostream& out, uint32_t v) { put16(out, uint16_t(v & 0xffff)), put16(out, uint16_t(v >> 16)); }

/// A 16-bit mono .wav at the synth's rate.
bool writeWav(const std::filesystem::path& path, const std::vector<float>& samples) {
    std::ofstream out(path, std::ios::binary);
    uint32_t bytes = uint32_t(samples.size() * 2);
    out << "RIFF";
    put32(out, 36 + bytes);
    out << "WAVEfmt ";
    put32(out, 16);
    put16(out, 1);
    put16(out, 1);
    put32(out, synth::SAMPLE_RATE);
    put32(out, synth::SAMPLE_RATE * 2);
    put16(out, 2);
    put16(out, 16);
    out << "data";
    put32(out, bytes);
    for (float s : samples) put16(out, uint16_t(int16_t(std::lround(s * 32767.0f))));
    return bool(out);
}

struct Line {
    const char* name;
    const char* text;
};
constexpr Line LINES[] = {
    {"hello", "HELLO! NICE TO SEE A FACE."},
    {"where", "WHERE ARE WE? DO YOU KNOW THE WAY OUT?"},
    {"mine", "MINE!"},
    {"room", "I DON'T LIKE THIS ROOM."},
    {"food", "FOOD! I NEED FOOD!"},
};

/// Speakers of every sort: (name, pitch Hz, speed, size, brightness).
struct Speaker {
    const char* name;
    float pitch, speed, size, brightness;
};
constexpr Speaker SPEAKERS[] = {
    {"small", 470.0f, 1.2f, 1.18f, 0.8f},
    {"middling", 330.0f, 1.0f, 1.06f, 0.55f},
    {"big", 190.0f, 0.85f, 0.9f, 0.4f},
};

} // namespace

/// Each tone, named for the file.
constexpr const char* TONES[] = {"plain", "warm", "rude", "plead", "fear", "threat"};

int main(int argc, char** argv) {
    std::filesystem::path dir = argc > 1 ? argv[1] : "build/voices";
    int humans = argc > 2 ? std::stoi(argv[2]) : 6;
    std::filesystem::create_directories(dir);
    for (voice::Kind kind : {voice::Kind::English, voice::Kind::Formant, voice::Kind::Villager}) {
        const char* kindName = kind == voice::Kind::English ? "english" : kind == voice::Kind::Formant ? "formant" : "villager";
        for (const Speaker& sp : SPEAKERS) {
            voice::Voice v;
            v.kind = kind;
            v.pitch = sp.pitch, v.speed = sp.speed, v.size = sp.size, v.brightness = sp.brightness;
            // Every line of one speaker in one file, a pause between.
            std::vector<float> all;
            for (const Line& l : LINES) {
                std::vector<float> s = voice::speak(v, l.text);
                synth::crush(s, CRUSH_BITS, CRUSH_RATE);
                all.insert(all.end(), s.begin(), s.end());
                all.insert(all.end(), size_t(synth::SAMPLE_RATE / 2), 0.0f);
            }
            std::string name = std::string(kindName) + "_" + sp.name + ".wav";
            if (!writeWav(dir / name, all)) {
                std::fprintf(stderr, "can't write %s\n", name.c_str());
                return 1;
            }
            std::printf("%-22s %.2f s\n", name.c_str(), double(all.size()) / synth::SAMPLE_RATE);
        }
    }
    // Humans: the first two lines in every tone, a pause between.
    for (int i = 0; i < humans; i++) {
        Rng looks(uint64_t(i) * 7919u + 1u);
        Human h(i, HumanLook::random(looks), {0, 0}, looks.next());
        std::vector<float> all;
        for (int t = 0; t < int(std::size(TONES)); t++) {
            voice::Voice v = voice::of(h, talk::Tone(t));
            for (int l = 0; l < 2; l++) {
                std::vector<float> s = voice::speak(v, LINES[l].text);
                synth::crush(s, CRUSH_BITS, CRUSH_RATE);
                all.insert(all.end(), s.begin(), s.end());
                all.insert(all.end(), size_t(synth::SAMPLE_RATE / 3), 0.0f);
            }
            all.insert(all.end(), size_t(synth::SAMPLE_RATE / 3), 0.0f);
        }
        std::string name = "human_" + std::to_string(i) + ".wav";
        if (!writeWav(dir / name, all)) {
            std::fprintf(stderr, "can't write %s\n", name.c_str());
            return 1;
        }
        voice::Voice v = voice::of(h, talk::Tone::Plain);
        std::printf("%-22s %.2f s  %-10s torso %dx%d legs %d%s brave %.2f sociable %.2f greedy %.2f curious %.2f: "
                    "%.0f Hz, size %.2f, speed %.2f\n",
                    name.c_str(), double(all.size()) / synth::SAMPLE_RATE, h.name.c_str(), h.look.torsoW,
                    h.look.torsoH, h.look.legH, h.look.beard ? " beard" : "", double(h.traits.brave),
                    double(h.traits.sociable), double(h.traits.greedy), double(h.traits.curious), double(v.pitch),
                    double(v.size), double(v.speed));
    }
    return 0;
}
