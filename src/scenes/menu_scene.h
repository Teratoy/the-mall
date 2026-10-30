#pragma once
#include "scenes/scene.h"

/// The first screen: the game's name, ENTER THE MALL to make a human
/// (or ENTER), and OBITUARY for the humans you made whose runs are over.
class MenuScene : public Scene {
public:
    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    bool enter_ = false;
    bool obituary_ = false;
};
