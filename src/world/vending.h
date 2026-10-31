#pragma once
#include "core/vec.h"
#include <cstdint>

/// What a machine sells: cakes, empty bottles, or guns (none stand in the
/// mall yet: guns do nothing so far).
enum class Vend : uint8_t { Cake, Bottle, Gun, Count };

/// Keys fed in for one item, cake or bottle alike (user).
inline constexpr int MACHINE_PRICE = 3;
/// Items a machine holds when full.
inline constexpr int MACHINE_SLOTS = 6;
/// Seconds of world time in a day, about a waking stretch and a sleep: each
/// machine gets one item back a day (user).
inline constexpr float MACHINE_DAY = 1200.0f;

/// A vending machine stands two cells long against a wall, as tall as a
/// human, and blocks walking but not sight. Whoever stands in front of it
/// with keys enough feeds them in and takes what drops. Its stock shows
/// behind the glass, filled from the top shelf down, and comes back one
/// item a day.
struct Machine {
    Vend kind;
    /// Its top-left cell of the two.
    Vec2i at;
    /// The way it faces, away from its wall: down toward the viewer against
    /// a north wall, shown from the front; east or west against a side
    /// wall, shown from the side.
    Vec2i facing;
    /// How many items it still holds.
    int stock = 0;
    /// Seconds until it gets one item back.
    float restock = 0.0f;

    /// From its first cell to its second, along the wall.
    Vec2i along() const { return facing.y != 0 ? Vec2i{1, 0} : Vec2i{0, 1}; }
    bool covers(Vec2i cell) const { return cell == at || cell == at + along(); }
    /// Whether it is worked from the floor cell: one of the two in front of it.
    bool usedFrom(Vec2i cell) const { return covers(cell - facing); }
    /// Seen from the side, as against a west or east wall.
    bool side() const { return facing.x != 0; }
    /// The slots filled, as the art draws them: from the top shelf down.
    uint8_t stockBits() const { return uint8_t((1u << stock) - 1); }
};
