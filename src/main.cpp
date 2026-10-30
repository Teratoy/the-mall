#include "render/canvas.h"
#include "scenes/menu_scene.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace {
/// The window first opens at twice the phone canvas, then grows to the wide
/// canvas as large as fits this share of the monitor.
constexpr int WINDOW_SCALE = 2;
constexpr float MONITOR_SHARE = 0.9f;

/// Opens the window wide, for the PC layout, in whole steps where it can.
void fitWindowWide() {
    int monitor = GetCurrentMonitor();
    int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
    if (mw <= 0 || mh <= 0) return;
    float s = MONITOR_SHARE * std::min(mw / float(Canvas::WIDE_W), mh / float(Canvas::H));
    if (s >= 2.0f) s = std::floor(s);
    int w = int(Canvas::WIDE_W * s), h = int(Canvas::H * s);
    SetWindowSize(w, h);
    SetWindowPosition((mw - w) / 2, (mh - h) / 2);
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Canvas::W * WINDOW_SCALE, Canvas::H * WINDOW_SCALE, "The Mall");
    fitWindowWide();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    {
        Canvas canvas;
        std::unique_ptr<Scene> scene = std::make_unique<MenuScene>();
        while (!WindowShouldClose()) {
            float dt = std::min(GetFrameTime(), 0.1f);
            canvas.fit(scene->canBeWide());
            scene->update(dt, canvas);
            if (scene->next) scene = std::move(scene->next);
            canvas.begin();
            scene->draw(canvas);
            canvas.end();
            BeginDrawing();
            ClearBackground(BLACK);
            canvas.present();
            EndDrawing();
        }
    }
    CloseWindow();
}
