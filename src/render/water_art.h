#pragma once
#include <raylib.h>

/// Water: its colour, the glints kept close to it so it reads as still,
/// and the bank's shadow along its top.
inline constexpr Color WATER = {38, 66, 104, 255};
inline constexpr Color WATER_INK = {72, 106, 144, 255};
inline constexpr Color WATER_EDGE = {28, 48, 78, 255};

/// The colour of water this red (0 to 1) and this foul (0 to 1), clouded
/// as a pool or basin is: red first, then brown, each laid on as thickly.
/// Bottles and poured puddles show their water in it, so water looks the
/// same wherever it is.
Color waterColor(float red, float foul);
