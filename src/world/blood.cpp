#include "world/blood.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

namespace {

/// Seconds for fresh blood to dry.
constexpr float DRY_TIME = 90.0f;
/// Thickness a drop lays at its middle, thinning to its edge.
constexpr float DROP_INK = 150.0f;
constexpr float EDGE_INK = 50.0f;
/// Pixels of the outer ring left bare at this chance, so drops aren't discs.
constexpr float RAGGED = 0.35f;
/// The rows of a wall cell its south face is drawn on, as in world_view.cpp.
constexpr int WALL_FACE_PX = 2;
/// Water clouds this much for a pixel of the thickest blood, and clears in
/// this many seconds from fully red.
constexpr float TINT_PER_PIXEL = 4.0f / (CELL_PX * CELL_PX);
constexpr float TINT_CLEAR_TIME = 60.0f;
/// Each bit of red fouls the water this much, as feces in it do.
constexpr float BLOOD_FOUL = 1.0f;

bool takesBlood(Tile t) { return t == Tile::Floor || t == Tile::DoorOpen || t == Tile::Table || t == Tile::Statue || t == Tile::Toilet; }

} // namespace

float Stain::total() const {
    int sum = 0;
    for (uint8_t a : amount) sum += a;
    return sum / float(amount.size() * 255);
}

float Stain::share(int id) const {
    int mine = 0, all = 0;
    for (const Bleeder& b : whose) {
        all += b.ink;
        if (b.id == id) mine += b.ink;
    }
    return all ? mine / float(all) : 0.0f;
}

int Stain::mostly() const {
    const Bleeder* best = &whose[0];
    for (const Bleeder& b : whose)
        if (b.ink > best->ink) best = &b;
    return best->id;
}

// Each bleeder's ink adds up in its slot; a newcomer takes the slot of
// whoever laid least.
void Blood::drop(const World& world, float x, float y, float radius, int who, Rng& rng, float thin) {
    float reach = radius + 0.5f;
    int px0 = int(std::floor(x - reach)), px1 = int(std::floor(x + reach));
    int py0 = int(std::floor(y - reach)), py1 = int(std::floor(y + reach));
    for (int py = py0; py <= py1; py++)
        for (int px = px0; px <= px1; px++) {
            float d = std::hypot(px + 0.5f - x, py + 0.5f - y);
            if (d > reach) continue;
            float edge = d / reach;
            if (edge > 0.6f && rng.chance(RAGGED)) continue;
            Vec2i cell{floorDiv(px, CELL_PX), floorDiv(py, CELL_PX)};
            int lx = px - cell.x * CELL_PX, ly = py - cell.y * CELL_PX;
            int ink = int((DROP_INK + (EDGE_INK - DROP_INK) * edge) * thin);
            Tile t = world.tile(cell);
            if (drinkable(t)) {
                tintWater(cell, ink / 255.0f * TINT_PER_PIXEL);
                continue;
            }
            // Only a wall's face shows, where nothing stands before it.
            if (t == Tile::Wall && !opaque(world.tile(cell + Vec2i{0, 1})))
                ly = CELL_PX - WALL_FACE_PX + ly * WALL_FACE_PX / CELL_PX;
            else if (!takesBlood(t))
                continue;
            Stain& s = stains_[cell];
            size_t i = size_t(ly * CELL_PX + lx);
            s.amount[i] = uint8_t(std::min(255, s.amount[i] + ink));
            Stain::Bleeder* slot = &s.whose[0];
            for (Stain::Bleeder& b : s.whose) {
                if (b.id == who) {
                    slot = &b;
                    break;
                }
                if (b.ink < slot->ink) slot = &b;
            }
            if (slot->id != who) *slot = {who, 0};
            slot->ink += ink;
            s.wet = 1.0f;
            drying_.insert(cell);
        }
}

void Blood::tintWater(Vec2i cell, float amount) {
    float& t = tint_[cell];
    t = std::min(1.0f, t + amount);
    foul_[cell] += amount * BLOOD_FOUL;
}

void Blood::update(float dt) {
    std::erase_if(tint_, [&](auto& kv) { return (kv.second -= dt / TINT_CLEAR_TIME) <= 0.0f; });
    std::erase_if(drying_, [&](Vec2i c) {
        Stain& s = stains_[c];
        s.wet = std::max(0.0f, s.wet - dt / DRY_TIME);
        return s.wet <= 0.0f;
    });
}
