#pragma once

#include <functional>
#include <raylib.h>

struct Dropping;

/// Feces' colour, from dry (0) to fresh (1), as on a body; and piss's.
Color fecesColor(float fresh);
Color pissColor(float fresh);
/// Clouds a water cell with its top-left at (x, y) brown, foul from 0 to 1.
void drawWaterFoul(int x, int y, float foul);

/// Draws a puddle of piss or a pile of feces where it lies, shifted by
/// (-left, -top) world pixels: fresh is wet and bright, and it dries as
/// fresh falls to 0, feces darker and duller, piss fainter. age is seconds
/// since it was left, as a puddle spreads at first. brightness scales it,
/// for remembered ones. Only world pixels onFloor says yes to show it, so
/// a puddle by a wall stops at the wall.
void drawDropping(const Dropping& d, float fresh, float age, int left, int top, float brightness,
                  const std::function<bool(int, int)>& onFloor);
