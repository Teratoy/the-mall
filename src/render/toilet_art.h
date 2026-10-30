#pragma once
#include "core/vec.h"
#include <cstdint>

/// Draws a toilet filling the cell with its top-left at (x, y), its tank
/// against the wall that way (back), dirtier the higher dirt, the stains
/// placed by seed; dimmed when remembered rather than seen.
void drawToilet(int x, int y, Vec2i back, float dirt, uint32_t seed, bool lit);
