#pragma once
#include "core/vec.h"

/// Something in the world that made a sound the player may hear, kept
/// for the view only (src/audio/hearing.cpp). Nothing in the sim reads
/// these: humans hear only what their own code says they do.
enum class NoiseKind : uint8_t {
    /// A key turned in a locked door or gate.
    Unlock,
    /// A server room's airlock door pushed open from inside, and falling shut.
    AirlockOpen,
    AirlockShut,
    /// A slam room's gate falling shut behind someone.
    Slam,
    /// Spikes driven up through someone's feet.
    Spikes,
    /// A dart let fly from a wall, and one striking someone.
    DartFire,
    DartHit,
    /// A blow landing.
    Hit,
    /// Someone falling dead.
    Death,
    /// A shut door's handle tried, rattling against its lock.
    Knob,
};

struct Noise {
    NoiseKind kind;
    Vec2i cell;
    /// Whose noise it was, or -1.
    int who = -1;
};
