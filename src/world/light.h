#pragma once
#include "core/vec.h"
#include <cstdint>

/// What gives off light. Fittings hang from the ceiling over rooms and
/// corridors, a few night lights stay on while the mall is closed, and
/// machines, screens and some statues glow on their own. A torch is
/// carried.
enum class LightKind : uint8_t { None, Fitting, NightLight, Vending, Screen, Rack, Dispenser, Statue, Torch };

/// How bright a cell can be: 0 is dark, LIGHT_FULL right by a source.
inline constexpr int LIGHT_FULL = 3;
/// No source shines further than this many cells.
inline constexpr int LIGHT_REACH = 8;

/// How far each kind of source shines, in cells.
constexpr int lightRadius(LightKind k) {
    switch (k) {
    case LightKind::Fitting: return 7;
    case LightKind::NightLight: return 3;
    case LightKind::Vending: return 3;
    case LightKind::Screen: return 2;
    case LightKind::Rack: return 2;
    case LightKind::Dispenser: return 1;
    case LightKind::Statue: return 6;
    case LightKind::Torch: return 5;
    case LightKind::None: break;
    }
    return 0;
}

/// Something that gives off light, from its cell, as far as its kind
/// shines. Walls, statues, racks and shut doors stop the light, as they
/// stop sight; open doors let it through to the next room.
struct LightSource {
    Vec2i cell;
    LightKind kind;
    bool on = true;
    /// A fitting on the blink: the view makes it stutter. The sim takes it as on.
    bool flicker = false;
};

/// How bright a source leaves a cell d cells away: LIGHT_FULL on its own
/// cell, falling in steps to 1 at the edge of its reach.
int lightLevel(LightKind kind, float d);
