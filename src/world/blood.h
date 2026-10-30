#pragma once
#include "core/rng.h"
#include "core/vec.h"
#include "world/chunk.h"
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class World;

/// The blood on one cell, a pixel at a time, and how wet it still is. One
/// wetness for the whole cell: a fresh drop on an old stain wets it again.
struct Stain {
    /// How thick the blood is on each pixel, row by row; 0 is none.
    std::array<uint8_t, CELL_PX * CELL_PX> amount{};
    /// 1 fresh, 0 dry. Dry blood stains nothing.
    float wet = 0.0f;
    /// Whose blood it is, by how much each laid: the few who bled most here
    /// (id -1 is an empty slot).
    struct Bleeder {
        int id = -1;
        int ink = 0;
    };
    std::array<Bleeder, 3> whose{};

    /// All of it together, 1 being every pixel as thick as can be.
    float total() const;
    /// How much of it is id's own blood, 0 to 1.
    float share(int id) const;
    /// Who bled most of it, or -1.
    int mostly() const;
};

/// Every stain in the world. Blood stays where it falls for good, darkening
/// as it dries: on floor, open doors, tables, statue bases, and the faces
/// of walls, squashed into them. In water it only clouds it a while.
class Blood {
public:
    /// A round drop of who's blood radius world pixels wide, centred on world
    /// pixel (x, y), with a ragged edge; thin makes it fainter, as a footprint.
    void drop(const World& world, float x, float y, float radius, int who, Rng& rng, float thin = 1.0f);
    /// Blood dries over time.
    void update(float dt);

    const Stain* at(Vec2i cell) const {
        auto it = stains_.find(cell);
        return it == stains_.end() ? nullptr : &it->second;
    }
    bool wet(Vec2i cell) const {
        const Stain* s = at(cell);
        return s && s->wet > 0.0f;
    }
    const std::unordered_map<Vec2i, Stain, Vec2iHash>& stains() const { return stains_; }

    /// How red the water at a cell is, 0 clear to 1.
    float tint(Vec2i cell) const {
        auto it = tint_.find(cell);
        return it == tint_.end() ? 0.0f : it->second;
    }
    /// Clouds the water at a cell with this much more red, and fouls it.
    void tintWater(Vec2i cell, float amount);
    /// The foulness blood put into water since last asked, by cell, handed
    /// on to World::mess(), where it lingers long after the red clears.
    std::unordered_map<Vec2i, float, Vec2iHash> takeFoul() { return std::exchange(foul_, {}); }
    /// The same, without handing it on.
    const std::unordered_map<Vec2i, float, Vec2iHash>& fouled() const { return foul_; }

private:
    std::unordered_map<Vec2i, Stain, Vec2iHash> stains_;
    /// Cells still drying, so dry ones are never looked at again.
    std::unordered_set<Vec2i, Vec2iHash> drying_;
    std::unordered_map<Vec2i, float, Vec2iHash> tint_;
    std::unordered_map<Vec2i, float, Vec2iHash> foul_;
};
