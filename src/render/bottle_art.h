#pragma once
#include "human/bottle.h"
#include "human/human_look.h"
#include <functional>
#include <raylib.h>

/// A water bottle as a sprite: BOTTLE_ACROSS pixels wide, BOTTLE_LONG from
/// base to cap, its water filling it from the bottom (whichever way is down
/// as it is turned) in BOTTLE_STEPS steps, any water at all showing as one.
/// The water is the colour of the world's water, clouded as red and foul
/// as it is; the empty part is clear glass.
inline constexpr int BOTTLE_ACROSS = 2;
inline constexpr int BOTTLE_LONG = 5;
inline constexpr int BOTTLE_STEPS = BOTTLE_LONG - 1;

/// Every pixel of the bottle with its base on (x, y), pointing turn, and
/// its colour; mirrored about x when flip (facing left).
void forBottlePixels(int x, int y, BottleTurn turn, bool flip, const Bottle& b,
                     const std::function<void(int, int, Color)>& put);

/// Draws it so on screen, a pixel being scale screen pixels, origin the
/// screen point of pixel (0, 0); the colours multiplied by tint.
void drawBottle(Vector2 origin, int x, int y, BottleTurn turn, bool flip, const Bottle& b, int scale, Color tint);

/// Water pouring out of an upside-down bottle with its base on (x, y),
/// down to the floor at row floorY, drawn so; time moves the drops.
void drawPourStream(Vector2 origin, int x, int y, int floorY, bool flip, const Bottle& b, int scale, Color tint,
                    float time);

/// The bottle as an icon, upright, its top-left at (x, y).
void drawBottleIcon(int x, int y, const Bottle& b);
