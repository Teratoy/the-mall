#pragma once
#include "world/trap.h"

/// A cake put down is drawn smaller, HEAP_CAKE_W across and HEAP_CAKE_H high,
/// so a few heap up on one cell.
inline constexpr int HEAP_CAKE_W = 4, HEAP_CAKE_H = 3;

/// A key lying on a cell with its top-left at canvas pixel x, y, dimmed unless in sight.
void drawKey(int x, int y, bool inSight);
/// A cake on a table or the floor, filling its cell.
void drawCake(int x, int y, bool inSight);
/// A cake put down on a heap, its top-left at x, y.
void drawHeapCake(int x, int y, bool inSight);
/// A trap on the floor; sprung shows spikes up or the plate pressed down.
void drawTrap(int x, int y, TrapKind kind, bool sprung, bool inSight);
