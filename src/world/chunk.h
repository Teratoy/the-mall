#pragma once
#include "core/vec.h"
#include "world/floor.h"
#include "world/wall.h"
#include <cstdint>
#include <vector>

/// Cells per side of a slot. Chunks are one slot or several.
inline constexpr int CHUNK = 100;
/// Screen pixels per cell side.
inline constexpr int CELL_PX = 6;

/// Water blocks walking but not sight; humans drink and wash at it. So do
/// tables, which stand on floor and hold what lies on them. A statue's base
/// stands on floor too, and blocks both. A basin is a little square of
/// water in a stone rim, standing on floor, drunk from like any water. A
/// toilet stands on floor against a wall, and blocks walking, not sight.
enum class Tile : uint8_t { Void, Floor, Wall, DoorLocked, DoorOpen, Water, Table, Statue, Basin, Toilet };

inline bool passable(Tile t) { return t == Tile::Floor || t == Tile::DoorOpen; }
/// Water to drink from or wash in: pools, canals and basins.
inline bool drinkable(Tile t) { return t == Tile::Water || t == Tile::Basin; }
/// Blocks sight. Void is anywhere not generated yet.
inline bool opaque(Tile t) {
    return t == Tile::Void || t == Tile::Wall || t == Tile::DoorLocked || t == Tile::Statue;
}

/// WaterMaze is a maze walled with water instead of stone. Plaza is one
/// grand hall round a fountain.
enum class ChunkKind { Spawn, Maze, WaterMaze, Rooms, Plaza };

/// A piece of the world covering one or more slots, walled all round.
/// Once made it stays, and keeps being simulated off screen.
struct Chunk {
    int id;
    ChunkKind kind;
    /// Top-left slot, and size in slots.
    Vec2i slot, slots;
    /// Size in cells.
    int w, h;
    std::vector<Tile> tiles;
    /// How each cell's floor looks; meaningless where there is no floor.
    std::vector<Floor> floors;
    /// The floor generators lay on cells they carve.
    Floor paint;
    /// How each cell's wall looks; meaningless where there is no wall.
    std::vector<WallStyle> walls;

    Chunk(int id, ChunkKind kind, Vec2i slot, Vec2i slots)
        : id(id), kind(kind), slot(slot), slots(slots),
          w(slots.x * CHUNK), h(slots.y * CHUNK), tiles(size_t(w * h), Tile::Wall),
          floors(size_t(w * h)), walls(size_t(w * h)) {}

    Vec2i origin() const { return slot * CHUNK; }
    bool covers(Vec2i s) const {
        return s.x >= slot.x && s.y >= slot.y && s.x < slot.x + slots.x && s.y < slot.y + slots.y;
    }
    bool inside(Vec2i l) const { return l.x >= 0 && l.y >= 0 && l.x < w && l.y < h; }
    /// Inside the border walls.
    bool interior(Vec2i l) const { return l.x > 0 && l.y > 0 && l.x < w - 1 && l.y < h - 1; }
    Tile get(Vec2i l) const { return tiles[size_t(l.y * w + l.x)]; }
    void set(Vec2i l, Tile t) { tiles[size_t(l.y * w + l.x)] = t; }
    Floor floor(Vec2i l) const { return floors[size_t(l.y * w + l.x)]; }
    void setFloor(Vec2i l, Floor f) { floors[size_t(l.y * w + l.x)] = f; }
    WallStyle wall(Vec2i l) const { return walls[size_t(l.y * w + l.x)]; }
    void setWall(Vec2i l, WallStyle s) { walls[size_t(l.y * w + l.x)] = s; }

    /// Runs whatever lives here, on screen or not. Nothing does yet.
    void simulate(float dt) { (void)dt; }
};
