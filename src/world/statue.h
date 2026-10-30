#pragma once
#include "core/vec.h"
#include "world/chunk.h"
#include <cstdint>

/// Giant household objects stood about the world as statues. A statue's
/// base blocks walking and sight; the rest of it rises over the cells
/// behind, drawn side on like a human. Kind and material are purely looks.
enum class StatueKind : uint8_t {
    Mug,
    Duck,       // a rubber one
    Chair,
    Teapot,
    Toilet,
    Toaster,
    Lamp,       // an angled desk lamp
    Clock,      // a twin-bell alarm clock
    Spoon,      // stuck upright in the floor, handle down
    Bulb,       // a light bulb standing on its screw
    Count
};

/// Pop is glossy paint, each part its own colour.
enum class StatueMaterial : uint8_t { Marble, Bronze, Stone, Pop, Count };

struct Statue {
    StatueKind kind;
    StatueMaterial material;
    /// Top-left cell of the base, and its size in cells, plinth included.
    Vec2i at, size;
    /// Faces the other way.
    bool flip = false;
    /// Stands on a plinth a cell wider all round.
    bool plinth = false;
    /// Varies veins, patina and paint from one statue to the next.
    uint32_t seed = 0;
    /// Half as big again, for the middle of a plaza.
    bool grand = false;

    bool covers(Vec2i cell) const {
        Vec2i l = cell - at;
        return l.x >= 0 && l.y >= 0 && l.x < size.x && l.y < size.y;
    }
};

/// Size in cells of each kind's base, without a plinth. The statue is as
/// wide as its base.
inline constexpr Vec2i STATUE_BASES[] = {{7, 3}, {8, 4}, {7, 4}, {10, 4}, {9, 4},
                                         {9, 4}, {8, 3}, {8, 3}, {3, 2}, {6, 3}};
/// How tall each kind stands, in pixels, its base included.
inline constexpr int STATUE_HEIGHTS[] = {48, 48, 72, 48, 60, 42, 84, 54, 90, 60};

static_assert(sizeof STATUE_BASES / sizeof STATUE_BASES[0] == size_t(StatueKind::Count));
static_assert(sizeof STATUE_HEIGHTS / sizeof STATUE_HEIGHTS[0] == size_t(StatueKind::Count));
/// How far a plinth stands up off the floor.
inline constexpr int PLINTH_RISE_PX = 8;
/// How much bigger a grand statue is.
inline constexpr float GRAND_SCALE = 1.5f;

inline float statueScale(const Statue& s) { return s.grand ? GRAND_SCALE : 1.0f; }
/// The size in cells of a statue's base, plinth included, from its kind
/// and whether it is grand or on a plinth.
inline Vec2i statueSize(const Statue& s) {
    Vec2i b = STATUE_BASES[size_t(s.kind)];
    if (s.grand) b = {int(b.x * GRAND_SCALE + 0.99f), int(b.y * GRAND_SCALE + 0.99f)};
    return s.plinth ? b + Vec2i{2, 2} : b;
}
/// The statue's own height in pixels, its base included but not its plinth.
inline int statueBodyPx(const Statue& s) { return int(STATUE_HEIGHTS[size_t(s.kind)] * statueScale(s)); }
/// The statue's whole height as drawn, plinth included.
inline int statueHeightPx(const Statue& s) {
    return statueBodyPx(s) + (s.plinth ? PLINTH_RISE_PX + CELL_PX : 0);
}

/// Rows of cells behind the base that the statue's body is drawn over.
inline int statueOverhang(const Statue& s) {
    return (statueHeightPx(s) - s.size.y * CELL_PX + CELL_PX - 1) / CELL_PX;
}
