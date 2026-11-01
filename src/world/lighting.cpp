// Light in the world: where it comes from and how far it reaches. Each
// chunk keeps the light on its cells, worked out again when a chunk is
// made beside it or a door near it opens or shuts. Nothing in the sim
// reads it yet (AI/light_plan.md step 1).
#include "world/shadowcast.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

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

void World::addLight(Vec2i cell, LightKind kind) {
    slotLights_[slotOf(cell)].push_back(int(lights_.size()));
    lights_.push_back({cell, kind});
}

// Vending machines light the floor before them, screens and racks glow a
// little round them, a dispenser's tap barely, and the giant desk lamp and
// light bulb shine from their bases (user).
void World::lightUp(const Chunk& c, Glowing before) {
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
    relightAround(c.origin(), c.origin() + Vec2i{c.w - 1, c.h - 1});
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
                if (!s.on || s.cell.x + r < lo.x || s.cell.y + r < lo.y || s.cell.x - r > hi.x || s.cell.y - r > hi.y)
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
