// Writes the game's icon where the computer and the phone pick it up, from
// the same spiral the window draws (src/render/icon_art.cpp):
//   data/icon.png                       for the desktop shortcut
//   android/res/drawable/ic_spiral.xml  the spiral as a vector, for the APK's launcher icon
// make icon runs it; commit what it writes, since make apk runs on the host without raylib.
#include "render/icon_art.h"
#include "ui/pixel_ui.h"
#include <cstdio>
#include <raylib.h>

namespace {

/// The desktop icon is the window's icon drawn this many times over again.
constexpr int DESKTOP_SCALE = 4;
/// An adaptive icon's layers are 108 dp across; a circle mask keeps the middle 66,
/// so the spiral is drawn 45 across to keep its corners inside it.
constexpr int LAYER_DP = 108;
constexpr int CELL_DP = 3;

/// Writes the spiral as one vector path, a rectangle for each run of wall along a row.
bool writeVector(const char* path) {
    FILE* f = std::fopen(path, "w");
    if (!f) return false;
    float origin = (LAYER_DP - ICON_PX * CELL_DP) / 2.0f;
    std::fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<!-- The square spiral, written by tools/icon.cpp (make icon): don't edit by hand. -->\n"
                    "<vector xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                    "        android:width=\"%ddp\" android:height=\"%ddp\"\n"
                    "        android:viewportWidth=\"%d\" android:viewportHeight=\"%d\">\n"
                    "    <path android:fillColor=\"#%02X%02X%02X\"\n"
                    "          android:pathData=\"",
                 LAYER_DP, LAYER_DP, LAYER_DP, LAYER_DP, ui::BRIGHT.r, ui::BRIGHT.g, ui::BRIGHT.b);
    for (int y = 0; y < ICON_PX; ++y)
        for (int x = 0; x < ICON_PX;) {
            if (!iconWall(x, y)) { ++x; continue; }
            int run = 0;
            while (x + run < ICON_PX && iconWall(x + run, y)) ++run;
            std::fprintf(f, "M%g,%gh%dv%dh%dz", origin + x * CELL_DP, origin + y * CELL_DP, run * CELL_DP, CELL_DP,
                         -run * CELL_DP);
            x += run;
        }
    std::fprintf(f, "\" />\n</vector>\n");
    return std::fclose(f) == 0;
}

} // namespace

int main() {
    SetTraceLogLevel(LOG_WARNING);
    Image icon = makeIcon();
    ImageResizeNN(&icon, icon.width * DESKTOP_SCALE, icon.height * DESKTOP_SCALE);
    bool ok = ExportImage(icon, "data/icon.png");
    UnloadImage(icon);
    ok = writeVector("android/res/drawable/ic_spiral.xml") && ok;
    std::puts(ok ? "icon: wrote data/icon.png and android/res/drawable/ic_spiral.xml" : "icon: could not write");
    return ok ? 0 : 1;
}
