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
    /// Blood thrown from a blow; size grows with how much flew.
    Spray,
    /// A drop of blood falling from a wound, or round a body pooling (size
    /// more than 1).
    Drip,
    /// A bite torn from a body.
    Bite,
    /// A punch thrown, landing or not.
    Swing,
    /// Piss and feces coming down, on a toilet, into water or on the floor:
    /// the view tells which from the cell.
    Piss,
    Feces,
    /// A gulp of water from a pool, a basin, a toilet or a tap, and a sip
    /// from a bottle.
    Gulp,
    Sip,
    /// Someone dropping where they stand, worn through.
    Collapse,
    /// A vending machine dropping what was paid for, and one with nothing left.
    VendDrop,
    VendEmpty,
    /// A key or a cake picked up, and a bottle taken from a pile.
    PickKey,
    PickCake,
    PickBottle,
    /// A fist banging on a locked door or a slam room's gate.
    Bang,
    /// A head beaten against a wall.
    Thud,
};

struct Noise {
    NoiseKind kind;
    Vec2i cell;
    /// Whose noise it was, or -1.
    int who = -1;
    /// How big it was, 1 as usual: the view makes it louder or quieter.
    float size = 1.0f;
};
