#pragma once
#include "core/rng.h"
#include "human/human_look.h"
#include "human/traits.h"
#include "render/human_sprites.h"
#include "scenes/scene.h"
#include <string>

/// Making a human: a freshly rolled one with their name and traits,
/// REFRESH to roll another, CONTINUE to send them into a new world, BACK
/// (or ESC) for the menu. Tapping the name lets you type one of your own
/// (ENTER or a tap elsewhere ends it), which REFRESH then keeps; leaving
/// it empty brings back the rolled one.
class CreateScene : public Scene {
public:
    explicit CreateScene(uint64_t seed);

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    Rng rng_;
    HumanLook look_;
    /// The human's own seed, and the name and traits it gives them.
    uint64_t humanSeed_ = 0;
    std::string name_;
    /// Typed in; empty for the rolled name. Being typed while editing_.
    std::string chosen_;
    bool editing_ = false;
    bool startEdit_ = false;
    float caret_ = 0.0f;
    Traits traits_;
    std::unique_ptr<HumanSprites> sprites_;
    /// The preview stands, walks in place and waves in turn, turning each round.
    float showTime_ = 0.0f;
    int phase_ = 0;
    bool facingLeft_ = false;
    bool refresh_ = false;
    bool proceed_ = false;
    bool back_ = false;

    void roll();
    Pose pose() const;
};
