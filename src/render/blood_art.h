#pragma once
#include <raylib.h>

struct Stain;

/// Blood's colour, from dry (0) to fresh (1).
Color bloodColor(float wet);
/// How opaque blood this thick (0 to 255) is: thin smears show through.
unsigned char bloodAlpha(int amount);

/// Draws a cell's blood with its top-left at (x, y), red while wet and
/// darker as it dries, thin smears lighter than pools. brightness scales
/// it, for remembered cells.
void drawStain(int x, int y, const Stain& stain, float brightness);
/// Clouds a water cell with its top-left at (x, y) red, tint from 0 to 1.
void drawWaterTint(int x, int y, float tint);
