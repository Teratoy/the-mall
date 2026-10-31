#pragma once
#include "core/color.h"

/// An old server rack stands two cells wide against a wall, a head taller
/// than a human: a dark steel cabinet with a tape drive, a bank of status
/// lights, two drive bays, a little green screen and its keyboard stacked inside.
inline constexpr int SERVER_RACK_W = 12;
inline constexpr int SERVER_RACK_H = 25;
/// Seen from the side it is this deep.
inline constexpr int SERVER_RACK_DEPTH = 10;

/// The rack's pixel at gx, gy seen from the front at the given tick of its
/// clock: the reels turn in bursts, the lights blink each at its own pace
/// and the screen scrolls; clear where nothing is drawn.
Rgba serverRackPixel(int gx, int gy, int tick);

/// Draws the rack with the bottom-left corner of its foot at canvas pixel
/// x, y, dimmed unless lit, ticking with the seconds gone. Remembered, it
/// holds still.
void drawServerRack(int x, int y, bool lit, float seconds);

/// The rack's pixel at gx, gy seen from its right side: a steel flank with
/// louvres and a maker's plate.
Rgba serverRackSidePixel(int gx, int gy);

/// Draws the rack seen from its right side, as drawServerRack draws its front.
void drawServerRackSide(int x, int y, bool lit);
