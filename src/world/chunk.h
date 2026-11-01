#pragma once
#include "core/vec.h"
#include "world/floor.h"
#include "world/light.h"
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
/// toilet stands on floor against a wall, and blocks walking, not sight;
/// so does each of a vending machine's two cells. In a server room a rack
/// blocks walking and sight, and a terminal or the console desk walking only;
/// so does a water dispenser, drunk from at its tap.
enum class Tile : uint8_t { Void, Floor, Wall, DoorLocked, DoorOpen, Water, Table, Statue, Basin, Toilet, Vending, Rack,
                            Console, Dispenser };

inline bool passable(Tile t) { return t == Tile::Floor || t == Tile::DoorOpen; }
/// Open water, to drink from or wash in, which blood and filth get into:
/// pools, canals and basins.
inline bool pooled(Tile t) { return t == Tile::Water || t == Tile::Basin; }
/// Water to drink from: open water, and a water dispenser's tap.
inline bool drinkable(Tile t) { return pooled(t) || t == Tile::Dispenser; }
/// Blocks sight. Void is anywhere not generated yet.
inline bool opaque(Tile t) {
    return t == Tile::Void || t == Tile::Wall || t == Tile::DoorLocked || t == Tile::Statue || t == Tile::Rack;
}

/// WaterMaze is a maze walled with water instead of stone. Plaza is one
/// grand hall round a fountain. DataCentre is a grid of corridors with
/// server rooms between them.
enum class ChunkKind { Spawn, Maze, WaterMaze, Rooms, Plaza, DataCentre };

/// The slots a chunk owns, as offsets from the top-left of its box, which
/// is offset {0, 0}'s row and column. A box owns every slot in it; an L, T,
/// S or Z leaves some out, and those belong to other chunks.
using Footprint = std::vector<Vec2i>;
/// Every slot of a box this many slots across and down, row by row.
Footprint boxFootprint(Vec2i slots);
/// The size in slots of the box round a footprint.
Vec2i footprintBox(const Footprint& parts);

/// A piece of the world covering one or more slots, walled all round.
/// Once made it stays, and keeps being simulated off screen. Its cells
/// span its whole box; those in slots it doesn't own stay Void, and are
/// never read but by its own generators, which leave them be.
struct Chunk {
    int id;
    ChunkKind kind;
    /// Top-left slot of the box, and its size in slots.
    Vec2i slot, slots;
    /// The slots it owns.
    Footprint parts;
    /// Size of the box in cells.
    int w, h;
    std::vector<Tile> tiles;
    /// How each cell's floor looks; meaningless where there is no floor.
    std::vector<Floor> floors;
    /// The floor generators lay on cells they carve.
    Floor paint;
    /// How each cell's wall looks; meaningless where there is no wall.
    std::vector<WallStyle> walls;
    /// How brightly each cell is lit, 0 to LIGHT_FULL, and by what kind of
    /// source the brightest light on it comes from.
    std::vector<uint8_t> light;
    std::vector<LightKind> lightFrom;

    Chunk(int id, ChunkKind kind, Vec2i slot, Footprint parts);

    Vec2i origin() const { return slot * CHUNK; }
    int slotCount() const { return int(parts.size()); }
    /// Owns the slot this far from the top-left of its box.
    bool ownsSlot(Vec2i off) const {
        return off.x >= 0 && off.y >= 0 && off.x < slots.x && off.y < slots.y &&
               owned_[size_t(off.y * slots.x + off.x)];
    }
    bool covers(Vec2i s) const { return ownsSlot(s - slot); }
    bool inside(Vec2i l) const { return l.x >= 0 && l.y >= 0 && l.x < w && l.y < h; }
    /// In a slot it owns.
    bool owns(Vec2i l) const { return inside(l) && ownsSlot({l.x / CHUNK, l.y / CHUNK}); }
    /// Inside the border walls: it and every cell round it are owned. So
    /// the border runs round the footprint's outline, the inside corner of
    /// an L included, and seams between owned slots are open.
    bool interior(Vec2i l) const {
        if (box_) return l.x > 0 && l.y > 0 && l.x < w - 1 && l.y < h - 1;
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++)
                if (!owns(l + Vec2i{x, y})) return false;
        return true;
    }
    /// Every cell of the rectangle is interior.
    bool interiorRect(int x, int y, int rw, int rh) const;
    Tile get(Vec2i l) const { return tiles[size_t(l.y * w + l.x)]; }
    void set(Vec2i l, Tile t) { tiles[size_t(l.y * w + l.x)] = t; }
    Floor floor(Vec2i l) const { return floors[size_t(l.y * w + l.x)]; }
    void setFloor(Vec2i l, Floor f) { floors[size_t(l.y * w + l.x)] = f; }
    WallStyle wall(Vec2i l) const { return walls[size_t(l.y * w + l.x)]; }
    void setWall(Vec2i l, WallStyle s) { walls[size_t(l.y * w + l.x)] = s; }
    int lightAt(Vec2i l) const { return light[size_t(l.y * w + l.x)]; }

    /// Runs whatever lives here, on screen or not. Nothing does yet.
    void simulate(float dt) { (void)dt; }

private:
    /// Which slots of the box it owns, row by row.
    std::vector<bool> owned_;
    /// It owns its whole box, so interior() needs only the box.
    bool box_;
};
