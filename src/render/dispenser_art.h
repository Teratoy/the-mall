#pragma once
#include "core/color.h"

/// A water dispenser stands on one cell, a little shorter than a human: an
/// upturned jug on a stand with a hot and a cold tap. The jug is see-through,
/// so how full it is shows.
inline constexpr int DISPENSER_W = 6;
inline constexpr int DISPENSER_H = 17;

/// The dispenser's pixel at gx, gy, seen from the front or (side) from the
/// side, its taps toward the right, with the jug full to level (0 empty, 1
/// full); clear where nothing is drawn.
Rgba dispenserPixel(int gx, int gy, float level, bool side);

/// Draws the dispenser with the bottom-left corner of its foot at canvas
/// pixel x, y, full to level, dimmed unless in sight. Side on, flipped, its taps
/// face left.
void drawDispenser(int x, int y, float level, bool side, bool flip, bool inSight);
