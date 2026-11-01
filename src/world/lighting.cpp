// Light in the world: where it comes from and how far it reaches. Each
// chunk keeps the light on its cells, worked out again when a chunk is
// made beside it or a door near it opens or shuts. Nothing in the sim
// reads it yet (AI/light_plan.md step 1).
#include "world/shadowcast.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace {

/// Ceiling fittings hang on a grid this many cells apart.
constexpr int FITTING_SPACING = 8;
/// A grid point over no floor takes the floor this many cells off, at most.
constexpr int FITTING_SNAP = 2;
/// No two fittings hang closer than this, in steps.
constexpr int FITTING_APART = 4;
/// How many fittings are dead, and how many of the rest flicker.
constexpr float FITTING_DEAD_CHANCE = 0.15f;
constexpr float FITTING_FLICKER_CHANCE = 0.08f;
/// How many chunks have every fitting dead.
constexpr float DARK_CHUNK_CHANCE = 0.12f;
/// Night lights in each slot of a chunk, each tried this many times.
constexpr int NIGHT_LIGHTS_PER_SLOT = 2;
constexpr int NIGHT_LIGHT_TRIES = 40;

} // namespace

int World::light(Vec2i cell) const {
    const Chunk* c = chunkAt(slotOf(cell));
    return c ? c->lightAt(cell - c->origin()) : 0;
}

LightKind World::lightFrom(Vec2i cell) const {
    const Chunk* c = chunkAt(slotOf(cell));
    if (!c) return LightKind::None;
    Vec2i l = cell - c->origin();
    return c->lightFrom[size_t(l.y * c->w + l.x)];
}

float World::mallTime() const { return std::fmod(time_ + OPENING * MALL_HOUR, MALL_DAY); }

int World::hour() const { return int(mallTime() / MALL_HOUR); }

int World::minute() const { return int(std::fmod(mallTime(), MALL_HOUR) / MALL_HOUR * 60.0f); }

int World::day() const { return 1 + int((time_ + OPENING * MALL_HOUR) / MALL_DAY); }

// The world's own clock: no dice, the same in every world. The lights follow
// whether it is closed, so a jump in time (a test setting the clock) leaves
// them right too, though no bell rings for it.
void World::tickClock(float before) {
    const int was = int(std::fmod(before + OPENING * MALL_HOUR, MALL_DAY) / MALL_HOUR), now = hour();
    if (now != was) {
        bellsRung_++;
        lastBell_ = now == OPENING ? Bell::Opening : now == 0 ? Bell::Closing : Bell::Hour;
    }
    if (closed() == closed_) return;
    closed_ = closed();
    for (auto& c : chunks_) relight(*c);
}

void World::addLight(Vec2i cell, LightKind kind, bool on, bool flicker) {
    slotLights_[slotOf(cell)].push_back(int(lights_.size()));
    lights_.push_back({cell, kind, on, flicker});
}

// Fittings hang on a grid across the chunk, each over the floor nearest its
// point. Some are dead and some flicker; now and then a whole chunk is dark.
// Never in the spawn room, where the first human wakes: it wakes in light. Server rooms are lit only by
// their machines. The dice are the chunk's own, so lighting a chunk changes
// nothing else the world rolls.
void World::hangFittings(const Chunk& c) {
    Rng dice(seed_ ^ uint64_t(c.id + 1) * 0x94D049BB133111EBull);
    const bool dark = c.id > 0 && dice.chance(DARK_CHUNK_CHANCE);
    auto free = [&](Vec2i cell) {
        Vec2i l = cell - c.origin();
        return c.interior(l) && c.get(l) == Tile::Floor && !serverRoomAt(cell);
    };
    const Vec2i offset{dice.range(0, FITTING_SPACING - 1), dice.range(0, FITTING_SPACING - 1)};
    std::vector<Vec2i> hung;
    for (int y = offset.y; y < c.h; y += FITTING_SPACING)
        for (int x = offset.x; x < c.w; x += FITTING_SPACING) {
            Vec2i point = c.origin() + Vec2i{x, y};
            std::optional<Vec2i> at;
            for (int r = 0; r <= FITTING_SNAP && !at; r++)
                for (int dy = -r; dy <= r && !at; dy++)
                    for (int dx = -r; dx <= r && !at; dx++)
                        if (std::max(std::abs(dx), std::abs(dy)) == r && free(point + Vec2i{dx, dy}))
                            at = point + Vec2i{dx, dy};
            if (!at || std::any_of(hung.begin(), hung.end(), [&](Vec2i h) { return cheb(h, *at) < FITTING_APART; }))
                continue;
            hung.push_back(*at);
            bool dead = dark || (c.kind != ChunkKind::Spawn && dice.chance(FITTING_DEAD_CHANCE));
            addLight(*at, LightKind::Fitting, !dead, !dead && dice.chance(FITTING_FLICKER_CHANCE));
        }
    for (int i = 0; i < NIGHT_LIGHTS_PER_SLOT * c.slotCount(); i++)
        for (int tries = 0; tries < NIGHT_LIGHT_TRIES; tries++) {
            Vec2i cell = c.origin() + Vec2i{dice.range(0, c.w - 1), dice.range(0, c.h - 1)};
            if (!free(cell)) continue;
            addLight(cell, LightKind::NightLight);
            break;
        }
}

