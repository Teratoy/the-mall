#pragma once
#include "render/floor_tiles.h"
#include "render/statue_art.h"
#include "scenes/scene.h"
#include "world/world.h"
#include <raylib.h>

/// The first screen: the game's name, ENTER THE MALL to make a human
/// (or ENTER), OBITUARY for the humans you made whose runs are over, and
/// CONFIGURATION for the settings. Behind them an empty mall, grown fresh
/// each time, scrolls slowly south for ever.
class MenuScene : public Scene {
public:
    MenuScene();

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    /// The mall behind the menu. Nobody in it ever moves; it only grows
    /// ahead of the view.
    World world_;
    FloorTiles floorTiles_;
    StatueArt statueArt_;
    /// Where the view looks, in world pixels.
    Vector2 focus_{};
    bool enter_ = false;
    bool obituary_ = false;
    bool configuration_ = false;
};
