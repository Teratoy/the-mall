#include "scenes/menu_scene.h"
#include "render/canvas.h"
#include "scenes/create_scene.h"
#include "scenes/obituary_scene.h"
#include "ui/pixel_ui.h"
#include <ctime>

namespace {

/// The name, in the default font at three times the UI's size.
constexpr const char* TITLE = "THE MALL";
constexpr float TITLE_SIZE = 30.0f, TITLE_SPACING = 3.0f;
constexpr int TITLE_Y = 150;
constexpr int BUTTON_W = 120, BUTTON_H = 24, BUTTON_GAP = 12;
constexpr int BUTTONS_Y = 260;
constexpr Rectangle ENTER{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y, BUTTON_W, BUTTON_H};
constexpr Rectangle OBITUARY{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y + BUTTON_H + BUTTON_GAP, BUTTON_W, BUTTON_H};

} // namespace

void MenuScene::update(float, const Canvas&) {
    if (IsKeyPressed(KEY_ENTER)) enter_ = true;
    if (enter_) next = std::make_unique<CreateScene>(uint64_t(std::time(nullptr)));
    else if (obituary_) next = std::make_unique<ObituaryScene>();
}

// Buttons report clicks while drawing; they act on the next update.
void MenuScene::draw(const Canvas& canvas) {
    ClearBackground(ui::PAPER);
    Font font = GetFontDefault();
    Vector2 size = MeasureTextEx(font, TITLE, TITLE_SIZE, TITLE_SPACING);
    DrawTextEx(font, TITLE, {(Canvas::W - size.x) / 2, float(TITLE_Y)}, TITLE_SIZE, TITLE_SPACING, ui::BRIGHT);
    Vector2 m = canvas.mouse();
    enter_ |= ui::button(ENTER, "ENTER THE MALL", m);
    obituary_ |= ui::button(OBITUARY, "OBITUARY", m);
}
