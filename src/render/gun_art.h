#pragma once
#include "core/color.h"
#include "human/human_look.h"
#include <raylib.h>

/// A pistol filling one cell, its barrel pointing right.
inline constexpr int GUN_W = 6;
inline constexpr int GUN_H = 6;

/// The gun's pixel at gx, gy, from its sprite in data/sprites.txt; clear
/// where nothing is drawn.
Rgba gunPixel(int gx, int gy);

/// Draws a gun lying on the floor with the cell's top-left at canvas pixel
/// x, y, dimmed unless lit.
void drawGun(int x, int y, bool lit);

/// Draws the gun a human shoots with in its hand at sprite pixel at.x,
/// at.y of a frame whose pixel (0, 0) is at origin, scale screen pixels to
/// a sprite pixel, mirrored when flip; aimed at.aim, the muzzle flashing
/// when at.fire.
void drawHeldGun(Vector2 origin, GunSpot at, bool flip, int scale, Color tint);
