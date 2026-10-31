#pragma once
#include "core/vec.h"
#include <cstdint>

/// Draws a toilet filling the cell with its top-left at (x, y), its tank
/// against the wall that way (back), dirtier the higher dirt, its water
/// yellower the more piss, the stains placed by seed; dimmed when remembered
/// rather than seen.
void drawToilet(int x, int y, Vec2i back, float dirt, float piss, uint32_t seed, bool lit);