// Vending machines light the floor before them, screens and racks glow a
// little round them, a dispenser's tap barely, and the giant desk lamp and
// light bulb shine from their bases (user).
void World::lightUp(const Chunk& c, Glowing before) {
    lightPlaced(before);
    hangFittings(c);
    relightAround(c.origin(), c.origin() + Vec2i{c.w - 1, c.h - 1});
}

void World::lightPlaced(Glowing before) {
    const size_t first = lights_.size();
    for (size_t i = before.machines; i < machines_.size(); i++) addLight(machines_[i].at, LightKind::Vending);
    for (size_t i = before.pieces; i < pieces_.size(); i++) {
        const ServerPiece& p = pieces_[i];
        if (p.kind == Rig::Rack) addLight(p.at, LightKind::Rack);
        else if (p.kind == Rig::Terminal) addLight(p.at, LightKind::Screen);
        else addLight(p.at + Vec2i{DESK_W / 2, 1}, LightKind::Screen);
    }
    for (size_t i = before.dispensers; i < dispensers_.size(); i++) addLight(dispensers_[i].at, LightKind::Dispenser);
    for (size_t i = before.statues; i < statues_.size(); i++) {
        const Statue& s = statues_[i];
        if (s.kind == StatueKind::Lamp || s.kind == StatueKind::Bulb)
            addLight(s.at + Vec2i{s.size.x / 2, s.size.y - 1}, LightKind::Statue);
    }
    for (size_t i = first; i < lights_.size(); i++) relightAround(lights_[i].cell, lights_[i].cell);
}

// Each source shines into the chunk only on cells the chunk owns; where
// two reach a cell, the brighter wins, and on a tie the one put up first.
void World::relight(Chunk& c) {
    std::fill(c.light.begin(), c.light.end(), uint8_t(0));
    std::fill(c.lightFrom.begin(), c.lightFrom.end(), LightKind::None);
    const Vec2i lo = c.origin(), hi = c.origin() + Vec2i{c.w - 1, c.h - 1};
    const Vec2i from = slotOf(lo - Vec2i{LIGHT_REACH, LIGHT_REACH}), to = slotOf(hi + Vec2i{LIGHT_REACH, LIGHT_REACH});
    auto tileAt = [&](Vec2i cell) { return tile(cell); };
    for (int sy = from.y; sy <= to.y; sy++)
        for (int sx = from.x; sx <= to.x; sx++) {
            auto it = slotLights_.find({sx, sy});
            if (it == slotLights_.end()) continue;
            for (int id : it->second) {
                const LightSource& s = lights_[size_t(id)];
                const int r = lightRadius(s.kind);
                if (!s.on || (closed_ && s.kind == LightKind::Fitting) || s.cell.x + r < lo.x || s.cell.y + r < lo.y || s.cell.x - r > hi.x || s.cell.y - r > hi.y)
                    continue;
                shadowcast(s.cell, r, tileAt, [&](Vec2i cell) {
                    Vec2i l = cell - lo;
                    if (!c.owns(l)) return;
                    Vec2i d = cell - s.cell;
                    int level = lightLevel(s.kind, std::sqrt(float(d.x * d.x + d.y * d.y)));
                    size_t i = size_t(l.y * c.w + l.x);
                    if (level <= c.light[i]) return;
                    c.light[i] = uint8_t(level);
                    c.lightFrom[i] = s.kind;
                });
            }
        }
}

// Each chunk's light is worked out from scratch, so the order they are
// relit in doesn't matter.
void World::relightAround(Vec2i lo, Vec2i hi) {
    const Vec2i from = slotOf(lo - Vec2i{LIGHT_REACH, LIGHT_REACH}), to = slotOf(hi + Vec2i{LIGHT_REACH, LIGHT_REACH});
    std::vector<Chunk*> near;
    for (int sy = from.y; sy <= to.y; sy++)
        for (int sx = from.x; sx <= to.x; sx++)
            if (Chunk* c = chunkAt({sx, sy}); c && std::find(near.begin(), near.end(), c) == near.end())
                near.push_back(c);
    for (Chunk* c : near) relight(*c);
}
