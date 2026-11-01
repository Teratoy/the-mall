#pragma once
#include "core/color.h"
#include <cstdint>

/// A thick black cable lies flat on the floor of a cell. Which edges of
/// the cell it leaves by says what piece it is: two opposite a straight run, two side by side a
/// corner, three a T where a branch joins, all four a cross.
enum CableLink : uint8_t { CABLE_N = 1, CABLE_E = 2, CABLE_S = 4, CABLE_W = 8 };

/// Straight runs come in this many looks, which all meet seamlessly, so a
/// long run can mix them: plain, sagging a little, and with a tie round it.
inline constexpr int CABLE_VARIANTS = 3;

/// The cable's pixel at gx, gy of its cell, for the edges it leaves by;
/// variant picks a straight run's look and is left alone by the other
/// pieces. Clear where the floor shows, and wholly clear for fewer than two edges.
Rgba cablePixel(uint8_t links, int variant, int gx, int gy);

/// Draws the cable lying in the cell with its top-left at canvas pixel x, y,
/// dimmed when remembered rather than seen.
void drawCable(int x, int y, uint8_t links, int variant, bool inSight);
