#include "world/chunk.h"
#include <algorithm>

Footprint boxFootprint(Vec2i slots) {
    Footprint parts;
    for (int y = 0; y < slots.y; y++)
        for (int x = 0; x < slots.x; x++) parts.push_back({x, y});
    return parts;
}

Vec2i footprintBox(const Footprint& parts) {
    Vec2i size{0, 0};
    for (Vec2i p : parts) size = {std::max(size.x, p.x + 1), std::max(size.y, p.y + 1)};
    return size;
}

// Cells of slots it doesn't own are Void, so carving, digging and the
// floods, which only turn wall into something else, never reach them.
Chunk::Chunk(int id, ChunkKind kind, Vec2i slot, Footprint parts)
    : id(id), kind(kind), slot(slot), slots(footprintBox(parts)), parts(std::move(parts)), w(slots.x * CHUNK),
      h(slots.y * CHUNK), tiles(size_t(w * h), Tile::Wall), floors(size_t(w * h)), walls(size_t(w * h)),
      light(size_t(w * h), 0), lightFrom(size_t(w * h), LightKind::None),
      owned_(size_t(slots.x * slots.y), false) {
    for (Vec2i p : this->parts) owned_[size_t(p.y * slots.x + p.x)] = true;
    box_ = slotCount() == slots.x * slots.y;
    if (box_) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (!owns({x, y})) set({x, y}, Tile::Void);
}

// Interior means owned with a ring of owned round it, so it's enough that
// every slot the rectangle grown by one cell touches is owned, and that
// the rectangle keeps off the box's own edge.
bool Chunk::interiorRect(int x, int y, int rw, int rh) const {
    if (x < 1 || y < 1 || x + rw > w - 1 || y + rh > h - 1) return false;
    if (box_) return true;
    for (int sy = (y - 1) / CHUNK; sy <= (y + rh) / CHUNK; sy++)
        for (int sx = (x - 1) / CHUNK; sx <= (x + rw) / CHUNK; sx++)
            if (!ownsSlot({sx, sy})) return false;
    return true;
}
