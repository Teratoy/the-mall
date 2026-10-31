#pragma once
#include "core/color.h"

/// A pistol filling one cell, its barrel pointing right.
inline constexpr int GUN_W = 6;
inline constexpr int GUN_H = 6;

/// The gun's pixel at gx, gy, from its sprite in data/sprites.txt; clear
/// where nothing is drawn.
Rgba gunPixel(int gx, int gy);

/// Draws a gun lying on the floor with the cell's top-left at canvas pixel
/// x, y, dimmed unless lit.
void drawGun(int x, int y, bool lit);
