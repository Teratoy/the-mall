// Renders every sound in data/sounds.txt to a .wav in a folder, clear and
// muffled (as through a wall), to listen to, and to cmp before and after a
// change to the synth. Prints each sound's name and length.
//   build/sound_sheet [sounds.txt] [out dir]
// The sound editor (tools/sound_editor.py) uses its other way: the first
// sound in what comes in on stdin, as a .wav on stdout, dulled under
// LOWPASS Hz when given. Lines it can't make out go to stderr.
//   build/sound_sheet --one [LOWPASS] < sound.txt > sound.wav
// The music: each section of each song in data/music.txt alone, and each
// song as the game would play it for SECONDS (180), to a folder.
//   build/sound_sheet --music [music.txt] [out dir] [SECONDS]
// And for the sound editor, one song from stdin to stdout: a section of it
// alone, the song as played from its start (-), or from a section
// (from:SECTION), for SECONDS.
//   build/sound_sheet --music-one SONG SECTION|-|from:SECTION [SECONDS] < music.txt > song.wav
// No raylib, so it runs on the host as well as in the toolbox.
#include "audio/music_data.h"
#include "audio/synth.h"
#include <algorithm>
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

/// The tape's dice in the sheets, so a sheet is the same each time.
constexpr uint64_t TAPE_SEED = 1;
/// How long a song plays in its sheet, in seconds, unless told.
constexpr float SONG_SECONDS = 180.0f;

/// A section alone, every channel, through to the end of its tail, crushed.
std::vector<float> sectionAlone(const music::Song& song, int section) {
    music::Rendered r = music::render(song, section);
    std::vector<float> out(r.tail, 0.0f);
    for (const auto& ch : r.channels)
        for (size_t i = 0; i < ch.size(); i++) out[i] += ch[i];
    synth::Crush crush(song.bits, song.rate);
    for (float& s : out) s = crush(std::clamp(s * song.gain, -1.0f, 1.0f));
    return out;
}

/// The song as the game plays it, from a section, for seconds.
std::vector<float> songPlayed(const music::Song& song, const std::string& from, float seconds) {
    music::Tape tape(song, TAPE_SEED);
    if (!from.empty()) tape.seek(from, 0.0);
    std::vector<float> out(size_t(seconds * synth::SAMPLE_RATE));
    tape.fill(out.data(), out.size());
    return out;
}

int musicSheet(const std::string& from, const std::filesystem::path& dir, float seconds) {
    std::ifstream in(from);
    if (!in) {
        std::fprintf(stderr, "can't read %s\n", from.c_str());
        return 1;
    }
    std::filesystem::create_directories(dir);
    for (const music::Song& song : music::parse(in, from)) {
        for (size_t i = 0; i < song.sections.size(); i++) {
            std::vector<float> s = sectionAlone(song, int(i));
            std::string name = song.name + "-" + song.sections[i].name;
            if (!writeWav(dir / (name + ".wav"), s)) return std::fprintf(stderr, "can't write %s\n", name.c_str()), 1;
            std::printf("%-20s %.1f s\n", name.c_str(), double(s.size()) / synth::SAMPLE_RATE);
        }
        if (!writeWav(dir / (song.name + ".wav"), songPlayed(song, "", seconds)))
            return std::fprintf(stderr, "can't write %s\n", song.name.c_str()), 1;
        std::printf("%-20s %.1f s as played\n", song.name.c_str(), double(seconds));
    }
    return 0;
}

/// One song from stdin to stdout, for the sound editor.
int musicOne(const std::string& name, const std::string& section, float seconds) {
    for (const music::Song& song : music::parse(std::cin, "stdin")) {
        if (song.name != name) continue;
        if (song.sections.empty()) break;
        if (section == "-") return writeWav(std::cout, songPlayed(song, "", seconds)) ? 0 : 1;
        for (size_t i = 0; i < song.sections.size(); i++)
            if (song.sections[i].name == section) return writeWav(std::cout, sectionAlone(song, int(i))) ? 0 : 1;
        if (section.rfind("from:", 0) == 0) return writeWav(std::cout, songPlayed(song, section.substr(5), seconds)) ? 0 : 1;
        std::fprintf(stderr, "no section %s in song %s\n", section.c_str(), name.c_str());
        return 1;
    }
    std::fprintf(stderr, "no song %s with sections on stdin\n", name.c_str());
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--one") return renderOne(argc > 2 ? std::stof(argv[2]) : 0.0f);
    if (argc > 1 && std::string(argv[1]) == "--music")
        return musicSheet(argc > 2 ? argv[2] : "data/music.txt", argc > 3 ? argv[3] : "build/music",
                          argc > 4 ? std::stof(argv[4]) : SONG_SECONDS);
    if (argc > 3 && std::string(argv[1]) == "--music-one")
        return musicOne(argv[2], argv[3], argc > 4 ? std::stof(argv[4]) : SONG_SECONDS);
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
