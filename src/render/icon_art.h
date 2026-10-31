#pragma once
#include <raylib.h>

/// Pixels along each side of the spiral.
constexpr int ICON_PX = 15;

/// Whether the spiral's wall is at (x, y), counted from its top left, so
/// the phone's icon can be drawn from the same picture.
bool iconWall(int x, int y);

/// The game's icon, a square spiral winding in to the middle like the mall,
/// as a fresh image the caller unloads.
Image makeIcon();
