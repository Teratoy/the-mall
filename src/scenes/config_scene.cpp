#include "scenes/config_scene.h"
#include "audio/music.h"
#include "render/canvas.h"
#include "scenes/menu_scene.h"
#include "ui/pixel_ui.h"

void ConfigScene::update(float, const Canvas&) {
    music::want(music::MENU);
    if (ui::backPressed()) back_ = true;
    if (back_) next = std::make_unique<MenuScene>();
}

// BACK reports while drawing; it acts on the next update.
void ConfigScene::draw(const Canvas& canvas) {
    ClearBackground(ui::PAPER);
    back_ |= panel_.draw(canvas.mouse(), Canvas::W);
}
