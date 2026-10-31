#pragma once
#include "core/rng.h"
#include "world/chunk.h"
#include <optional>
#include <utility>

/// Chunk interiors. Generators carve floor out of a chunk that starts all
/// wall, and never touch the border ring, which belongs to walls and doors,
/// nor the Void of slots it doesn't own. Only interior() cells are theirs.
/// Carving lays the chunk's current paint on new floor only, so a corridor
/// running into a room keeps the room's floor. Carving and digging never
/// touch water, so whatever water cuts apart stays apart. Nor statues,
/// though nothing should be dug once they stand.
namespace gen {

Floor randomFloor(Rng& rng);
WallStyle randomWall(Rng& rng);
/// Repaints the floor already in a rectangle.
void paintRect(Chunk& c, int x, int y, int w, int h);

/// Turns a wall into floor; anything else is left be.
void carve(Chunk& c, Vec2i p);
void carveRect(Chunk& c, int x, int y, int w, int h);
/// A width x width square around p.
void carveBrush(Chunk& c, Vec2i p, int width);
/// Straight along x then y, or y then x.
void carveCorridor(Chunk& c, Rng& rng, Vec2i a, Vec2i b, int width);

/// Digs from the cell just inside a door to the nearest floor.
void connectDoor(Chunk& c, Vec2i inner, int width);
/// Digs between floor areas until those on the same side of the water
/// are all one.
void connectRegions(Chunk& c, int width);
/// Which side of the water each cell is on: interior cells joined by
/// anything but water share a label. Water and the border are -1.
std::vector<int> sides(const Chunk& c);

/// The spawn: one square room this many cells a side in the middle of the
/// chunk. Its doors dig their own corridors to it.
void squareRoom(Chunk& c, int side);
void maze(Chunk& c, Rng& rng);
/// A room's box, and the floor cell in it that corridors aim at. Local cells.
struct RoomBox {
    int x, y, w, h;
    Vec2i centre;
};
/// Rooms of assorted shapes and sizes, joined by corridors, maybe split
/// by a canal and dotted with pools. Returns the rooms.
std::vector<RoomBox> rooms(Chunk& c, Rng& rng);

/// What a plaza leaves for statues, in local cells: the middle of its hall,
/// the island in its fountain if it has one, and how far out from the
/// middle a ring of smaller statues would stand.
struct Plaza {
    Vec2i centre;
    /// The island's top-left and size; no fountain when the size is zero.
    Vec2i island, islandSize;
    int ring = 0;
    /// A big plaza's avenues are lined with statues, one about each of
    /// these cells, in place of the ring.
    std::vector<Vec2i> lining;
    /// Top-left of each fountain court's island, all COURT_ISLAND in size.
    std::vector<Vec2i> courts;
    /// The floor cell in each kiosk where its table goes.
    std::vector<Vec2i> kiosks;
};
/// A fountain court's island: room for any statue at scale 1 on its plinth.
inline constexpr Vec2i COURT_ISLAND{14, 8};
/// One grand hall over the chunk, which must own its whole square box,
/// inlaid with a border and maybe lined
/// with columns, round a fountain whose pool circles an island of the
/// given size, if it is not zero. A big one (3 slots across) is crossed
/// by two avenues at the fountain, and each quarter between them is a
/// fountain court, a colonnade court or a cluster of kiosks.
Plaza plaza(Chunk& c, Rng& rng, Vec2i island);

/// Ponds on open floor, each a plain shape (round, oval, square, L, T or
/// C) turned some multiple of 90 degrees, with a ring of floor all round
/// so it cuts nothing off.
void pools(Chunk& c, Rng& rng, int count);
/// A straight channel of water from wall to wall with a floor quay along
/// each bank. Nothing crosses it. It keeps off doors already in the border.
void canal(Chunk& c, Rng& rng);
/// Every interior wall becomes water.
void flood(Chunk& c);

/// A closet off the floor, whose open gate slams shut behind whoever
/// takes the bait off the plate inside. Local cells.
struct SlamRoom {
    Vec2i gate, plate;
    /// The room's floor, inside its walls.
    int x, y, w, h;

    bool contains(Vec2i l) const;
};
/// Carves up to count slam rooms into solid wall, each joined to the floor
/// by a short corridor through its gate. Nothing must dig after, or it
/// may open a second way in.
std::vector<SlamRoom> slamRooms(Chunk& c, Rng& rng, int count);
/// A wall cell with a straight run of floor, water or tables from it to the plate,
/// and the way darts fly from it. Nothing if there is none.
std::optional<std::pair<Vec2i, Vec2i>> dartShooter(const Chunk& c, Rng& rng, Vec2i plate);
/// Room to walk round a cell: it and all eight around it are floor.
bool openAround(const Chunk& c, Vec2i p);

} // namespace gen
