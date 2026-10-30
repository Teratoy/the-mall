#pragma once
#include "core/rng.h"
#include "world/blood.h"
#include "world/chunk.h"
#include "world/door.h"
#include "world/gen.h"
#include "world/mess.h"
#include "world/pile.h"
#include "world/statue.h"
#include "world/table.h"
#include "world/toilet.h"
#include "world/trap.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Human;
struct HumanLook;

/// The potentially infinite world: chunks on a grid of slots, made when
/// someone first crosses into a slot, and kept for good after. Each chunk
/// but the spawn brings one more human and a few cakes; humans roam across
/// chunks. Every key and cake it makes is set on a table of its own, but
/// for a key in a chunk with no room for one.
/// Here and there giant household objects stand as statues, in rooms roomy
/// enough for them, and maybe one in the spawn room.
/// A plaza without a fountain has a basin to drink from. Here and there, rarely, stands a toilet.
/// Hidden traps lie about every chunk but the spawn: spikes in the open,
/// darts rigged under some cakes, and in room networks slam rooms baited
/// with a cake, each with one more key to open it again.
class World {
public:
    explicit World(uint64_t seed);
    ~World();

    /// Makes the spawn room at slot (0, 0) with the given human at its centre.
    /// seed is the human's own: its name, traits and dice, rolled where
    /// they were shown before the world existed. A name given replaces the
    /// one grown from seed.
    Human& makeSpawn(const HumanLook& look, uint64_t seed, const std::string& name = {});

    static Vec2i slotOf(Vec2i cell) { return {floorDiv(cell.x, CHUNK), floorDiv(cell.y, CHUNK)}; }
    bool generated(Vec2i slot) const { return slotChunk_.count(slot) > 0; }
    /// The id of the chunk over a cell; -1 where nothing is generated.
    int chunkIdAt(Vec2i cell) const {
        auto it = slotChunk_.find(slotOf(cell));
        return it == slotChunk_.end() ? -1 : int(it->second);
    }
    /// What is at a cell; Void where nothing is generated.
    Tile tile(Vec2i cell) const;
    /// How the floor at a cell looks; Plain where nothing is generated.
    Floor floor(Vec2i cell) const;
    /// How the wall at a cell looks; Plain where nothing is generated.
    WallStyle wall(Vec2i cell) const;
    /// Generates a chunk over the cell's slot if there is none yet.
    void ensureGenerated(Vec2i cell);

    /// The table standing on a cell, if any. Tables never move.
    const Table* table(Vec2i cell) const;
    const std::vector<Table>& tables() const { return tables_; }

    /// The statue whose base is on a cell, if any. Statues never move.
    const Statue* statue(Vec2i cell) const;
    const std::vector<Statue>& statues() const { return statues_; }

    /// The toilet on a cell, if any; toilets never move.
    const Toilet* toilet(Vec2i cell) const {
        auto it = toilets_.find(cell);
        return it == toilets_.end() ? nullptr : &it->second;
    }
    const std::unordered_map<Vec2i, Toilet, Vec2iHash>& toilets() const { return toilets_; }
    /// Someone left this much in the toilet on the cell.
    void soilToilet(Vec2i cell, float amount) {
        if (auto it = toilets_.find(cell); it != toilets_.end()) it->second.dirt += amount;
    }

    bool hasKey(Vec2i cell) const { return keys_.count(cell) > 0; }
    bool takeKey(Vec2i cell) { return keys_.erase(cell) > 0; }
    const std::unordered_set<Vec2i, Vec2iHash>& keys() const { return keys_; }
    /// Unlocks the door the cell is part of, on both sides, or the slam
    /// room gate it is, for good. False if the cell is neither, locked.
    bool unlock(Vec2i cell);

    bool hasCake(Vec2i cell) const { return cakes_.count(cell) > 0; }
    bool takeCake(Vec2i cell) { return cakes_.erase(cell) > 0; }
    const std::unordered_set<Vec2i, Vec2iHash>& cakes() const { return cakes_; }

