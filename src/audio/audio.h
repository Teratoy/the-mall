#pragma once
#include <string_view>

/// The game's sound: the sounds of data/sounds.txt, rendered once and
/// played at a volume, pan and pitch, clear or muffled as through a wall
/// (the music is in audio/music.h); and the volumes the player set in CONFIGURATION, kept between runs.
/// Where there is no sound device, or it was never opened (the tools),
/// nothing plays and nothing fails.
namespace audio {

/// The volume steps in CONFIGURATION, 0 silent to VOLUME_STEPS full.
inline constexpr int VOLUME_STEPS = 10;

struct Settings {
    int master = 8;
    int sounds = 8;
    int music = 6;
};

/// Opens the sound device and reads the settings.
void open();
void close();
/// Reads data/sounds.txt again (F5 in the game). False when it isn't found.
bool reload();
/// Plays a sound of that name, volume 0 to 1 before the settings, pan 0.5
/// in the middle, 1 all left, 0 all right, pitch 1 as made. An unknown
/// name is reported once and plays nothing.
void play(std::string_view name, float volume = 1.0f, float pan = 0.5f, float pitch = 1.0f, bool muffled = false);

Settings& settings();
/// A volume step as a gain, heard as its square, which sounds more even
/// across the steps.
float gainOf(int step);
/// True once the sound device is open and working.
bool ready();
/// Writes the settings to the save file, after a change.
void save();

} // namespace audio
