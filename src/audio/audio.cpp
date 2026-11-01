#include "audio/audio.h"
#include "audio/synth.h"
#include "core/files.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <raylib.h>
#include <vector>

namespace audio {

namespace {

/// How many of one sound can play at once; the oldest is cut for another.
constexpr int VOICES = 6;
/// Through a wall a sound keeps only what is under this, in Hz.
constexpr float MUFFLE_HZ = 450.0f;
constexpr const char* SETTINGS_FILE = "settings.txt";
/// The crush a voice gets: lighter than data/sounds.txt's, so its
/// consonants come through.
constexpr int VOICE_BITS = 8;
constexpr float VOICE_RATE = 16000.0f;

/// A sound made ready to play, clear and muffled, each with its voices.
struct Entry {
    Sound clear{}, muffled{};
    std::vector<Sound> clearVoices, muffledVoices;
    int next = 0;
};

bool opened = false;
bool loaded = false;
Settings current;
std::map<std::string, Entry, std::less<>> table;
std::set<std::string, std::less<>> missing;
/// Voices playing, each its own sound, let go once it has finished.
std::vector<Sound> talking;

/// Where the file may be: named by THEMALL_SOUNDS, beside where the game
/// was started, or beside the game itself.
std::vector<std::filesystem::path> places() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_SOUNDS")) p.emplace_back(env);
    p.emplace_back("data/sounds.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/sounds.txt");
        p.push_back(exe.parent_path() / "data/sounds.txt");
    }
    return p;
}

Sound soundOf(const std::vector<float>& samples) {
    std::vector<int16_t> pcm(samples.size());
    for (size_t i = 0; i < samples.size(); i++) pcm[i] = int16_t(std::lround(samples[i] * 32767.0f));
    Wave w{};
    w.frameCount = unsigned(pcm.size());
    w.sampleRate = synth::SAMPLE_RATE;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    return LoadSoundFromWave(w);
}

void unload(Entry& e) {
    for (Sound& s : e.clearVoices) UnloadSoundAlias(s);
    for (Sound& s : e.muffledVoices) UnloadSoundAlias(s);
    UnloadSound(e.clear);
    UnloadSound(e.muffled);
}

void store(const std::vector<synth::Def>& defs) {
    for (auto& [name, e] : table) unload(e);
    table.clear();
    missing.clear();
    for (const synth::Def& d : defs) {
        Entry e;
        e.clear = soundOf(synth::render(d));
        e.muffled = soundOf(synth::render(d, MUFFLE_HZ));
        for (int i = 0; i < VOICES; i++) {
            e.clearVoices.push_back(LoadSoundAlias(e.clear));
            e.muffledVoices.push_back(LoadSoundAlias(e.muffled));
        }
        if (auto old = table.find(d.name); old != table.end()) unload(old->second), table.erase(old);
        table.emplace(d.name, std::move(e));
    }
}

/// A volume step heard as its square, which sounds more even across the steps.
float gainOf(int step) {
    float v = std::clamp(step, 0, VOLUME_STEPS) / float(VOLUME_STEPS);
    return v * v;
}

void loadSettings() {
    std::ifstream in(files::savePath(SETTINGS_FILE));
    std::string key;
    int v;
    while (in >> key >> v) {
        v = std::clamp(v, 0, VOLUME_STEPS);
        if (key == "master") current.master = v;
        else if (key == "sounds") current.sounds = v;
        else if (key == "voices") current.voices = v;
    }
}

} // namespace

void open() {
    loadSettings();
    InitAudioDevice();
    opened = IsAudioDeviceReady();
    if (!opened) std::fprintf(stderr, "no sound device: the game will be silent\n");
}

void close() {
    if (!opened) return;
    for (auto& [name, e] : table) unload(e);
    table.clear();
    for (Sound& s : talking) UnloadSound(s);
    talking.clear();
    CloseAudioDevice();
    opened = false;
}

bool reload() {
    if (!opened) return false;
    loaded = true;
    if (auto in = files::asset("data/sounds.txt")) {
        store(synth::parse(*in, "data/sounds.txt"));
        return true;
    }
    for (const auto& path : places()) {
        std::ifstream in(path);
        if (!in) continue;
        store(synth::parse(in, path.string()));
        return true;
    }
    std::fprintf(stderr, "no data/sounds.txt found: the game will be silent\n");
    return false;
}

void play(std::string_view name, float volume, float pan, float pitch, bool muffled) {
    if (!opened) return;
    if (!loaded) reload();
    auto it = table.find(name);
    if (it == table.end()) {
        if (missing.emplace(name).second)
            std::fprintf(stderr, "no sound %.*s in data/sounds.txt\n", int(name.size()), name.data());
        return;
    }
    float v = volume * gainOf(current.master) * gainOf(current.sounds);
    if (v <= 0.0f) return;
    Entry& e = it->second;
    std::vector<Sound>& voices = muffled ? e.muffledVoices : e.clearVoices;
    Sound& s = voices[size_t(e.next)];
    e.next = (e.next + 1) % VOICES;
    SetSoundVolume(s, std::min(v, 1.0f));
    SetSoundPan(s, std::clamp(pan, 0.0f, 1.0f));
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void speak(const std::vector<float>& samples, float volume, float pan, bool muffled) {
    if (!opened || samples.empty()) return;
    std::erase_if(talking, [](Sound& s) {
        if (IsSoundPlaying(s)) return false;
        UnloadSound(s);
        return true;
    });
    float v = volume * gainOf(current.master) * gainOf(current.voices);
    if (v <= 0.0f) return;
    std::vector<float> crushed = samples;
    synth::crush(crushed, VOICE_BITS, VOICE_RATE, muffled ? MUFFLE_HZ : 0.0f);
    Sound s = soundOf(crushed);
    SetSoundVolume(s, std::min(v, 1.0f));
    SetSoundPan(s, std::clamp(pan, 0.0f, 1.0f));
    PlaySound(s);
    talking.push_back(s);
}

Settings& settings() { return current; }

void save() {
    std::ofstream out(files::savePath(SETTINGS_FILE));
    out << "master " << current.master << "\nsounds " << current.sounds << "\nvoices " << current.voices << "\n";
}

} // namespace audio
