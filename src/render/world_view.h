#pragma once
#include <functional>
#include <raylib.h>

class World;
class Human;
class HumanSprites;
class FloorTiles;
class StatueArt;

using SpritesFor = std::function<const HumanSprites&(const Human&)>;

/// A human's feet in world pixels, between the cells it steps from and to.
Vector2 feetOf(const Human& h);
/// The world pixel at the view's top-left corner, centring the viewer in a
/// view of size canvas pixels. zoom is canvas pixels per world pixel.
Vector2 cameraFor(const Human& viewer, float zoom, Vector2 size);
/// Whether the viewer's screen shows this human: in their sight, or reveal.
bool shown(const Human& viewer, const Human& h, bool reveal);
/// A key or a cake as it lies in the world, lit, CELL_PX square: for the HUD.
void drawKeyIcon(int x, int y);
void drawCakeIcon(int x, int y);

/// Draws the world as the viewer knows it, centred on them: cells in sight
/// lit, remembered ones dimmed, never-seen ones black, and the humans in
/// sight. reveal shows the whole truth instead, for debugging. zoom is
/// canvas pixels per world pixel; area is the part of the canvas the world
/// is shown in, and nothing is drawn outside it.
void drawWorld(const World& world, const Human& viewer, const SpritesFor& sprites,
               const FloorTiles& floorTiles, const StatueArt& statueArt, bool reveal, float zoom, Rectangle area);

