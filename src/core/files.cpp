#include "core/files.h"
#include <raylib.h>

#if defined(PLATFORM_ANDROID)
#include <android_native_app_glue.h>
// raylib's Android platform hands out the app it was started with.
extern "C" struct android_app* GetAndroidApp(void);
#endif

namespace files {

std::optional<std::istringstream> asset([[maybe_unused]] const char* name) {
#if defined(PLATFORM_ANDROID)
    // raylib's file reading looks in the assets first on Android.
    if (char* text = LoadFileText(name)) {
        std::istringstream in{std::string(text)};
        UnloadFileText(text);
        return in;
    }
#endif
    return std::nullopt;
}

std::string savePath(const char* name) {
#if defined(PLATFORM_ANDROID)
    return std::string(GetAndroidApp()->activity->internalDataPath) + "/" + name;
#else
    return std::string("data/") + name;
#endif
}

} // namespace files
