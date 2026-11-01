#pragma once
#include <string_view>

/// The music playing: a song of data/music.txt, streamed on and on with its
/// variations, at the MUSIC volume. Each scene says every frame what it
/// wants to hear; asking for the song already playing changes nothing, so
/// a menu made anew doesn't start it again. Where there is no sound device,
/// nothing plays and nothing fails.
namespace music {

/// The song of the menus, from the title to choosing the sleeper.
inline constexpr const char* MENU = "menu";

/// The song a scene wants, at level 0 to 1 (lower to duck it), or none
/// ("") to fade it out. Another song fades the one playing out first, then
/// starts from its first section.
void want(std::string_view song, float level = 1.0f);
/// Eases the level and feeds the stream, once a frame.
void update(float dt);
/// Reads data/music.txt again (F5); the song playing carries on from where
/// it was. False when it isn't found.
bool reload();
/// Before the sound device closes.
void close();

} // namespace music
