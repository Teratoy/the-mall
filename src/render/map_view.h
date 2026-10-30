#pragma once
#include <raylib.h>

class World;
class Human;

/// The viewer's map: every cell they remember, flat colour by tile, cellPx
/// canvas pixels a cell, with centre (in cells) in the middle of area.
/// Over it, in small symbols: where they are, the keys and cakes they saw,
/// traps they know of, bodies, toilets, blood pools and mess; and a legend
/// for the symbols along the bottom. Nothing is drawn outside area.
void drawMap(const World& world, const Human& viewer, Rectangle area, int cellPx, Vector2 centre, float time);
/// How tall the legend is, so a caller can keep clear of it.
int mapLegendHeight();