    /// What lies put down on a floor cell, if anything.
    const Pile* pile(Vec2i cell) const {
        auto it = piles_.find(cell);
        return it == piles_.end() ? nullptr : &it->second;
    }
    const std::unordered_map<Vec2i, Pile, Vec2iHash>& piles() const { return piles_; }
    /// Puts cakes and bottles down on a cell, onto whatever lies there.
    void putDown(Vec2i cell, int cakes, const std::vector<Bottle>& bottles);
    /// Takes up to n cakes off the pile on a cell; how many it got.
    int takePileCakes(Vec2i cell, int n);
    /// Takes the pile's i-th bottle off it.
    Bottle takePileBottle(Vec2i cell, size_t i);
    /// Pours up to amount from the pile's i-th bottle into into; how much.
    float pourFromPile(Vec2i cell, size_t i, Bottle& into, float amount);

    /// The trap set off from a cell, if any, hidden or not.
    const Trap* trap(Vec2i cell) const;
    const std::unordered_map<Vec2i, Trap, Vec2iHash>& traps() const { return traps_; }
    const std::vector<Dart>& darts() const { return darts_; }
    /// Someone just stepped onto the cell, or lifted what lay on the table
    /// there: sets off the trap there if it is armed, and everyone who sees
    /// it go off learns it is there. Darts are aimed at whoever set them off.
    void setOff(Vec2i cell, Human& h);

    /// Every stain of blood, wet or dry.
    Blood& blood() { return blood_; }
    const Blood& blood() const { return blood_; }
    /// Every bit of piss and feces left about.
    Mess& mess() { return mess_; }
    const Mess& mess() const { return mess_; }
    /// How foul the water at a cell is, from feces, piss and blood in it.
    float foul(Vec2i cell) const { return mess_.foul(cell); }

    /// Simulates every chunk and every human, seen or not.
    void update(float dt);
    /// Seconds simulated since the world began.
    float time() const { return time_; }
    size_t chunkCount() const { return chunks_.size(); }

    /// Every human ever made; a human's id is its index here.
    const std::vector<std::unique_ptr<Human>>& humans() const { return humans_; }
    Human& human(int id) { return *humans_[size_t(id)]; }
    const Human& human(int id) const { return *humans_[size_t(id)]; }
    /// How many humans are still alive.
    size_t livingCount() const;
    /// Who stands on a cell, if anyone. One human per cell.
    Human* occupant(Vec2i cell) const;
    void moveOccupant(Vec2i from, Vec2i to, Human* h);
    /// The dead stop taking up their cell; others walk over the body.
    void vacate(Vec2i cell) { occupied_.erase(cell); }

private:
    /// tests/stuck_soak.cpp packs extra humans in with addHuman.
    friend struct SoakProbe;
    struct Edge {
        EdgeKey key;
        /// The chunk's slot on this side of the wall, and the slot past it.
        Vec2i in, out;
    };

    Rng rng_;
    std::vector<std::unique_ptr<Chunk>> chunks_;
    std::unordered_map<Vec2i, int, Vec2iHash> slotChunk_;
    std::unordered_map<EdgeKey, Door, EdgeKeyHash> doors_;
    /// Which door each door cell belongs to, on both sides of its wall.
    std::unordered_map<Vec2i, EdgeKey, Vec2iHash> doorCells_;
    std::vector<Table> tables_;
    /// Which table stands on each cell.
    std::unordered_map<Vec2i, int, Vec2iHash> tableCells_;
    std::vector<Statue> statues_;
    /// Which statue's base is on each cell.
    std::unordered_map<Vec2i, int, Vec2iHash> statueCells_;
    /// Cells behind statues, under their bodies as drawn. Nothing is put
    /// there, where nobody could see it.
    std::unordered_set<Vec2i, Vec2iHash> behindStatues_;
    std::unordered_map<Vec2i, Toilet, Vec2iHash> toilets_;
    std::unordered_set<Vec2i, Vec2iHash> keys_;
    std::unordered_set<Vec2i, Vec2iHash> cakes_;
    std::unordered_map<Vec2i, Pile, Vec2iHash> piles_;
    std::unordered_map<Vec2i, Trap, Vec2iHash> traps_;
    std::vector<Dart> darts_;
    /// Slam room gates, which keys open as they do exit doors.
    std::unordered_set<Vec2i, Vec2iHash> gates_;
    std::vector<std::unique_ptr<Human>> humans_;
    std::unordered_map<Vec2i, Human*, Vec2iHash> occupied_;
    Blood blood_;
    Mess mess_;
    float time_ = 0.0f;

