#pragma once
#include <optional>
#include <sstream>
#include <string>

/// Where the game's files are. On a computer they sit in data/ beside where
/// the game was started; on a phone the shipped ones are inside the APK and
/// the saved ones in the app's own storage.
namespace files {

/// True where there is no keyboard, only touch: the Android build.
#if defined(PLATFORM_ANDROID)
inline constexpr bool TOUCH_ONLY = true;
#else
inline constexpr bool TOUCH_ONLY = false;
#endif

/// A file the game ships with, such as "data/lines.txt", read from the
/// APK's assets. Always empty off Android, where callers look on disk.
std::optional<std::istringstream> asset(const char* name);
/// Where a file the game writes, such as "obituary.txt", is kept.
std::string savePath(const char* name);

} // namespace files
