#pragma once
#include "human/human_look.h"
#include <array>

/// Blood on a human, a pixel at a time on its standing pose, which every
/// frame maps back to, so it stays put on the body whatever it does. Wet
/// blood runs down and comes off on what it touches; dry blood stays.
struct BodyBlood {
    std::array<uint8_t, SPRITE_PX> amount{};
    /// 1 fresh, 0 dry, a pixel at a time.
    std::array<float, SPRITE_PX> wet{};
    /// The body part each pixel of the standing pose is, or NO_PART.
    std::array<uint8_t, SPRITE_PX> parts;
    /// Changes whenever the blood looks different, so sprites know to redraw.
    int version = 0;

    explicit BodyBlood(const HumanLook& look) : parts(look.idle().part) {}

    bool any() const;
    /// Some of it is still wet.
    bool wetAny() const;
    /// All of it together, 1 being the whole body as thick as can be; with
    /// feet false, only what is above the feet, which any walk through a
    /// stain gets.
    float total(bool feet = true) const;
    /// What is on the feet alone, 1 being the feet as thick as can be.
    float feet() const;
    /// How much of it is still wet, by ink: 1 all fresh, 0 all dry or none.
    float freshness() const;
    /// As total(), but by how much the blood says where it is: round the
    /// mouth and on the hands far more than on the shoes.
    float telling() const;
};
