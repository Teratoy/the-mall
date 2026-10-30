#pragma once
#include <memory>

class Canvas;

/// One screen of the game. Setting next hands over to another scene
/// at the end of the frame.
class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(float dt, const Canvas& canvas) = 0;
    virtual void draw(const Canvas& canvas) = 0;

    std::unique_ptr<Scene> next;
};