    const Chunk* chunkAt(Vec2i slot) const;
    Chunk* chunkAt(Vec2i slot);
    Chunk& addChunk(ChunkKind kind, Vec2i slot, const Footprint& parts);
    void generate(Vec2i slot, Vec2i entry);
    std::vector<Edge> perimeter(const Chunk& c) const;
    /// Puts the doors neighbours already made into the chunk's border, as
    /// tiles only, so generators can keep water off them.
    void markDoors(Chunk& c);
    /// Draws the doors neighbours already put in the chunk's walls, and adds
    /// new locked ones toward empty slots: at least minNew, at most maxNew,
    /// each beyond the minimum with the given chance. New doors only go
    /// where they can be walked to from the local cell from, not across
    /// water. Returns how many are new.
    int placeDoors(Chunk& c, Vec2i from, float chance, int minNew, int maxNew);
    void drawDoor(Chunk& c, const Door& d);
    /// Free floor deep into the chunk, as walked from the local cell from,
    /// in random order, as world cells. Free means no key, cake or human,
    /// and not behind a statue.

    std::vector<Vec2i> deepSpots(const Chunk& c, Vec2i from);
    /// Sets a table under the cell, of some shape that fits there with
    /// floor all round it, so it walls nothing off. False if none fits.
    /// Most of a chunk's tables are of its house material.
    bool placeTable(Chunk& c, Vec2i cell, TableMaterial house);
    /// Now and then a toilet: in a room network against a straight wall, in
    /// a maze at the end of a dead end. In gen_toilets.cpp.
    void placeToilets(Chunk& c);
    /// Now and then in a maze, a restroom: a tiled hall lined with toilets.
    void placeRestrooms(Chunk& c);
    /// A toilet plaza's toilets: along the hall's walls and round its
    /// columns, wherever one blocks nothing. In gen_toilets.cpp.
    void placePlazaToilets(Chunk& c);
    /// Sets a square basin with its top left corner at the cell, with floor
    /// all round it. False if it doesn't fit there.
    bool placeBasin(Chunk& c, Vec2i cell);
    /// The next of spots, from next on, that a table fits under, with the
    /// table set. Once they run out, with orFloor, any of spots still free
    /// floor, left without a table, so a key is never lost; else nothing.
    std::optional<Vec2i> tableSpot(Chunk& c, const std::vector<Vec2i>& spots, size_t& next, TableMaterial house,
                                   bool orFloor = false);
    /// Stands a statue of some kind and material with its base about the
    /// local cell centre, and floor all round it, so it walls nothing off.
    /// It keeps clear of doors and of the local cell clear. False if none fits.
    bool placeStatue(Chunk& c, Vec2i centre, Vec2i clear, bool grand = false,
                     std::optional<StatueKind> kind = {});
    /// A statue of any kind and material, maybe on a plinth, sized to suit.
    Statue rollStatue(bool grand, std::optional<StatueKind> kind = {});
    /// The plaza's grand statue in its middle, on its island if it has
    /// one, and a ring of smaller ones round it.
    void placePlazaStatues(Chunk& c, const gen::Plaza& plaza, Statue grand, bool toilets);

    /// Room for a statue's base of the given size at the local cell at:
    /// free floor under it and all round it, and well clear of doors and of
    /// the local cell clear.
    bool statueFits(const Chunk& c, Vec2i size, Vec2i at, Vec2i clear) const;
    /// Statues in the middle of some of the rooms, and in niches off others.
    void placeStatues(Chunk& c, const std::vector<gen::RoomBox>& rooms);
    /// Cuts a recess into solid wall off the north edge of the room, just
    /// deep enough for a small statue, and stands one in it facing the room.
    /// False if there is nowhere to cut one.
    bool placeNiche(Chunk& c, const gen::RoomBox& room);
    /// Stands a statue with its base's top-left at the local cell at, and
    /// notes the cells behind it.
    void standStatue(Chunk& c, Statue s, Vec2i at);

    /// Spikes about the open floor, away from doors, gates and other traps.

    void placeSpikes(Chunk& c, const std::vector<gen::SlamRoom>& slams);
    Human& addHuman(const HumanLook& look, Vec2i cell);
    Human& addHuman(const HumanLook& look, Vec2i cell, uint64_t seed, const std::string& name = {});
    void setTile(Vec2i cell, Tile t);
    /// Spikes sink, plates re-arm, gates shut, and darts fly.
    void updateTraps(float dt);
};
