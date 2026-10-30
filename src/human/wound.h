#pragma once
#include <cstdint>

/// What made a wound. Nothing slashes yet.
enum class WoundKind : uint8_t { Blunt, Pierce, Slash };
/// Where on the body a wound is, for where its blood shows.
enum class BodyPart : uint8_t { Head, Torso, FrontArm, BackArm, Legs, Feet };

/// One wound, bleeding health away until it heals. It bleeds hardest when
/// fresh, and less and less until healed, so the health it costs in all
/// grows with the square of the damage that made it: one deep wound bleeds
/// far more than a few scratches.
struct Wound {
    WoundKind kind;
    BodyPart part;
    /// All the damage that went into it, deepened or not.
    float damage = 0.0f;
    /// Health lost per second right now.
    float bleed = 0.0f;
    /// Seconds of healing left; bleeding runs out with it.
    float healLeft = 0.0f;
    /// False for darts: they never bleed a human to death.
    bool lethal = true;
    /// Who opened or last deepened it, -1 for traps: whoever's wound bleeds
    /// a human to death killed them.
    int by = -1;

    bool healed() const { return healLeft <= 0.0f; }
};
