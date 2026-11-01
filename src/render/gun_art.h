#pragma once
#include "core/color.h"
#include "human/human_look.h"
#include <raylib.h>

/// Rounds an open ammo box shows: as many of them as it has left.
inline constexpr int AMMO_SLOTS = 8;
/// Spent casings lying on one cell each have their own spot, this many at
/// most; more look the same.
inline constexpr int CASINGS_SHOWN = 8;
/// Seconds from the shot a spent casing flies, bounces and lies before the
/// casings on the floor take over.
inline constexpr float CASING_TIME = 0.6f;

/// A pistol filling one cell, its barrel pointing right.
inline constexpr int GUN_W = 6;
inline constexpr int GUN_H = 6;

/// The gun's pixel at gx, gy, from its sprite in data/sprites.txt; clear
/// where nothing is drawn.
Rgba gunPixel(int gx, int gy);

/// Draws a gun lying on the floor with the cell's top-left at canvas pixel
/// x, y, dimmed unless lit.
void drawGun(int x, int y, bool lit);

/// Draws an ammo box with the cell's top-left at canvas pixel x, y, dimmed
/// unless lit: shut until opened, then open showing what is left of what
/// it held (0 to 1).
void drawAmmoBox(int x, int y, bool opened, float left, bool lit);

/// Draws count spent casings lying on a cell with its top-left at canvas
/// pixel x, y, each in its own spot, dimmed unless lit.
void drawCasings(int x, int y, int count, bool lit);

/// Draws a spent casing thrown out of the gun t seconds after the shot,
/// out to the gun's right; port is the screen point it leaves the gun at,
/// drop the sprite pixels from there down to the floor under it, side the
/// way it flies over the floor (a unit step, screen y down), scale screen
/// pixels to a sprite pixel. It tumbles out, bounces once and lies.
void drawEjectedCasing(Vector2 port, Vector2 side, float drop, float t, int scale, Color tint);
/// The screen point the casing comes to rest at, for the floor to take it.
Vector2 ejectedCasingRest(Vector2 port, Vector2 side, float drop, int scale);

/// Draws the gun a human shoots with in its hand at sprite pixel at.x,
/// at.y of a frame whose pixel (0, 0) is at origin, scale screen pixels to
/// a sprite pixel, mirrored when flip; aimed at.aim, the muzzle tipped up
/// by the kick when at.fire.
void drawHeldGun(Vector2 origin, GunSpot at, bool flip, int scale, Color tint);
