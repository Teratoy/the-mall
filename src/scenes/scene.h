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
    /// Whether it can draw on the wide PC canvas; the rest stay phone-shaped.
    virtual bool canBeWide() const { return false; }

    std::unique_ptr<Scene> next;
};
