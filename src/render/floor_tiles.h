#pragma once
#include "core/vec.h"
#include "world/floor.h"
#include "world/wall.h"
#include <raylib.h>

/// Floor patterns as one texture: each pattern a seamless square of
/// TILE_PX, laid over the world so neighbouring cells line up. A second row
/// holds the frames of the glints on water, and a third the wall patterns.
class FloorTiles {
public:
    /// Four cells across, so the slower patterns get room to repeat.
    static constexpr int TILE_PX = 24;
    static constexpr int WATER_FRAMES = 20;

    FloorTiles();
    ~FloorTiles();
    FloorTiles(const FloorTiles&) = delete;
    FloorTiles& operator=(const FloorTiles&) = delete;

    /// Draws the pattern's part for a cell at canvas pixel x, y, in ink.
    void draw(int x, int y, Vec2i cell, FloorPattern pattern, Color ink) const;
    /// Draws one frame of the glints for a water cell, the same way.
    void drawWater(int x, int y, Vec2i cell, int frame, Color ink) const;
    /// Draws a wall's pattern on the top h pixels of a cell, so it stays
    /// off the wall's face.
    void drawWall(int x, int y, Vec2i cell, WallPattern pattern, int h, Color ink) const;

private:
    Texture2D atlas_;
};
