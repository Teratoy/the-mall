#pragma once
#include "core/vec.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

/// Piss and feces, as each was left: where, how much, when and by whom; the
/// prints of feet that stepped in fresh feces; and water poured out of a
/// bottle, which dries to nothing.
enum class MessKind : uint8_t { Piss, Feces, Print, Water };

struct Dropping {
    MessKind kind;
    /// The cell it lies on, and the world pixel it is centred on.
    Vec2i cell;
    float x, y;
    /// 1 a full one; less when it had less in it.
    float size;
    /// World seconds it was left at, and whose it is.
    float born;
    int who;
    /// Keeps its shape its own.
    uint32_t seed;
    /// Poured water: as foul and as red as it was in the bottle.
    float foul = 0.0f, red = 0.0f;
};

/// Everything humans have left behind. Nothing clears it: it dries where it
/// lies, slowly, feces darker and duller, piss fainter, and both grow less
/// disgusting as they dry, but stay for good, as blood does. Poured water
/// alone dries to nothing, and only disgusts as far as it was foul. Feces are more
/// disgusting than piss, wet or dry. What goes into water, washed off or
/// left straight in it, fouls the water instead, which clears in time.
class Mess {
public:
    void drop(MessKind kind, Vec2i cell, float x, float y, float size, float born, int who, uint32_t seed);
    /// Water poured out of a bottle, this foul and red.
    void pour(Vec2i cell, float x, float y, float size, float born, int who, uint32_t seed, float foul, float red);

    const std::vector<Dropping>& all() const { return all_; }
    /// What lies on a cell, as indexes into all(); nothing if none.
    const std::vector<int>* at(Vec2i cell) const {
        auto it = cells_.find(cell);
        return it == cells_.end() ? nullptr : &it->second;
    }
    /// 1 just left, down to 0 once dried right through.
    static float freshness(const Dropping& d, float now);
    /// How disgusting one is now, about 1 for a fresh full pile of feces.
    static float disgust(const Dropping& d, float now);
    /// All that lies on a cell together; but for whatever is but's own,
    /// as nobody minds its own (-1: all of it).
    float disgust(Vec2i cell, float now, int but = -1) const;
    /// The freshest feces on a cell, by freshness times size: 0 if none.
    float freshFeces(Vec2i cell, float now) const;

    /// How foul the water at a cell is, 0 clean, about 1 for a full pile in it.
    float foul(Vec2i cell) const {
        auto it = foul_.find(cell);
        return it == foul_.end() ? 0.0f : it->second;
    }
    /// Every water cell fouled, and how foul.
    const std::unordered_map<Vec2i, float, Vec2iHash>& fouled() const { return foul_; }
    /// Fouls a water cell this much more, there alone at first.
    void foulWater(Vec2i cell, float amount) { foul_[cell] += amount; }
    /// Foulness seeps slowly into the water next to it (water tells which
    /// cells are), and fades very slowly.
    void update(float dt, const std::function<bool(Vec2i)>& water);

private:
    std::vector<Dropping> all_;
    std::unordered_map<Vec2i, std::vector<int>, Vec2iHash> cells_;
    std::unordered_map<Vec2i, float, Vec2iHash> foul_;
    /// Seconds since foulness last seeped on.
    float seep_ = 0.0f;
};
