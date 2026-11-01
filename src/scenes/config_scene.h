#pragma once
#include "scenes/config_panel.h"
#include "scenes/scene.h"

/// CONFIGURATION from the main menu, on its own screen; BACK (or ESC)
/// returns to the menu.
class ConfigScene : public Scene {
public:
    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    ConfigPanel panel_;
    bool back_ = false;
};
