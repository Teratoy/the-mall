#pragma once
#include "core/vec.h"
#include "world/table.h"
#include <functional>

/// How much of a table's cell the viewer gets to see.
enum class CellView { Unknown, Remembered, InSight };

/// The table's top is drawn raised this far above the floor, with its front
/// face below it: whatever lies on it is drawn this much higher too.
inline constexpr int TABLE_RISE_PX = 2;

/// Draws a table with its top-left cell's corner at canvas pixel x, y,
/// pixel by pixel as each cell under it is seen: left out where Unknown,
/// dimmed where Remembered.
void drawTable(const Table& t, int x, int y, const std::function<CellView(Vec2i)>& view);
