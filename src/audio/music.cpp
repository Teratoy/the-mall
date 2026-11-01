#include "audio/music.h"
#include "audio/audio.h"
#include "audio/music_data.h"
#include "core/files.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <raylib.h>
#include <vector>

namespace music {

namespace {

/// Samples fed to the stream at a time; it holds two of these.
constexpr int BUFFER = 4096;
/// Seconds to fade a song out to silence (on WAKE UP), and to rise or fall
/// to another level (fading in, ducking in OBITUARY), each over the whole way.
constexpr float FADE_OUT = 3.0f, FADE = 1.0f;

bool loaded = false;
std::vector<Song> songs;
AudioStream stream{};
bool streaming = false;
std::unique_ptr<Tape> tape;
/// What the scenes want, and how loud it is now, 0 to 1.
std::string wanted, playing;
/// A song asked for that the file doesn't have, reported once.
std::string missing;
float wantedLevel = 1.0f, level = 0.0f;
/// The tapes' dice: view only, never the same twice.
Rng dice(uint64_t(std::time(nullptr)) ^ 0x6d75736963ULL);

/// Where the file may be: named by THEMALL_MUSIC, beside where the game was
/// started, or beside the game itself.
std::vector<std::filesystem::path> places() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_MUSIC")) p.emplace_back(env);
    p.emplace_back("data/music.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/music.txt");
        p.push_back(exe.parent_path() / "data/music.txt");
    }
    return p;
}

const Song* songNamed(std::string_view name) {
    for (const Song& s : songs)
        if (s.name == name) return &s;
    return nullptr;
}

/// Starts the song from its first section, silent, to fade in.
void startSong(const std::string& name) {
    const Song* s = songNamed(name);
    if (!s || s->sections.empty()) {
        if (missing != name) std::fprintf(stderr, "no song %s in data/music.txt\n", name.c_str());
        missing = name;
        return;
    }
    tape = std::make_unique<Tape>(*s, dice.next());
    playing = name;
    level = 0.0f;
    if (!streaming) {
        SetAudioStreamBufferSizeDefault(BUFFER);
        stream = LoadAudioStream(synth::SAMPLE_RATE, 16, 1);
        streaming = true;
    }
    SetAudioStreamVolume(stream, 0.0f);
    PlayAudioStream(stream);
}

void stop() {
    if (!tape) return;
    tape.reset();
    playing.clear();
    StopAudioStream(stream);
}

/// Tops the stream up from the tape while it has room. On Android the game's
/// loop stops while the app is in the background, so nothing feeds it: the
/// stream plays out what it holds and falls silent, and carries on from the
/// same place in the tape on coming back.
void feed() {
    static std::vector<float> samples(BUFFER);
    static std::vector<int16_t> pcm(BUFFER);
    while (IsAudioStreamProcessed(stream)) {
        tape->fill(samples.data(), samples.size());
        for (size_t i = 0; i < samples.size(); i++) pcm[i] = int16_t(std::lround(samples[i] * 32767.0f));
        UpdateAudioStream(stream, pcm.data(), BUFFER);
    }
}

} // namespace

void want(std::string_view song, float level) {
    wanted = song;
    wantedLevel = std::clamp(level, 0.0f, 1.0f);
}

void update(float dt) {
    if (!audio::ready()) return;
    if (!loaded) reload();
    // Another song, or none: fade out what plays first.
    if (playing != wanted) {
        if (!playing.empty()) level = std::max(0.0f, level - dt / FADE_OUT);
        if (playing.empty() || level <= 0.0f) {
            stop();
            if (!wanted.empty() && wanted != missing) startSong(wanted);
        }
    } else if (level < wantedLevel) {
        level = std::min(wantedLevel, level + dt / FADE);
    } else if (level > wantedLevel) {
        level = std::max(wantedLevel, level - dt / FADE);
    }
    if (!tape) return;
    const audio::Settings& s = audio::settings();
    SetAudioStreamVolume(stream, level * audio::gainOf(s.master) * audio::gainOf(s.music));
    feed();
}

bool reload() {
    loaded = true;
    std::vector<Song> read;
    if (auto in = files::asset("data/music.txt")) {
        read = parse(*in, "data/music.txt");
    } else {
        bool found = false;
        for (const auto& path : places()) {
            std::ifstream in(path);
            if (!in) continue;
            read = parse(in, path.string());
            found = true;
            break;
        }
        if (!found) {
            std::fprintf(stderr, "no data/music.txt found: there will be no music\n");
            return false;
        }
    }
    songs = std::move(read);
    missing.clear();
    // The song playing carries on from where it was, as now written.
    if (tape) {
        const Song* s = songNamed(playing);
        if (!s || s->sections.empty()) return stop(), true;
        std::string section = tape->sectionName();
        double at = tape->position();
        tape = std::make_unique<Tape>(*s, dice.next());
        tape->seek(section, at);
    }
    return true;
}

void close() {
    tape.reset();
    if (streaming) UnloadAudioStream(stream);
    streaming = false;
}

} // namespace music
