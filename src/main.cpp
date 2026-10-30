#include "render/canvas.h"
#include "scenes/menu_scene.h"
#include <algorithm>
#include <raylib.h>

namespace {
/// The window starts at twice the canvas: a phone held upright.
constexpr int WINDOW_SCALE = 2;
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Canvas::W * WINDOW_SCALE, Canvas::H * WINDOW_SCALE, "The Mall");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    {
        Canvas canvas;
        std::unique_ptr<Scene> scene = std::make_unique<MenuScene>();
        while (!WindowShouldClose()) {
            float dt = std::min(GetFrameTime(), 0.1f);
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
