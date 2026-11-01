// Renders every sound in data/sounds.txt to a .wav in a folder, clear and
// muffled (as through a wall), to listen to, and to cmp before and after a
// change to the synth. Prints each sound's name and length.
//   build/sound_sheet [sounds.txt] [out dir]
// The sound editor (tools/sound_editor.py) uses its other way: the first
// sound in what comes in on stdin, as a .wav on stdout, dulled under
// LOWPASS Hz when given. Lines it can't make out go to stderr.
//   build/sound_sheet --one [LOWPASS] < sound.txt > sound.wav
// No raylib, so it runs on the host as well as in the toolbox.
#include "audio/synth.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// The muffled copy keeps only what is under this, as the game's does.
constexpr float MUFFLE_HZ = 450.0f;

void put16(std::ostream& out, uint16_t v) { out.put(char(v & 0xff)).put(char(v >> 8)); }
void put32(std::ostream& out, uint32_t v) { put16(out, uint16_t(v & 0xffff)), put16(out, uint16_t(v >> 16)); }

/// A 16-bit mono .wav at the synth's rate.
bool writeWav(std::ostream& out, const std::vector<float>& samples) {
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

bool writeWav(const std::filesystem::path& path, const std::vector<float>& samples) {
    std::ofstream out(path, std::ios::binary);
    return out && writeWav(out, samples);
}

/// The first sound on stdin to stdout, for the sound editor.
int renderOne(float lowpass) {
    std::vector<synth::Def> defs = synth::parse(std::cin, "stdin");
    if (defs.empty()) {
        std::fprintf(stderr, "no sound on stdin\n");
        return 1;
    }
    return writeWav(std::cout, synth::render(defs[0], lowpass)) ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--one") return renderOne(argc > 2 ? std::stof(argv[2]) : 0.0f);
    std::string from = argc > 1 ? argv[1] : "data/sounds.txt";
    std::filesystem::path dir = argc > 2 ? argv[2] : "build/sounds";
    std::ifstream in(from);
    if (!in) {
        std::fprintf(stderr, "can't read %s\n", from.c_str());
        return 1;
    }
    std::filesystem::create_directories(dir);
    for (const synth::Def& d : synth::parse(in, from)) {
        std::vector<float> clear = synth::render(d), muffled = synth::render(d, MUFFLE_HZ);
        if (!writeWav(dir / (d.name + ".wav"), clear) || !writeWav(dir / (d.name + "_muffled.wav"), muffled)) {
            std::fprintf(stderr, "can't write %s\n", d.name.c_str());
            return 1;
        }
        std::printf("%-14s %.2f s\n", d.name.c_str(), double(clear.size()) / synth::SAMPLE_RATE);
    }
    return 0;
}
