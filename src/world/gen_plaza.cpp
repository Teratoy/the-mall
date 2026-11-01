#include "world/gen.h"
#include "world/gen_tuning.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <tuple>

namespace gen {

namespace {

/// Solid wall left between the hall and the chunk's border.
tune::Int MARGIN_MIN{"plaza", "MARGIN_MIN"}, MARGIN_MAX{"plaza", "MARGIN_MAX"};
/// The inlaid band: this far in from the hall's edge, and this wide.
tune::Int BAND_IN_MIN{"plaza", "BAND_IN_MIN"}, BAND_IN_MAX{"plaza", "BAND_IN_MAX"};
tune::Int BAND_WIDTH_MIN{"plaza", "BAND_WIDTH_MIN"}, BAND_WIDTH_MAX{"plaza", "BAND_WIDTH_MAX"};
/// Columns 2 cells across, this far apart, this far in from the hall's edge.
tune::Float COLONNADE_CHANCE{"plaza", "COLONNADE_CHANCE"};
tune::Int COLUMN{"plaza", "COLUMN"};
tune::Int COLUMN_PITCH_MIN{"plaza", "COLUMN_PITCH_MIN"}, COLUMN_PITCH_MAX{"plaza", "COLUMN_PITCH_MAX"};
tune::Int COLUMN_IN{"plaza", "COLUMN_IN"};
/// The pool round the island is at least this wide per slot across, and
/// has a quay of the band's floor this wide round it.
tune::Int MOAT_MIN{"plaza", "MOAT_MIN"}, MOAT_MAX{"plaza", "MOAT_MAX"};
tune::Int QUAY{"plaza", "QUAY"};
tune::Float ROUND_POOL_CHANCE{"plaza", "ROUND_POOL_CHANCE"};

/// Plazas this many slots across are big: avenues and quarters.
tune::Int BIG_SLOTS{"plaza", "BIG_SLOTS"};
/// Half the width of each avenue, open floor from the fountain to the band.
tune::Int AVENUE_HALF_MIN{"plaza", "AVENUE_HALF_MIN"}, AVENUE_HALF_MAX{"plaza", "AVENUE_HALF_MAX"};
/// Beyond each avenue's edge, a strip this wide where statues line it,
/// standing this far out, this far apart. The courts keep out of it.
tune::Int LINING{"plaza", "LINING"};
tune::Int LINING_OUT{"plaza", "LINING_OUT"};
tune::Int LINING_PITCH_MIN{"plaza", "LINING_PITCH_MIN"}, LINING_PITCH_MAX{"plaza", "LINING_PITCH_MAX"};
/// Courts keep this far from the grand fountain's quay and the hall's edge.
tune::Int COURT_GAP{"plaza", "COURT_GAP"};
/// How far a court's things spread from the quarter's centre.
tune::Int COURT_SPREAD{"plaza", "COURT_SPREAD"};
/// A colonnade court is a ring of columns this far out, this far apart
/// along it, or a grid this far apart.
tune::Int COURT_RING_MIN{"plaza", "COURT_RING_MIN"}, COURT_RING_MAX{"plaza", "COURT_RING_MAX"};
tune::Int COURT_RING_STEP_MIN{"plaza", "COURT_RING_STEP_MIN"}, COURT_RING_STEP_MAX{"plaza", "COURT_RING_STEP_MAX"};
tune::Int COURT_GRID_MIN{"plaza", "COURT_GRID_MIN"}, COURT_GRID_MAX{"plaza", "COURT_GRID_MAX"};
/// Kiosks in a cluster, tries to fit them, and open floor kept round each.
tune::Int KIOSKS_MIN{"plaza", "KIOSKS_MIN"}, KIOSKS_MAX{"plaza", "KIOSKS_MAX"};
tune::Int KIOSK_TRIES{"plaza", "KIOSK_TRIES"};
tune::Int KIOSK_GAP{"plaza", "KIOSK_GAP"};
/// A booth walls in this much floor, with an opening this wide; a stall
/// is two walls this long meeting in a corner.
tune::Float BOOTH_CHANCE{"plaza", "BOOTH_CHANCE"};
tune::Int BOOTH_LONG{"plaza", "BOOTH_LONG"}, BOOTH_SHORT{"plaza", "BOOTH_SHORT"};
tune::Int BOOTH_OPENING{"plaza", "BOOTH_OPENING"};
tune::Int STALL_ARM{"plaza", "STALL_ARM"};

enum class Hall { Rect, Octagon, Oval };

/// The hall's outline, shrunk by an inset for the band and the columns.
struct Outline {
    Hall kind;
    int x0, y0, x1, y1;

    bool in(Vec2i p, int inset) const {
        float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
        float hw = (x1 - x0) / 2.0f - inset, hh = (y1 - y0) / 2.0f - inset;
        float dx = std::fabs(p.x + 0.5f - cx), dy = std::fabs(p.y + 0.5f - cy);
        if (hw <= 0 || hh <= 0) return false;
        switch (kind) {
        case Hall::Rect: return dx <= hw && dy <= hh;
        case Hall::Octagon: {
            float cut = std::min(x1 - x0, y1 - y0) / 4.0f;
            return dx <= hw && dy <= hh && dx + dy <= (x1 - x0 + y1 - y0) / 2.0f - cut - 1.414f * inset;
        }
        case Hall::Oval: return (dx / hw) * (dx / hw) + (dy / hh) * (dy / hh) <= 1.0f;
        }
        return false;
    }
};

/// A pool round an island of the given size in the middle of it, its width
/// rolled per slot across, with a quay of the band's floor all round.
/// Paints only over floor. Returns the island's top-left, and how far the
/// pool reaches from the middle.
std::pair<Vec2i, int> fountain(Chunk& c, Rng& rng, Vec2i centre, Vec2i island, int across, Floor band) {
    const Vec2i at = centre - Vec2i{island.x / 2, island.y / 2};
    auto onIsland = [&](Vec2i q) {
        return q.x >= at.x && q.y >= at.y && q.x < at.x + island.x && q.y < at.y + island.y;
    };
    const int moat = rng.range(MOAT_MIN, MOAT_MAX) * across;
    const bool round = rng.chance(ROUND_POOL_CHANCE);
    // A round pool reaches past the island's corners as well as its sides.
    const float rx = std::max(island.x / 2.0f * 1.42f + 1, island.x / 2.0f + moat);
    const float ry = std::max(island.y / 2.0f * 1.42f + 1, island.y / 2.0f + moat);
    auto inPool = [&](Vec2i q) {
        if (onIsland(q)) return false;
        if (!round)
            return q.x >= at.x - moat && q.y >= at.y - moat && q.x < at.x + island.x + moat &&
                   q.y < at.y + island.y + moat;
        float dx = (q.x + 0.5f - (at.x + island.x / 2.0f)) / rx;
        float dy = (q.y + 0.5f - (at.y + island.y / 2.0f)) / ry;
        return dx * dx + dy * dy <= 1.0f;
    };
    const int reach = int(round ? std::max(rx, ry) : std::max(island.x, island.y) / 2.0f + moat) + 1;
    for (int y = centre.y - reach - QUAY; y <= centre.y + reach + QUAY; y++)
        for (int x = centre.x - reach - QUAY; x <= centre.x + reach + QUAY; x++) {
            Vec2i q{x, y};
            if (!c.interior(q) || c.get(q) != Tile::Floor) continue;
            if (inPool(q)) {
                c.set(q, Tile::Water);
                continue;
            }
            bool quay = onIsland(q);
            for (int dy = -QUAY; dy <= QUAY && !quay; dy++)
                for (int dx = -QUAY; dx <= QUAY && !quay; dx++) quay = inPool(q + Vec2i{dx, dy});
            if (quay) c.setFloor(q, band);
        }
    return {at, reach};
}

/// A 2x2 column of wall at q, if it and the ring round it are all free.
bool column(Chunk& c, Vec2i q, const std::function<bool(Vec2i)>& free) {
    for (int dy = -1; dy <= COLUMN; dy++)
        for (int dx = -1; dx <= COLUMN; dx++)
            if (!free(q + Vec2i{dx, dy})) return false;
    for (int dy = 0; dy < COLUMN; dy++)
        for (int dx = 0; dx < COLUMN; dx++) c.set(q + Vec2i{dx, dy}, Tile::Wall);
    return true;
}

/// A kiosk with its top-left at q: a booth walled all round but for an
/// opening on the side `open` faces, or a stall walled on the two sides
/// away from `open` and `open2`. Everything it covers, and KIOSK_GAP of
/// floor round it, must be free. Returns the cell its table goes on.
std::optional<Vec2i> kiosk(Chunk& c, Rng& rng, Vec2i q, Vec2i open, Vec2i away,
                           const std::function<bool(Vec2i)>& free) {
    const bool booth = rng.chance(BOOTH_CHANCE);
    Vec2i size{STALL_ARM, STALL_ARM};
    if (booth) {
        // The opening runs along the long side.
        size = open.x != 0 ? Vec2i{BOOTH_SHORT + 2, BOOTH_LONG + 2} : Vec2i{BOOTH_LONG + 2, BOOTH_SHORT + 2};
    }
    for (int y = -KIOSK_GAP; y < size.y + KIOSK_GAP; y++)
        for (int x = -KIOSK_GAP; x < size.x + KIOSK_GAP; x++)
            if (!free(q + Vec2i{x, y})) return std::nullopt;
    auto wall = [&](Vec2i l) { c.set(q + l, Tile::Wall); };
    if (booth) {
        const Vec2i mid{size.x / 2, size.y / 2};
        for (int y = 0; y < size.y; y++)
            for (int x = 0; x < size.x; x++) {
                bool edge = x == 0 || y == 0 || x == size.x - 1 || y == size.y - 1;
                if (!edge) continue;
                // The opening: the middle of the side facing open.
                bool facing = (open.x < 0 && x == 0) || (open.x > 0 && x == size.x - 1) ||
                              (open.y < 0 && y == 0) || (open.y > 0 && y == size.y - 1);
                int along = open.x != 0 ? y : x, middle = open.x != 0 ? mid.y : mid.x;
                if (facing && along >= middle - BOOTH_OPENING / 2 && along < middle - BOOTH_OPENING / 2 + BOOTH_OPENING)
                    continue;
                wall({x, y});
            }
        return q + mid;
    }
    // The stall's walls run along its far sides, away from the avenues.
    const int wx = away.x > 0 ? size.x - 1 : 0, wy = away.y > 0 ? size.y - 1 : 0;
    for (int k = 0; k < STALL_ARM; k++) {
        wall({wx, k});
        wall({k, wy});
    }
    return q + Vec2i{wx, wy} - away * 2;
}

// The avenues are painted first, and their statues' places worked out;
// then each quarter rolls its court. Everything in a court keeps to free
// floor with a ring of it round, so nothing is cut off; a fountain court
// that doesn't fit becomes a colonnade court.
void bigPlaza(Chunk& c, Rng& rng, Plaza& p, const Outline& hall, int reach, int edge, Floor band) {
    const Vec2i centre = p.centre;
    const int avenue = rng.range(AVENUE_HALF_MIN, AVENUE_HALF_MAX);
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++) {
            Vec2i q{x, y};
            bool onAvenue = std::abs(x - centre.x) <= avenue || std::abs(y - centre.y) <= avenue;
            if (onAvenue && c.get(q) == Tile::Floor && hall.in(q, edge)) c.setFloor(q, band);
        }

    // Statues stand in pairs, one each side, from past the fountain's quay
    // (or the grand statue, when there is none) out to the hall's edge.
    const int pitch = rng.range(LINING_PITCH_MIN, LINING_PITCH_MAX);
    const int from = std::max<int>(reach + QUAY, LINING) + pitch / 2;
    for (Vec2i d : DIRS4) {
        const Vec2i across{d.y, d.x};
        for (int t = from;; t += pitch) {
            const Vec2i mid = centre + d * t;
            if (!hall.in(mid, edge + LINING_OUT)) break;
            for (int side : {-1, 1}) p.lining.push_back(mid + across * (side * (avenue + LINING_OUT)));
        }
    }

    const int radius = hall.x1 - centre.x;
    const int out = (avenue + LINING + int(radius * 0.7f)) / 2;
    auto free = [&](Vec2i q) {
        return c.interior(q) && c.get(q) == Tile::Floor && hall.in(q, edge + COURT_GAP) &&
               std::abs(q.x - centre.x) > avenue + LINING && std::abs(q.y - centre.y) > avenue + LINING &&
               cheb(q, centre) > reach + QUAY + COURT_GAP;
    };
    for (Vec2i s : {Vec2i{-1, -1}, Vec2i{1, -1}, Vec2i{-1, 1}, Vec2i{1, 1}}) {
        const Vec2i qc = centre + s * out;
        int theme = rng.range(0, 2);
        if (theme == 0) {
            // The most any court's pool and quay can reach from its middle.
            const int most = int(COURT_ISLAND.x / 2.0f * 1.42f + 1) + 1 + QUAY + 1;
            bool fits = true;
            for (int y = -most; y <= most && fits; y++)
                for (int x = -most; x <= most && fits; x++) fits = free(qc + Vec2i{x, y});
            if (fits) {
                p.courts.push_back(fountain(c, rng, qc, COURT_ISLAND, 1, band).first);
                continue;
            }
            theme = 1;
        }
        if (theme == 1) {
            if (rng.chance(0.5f)) {
                const int r = rng.range(COURT_RING_MIN, COURT_RING_MAX);
                const int step = rng.range(COURT_RING_STEP_MIN, COURT_RING_STEP_MAX);
                if (rng.chance(0.5f)) {
                    const int n = std::max(4, int(6.2832f * float(r) / float(step)));
                    for (int i = 0; i < n; i++) {
                        float a = 6.2832f * float(i) / float(n);
                        column(c, qc + Vec2i{int(std::cos(a) * float(r)), int(std::sin(a) * float(r))} - Vec2i{1, 1},
                               free);
                    }
                } else {
                    for (int k = -r; k < r; k += step)
                        for (Vec2i q : {Vec2i{k, -r}, Vec2i{r, k}, Vec2i{-k, r}, Vec2i{-r, -k}})
                            column(c, qc + q - Vec2i{1, 1}, free);
                }
            } else {
                const int grid = rng.range(COURT_GRID_MIN, COURT_GRID_MAX);
                for (int y = -COURT_SPREAD; y <= COURT_SPREAD; y += grid)
                    for (int x = -COURT_SPREAD; x <= COURT_SPREAD; x += grid) column(c, qc + Vec2i{x, y}, free);
            }
            continue;
        }
        const int want = rng.range(KIOSKS_MIN, KIOSKS_MAX);
        for (int t = 0, made = 0; t < KIOSK_TRIES && made < want; t++) {
            Vec2i q = qc + Vec2i{rng.range(-COURT_SPREAD, COURT_SPREAD), rng.range(-COURT_SPREAD, COURT_SPREAD)};
            // A booth opens toward the nearer avenue.
            Vec2i open = std::abs(q.x - centre.x) < std::abs(q.y - centre.y) ? Vec2i{-s.x, 0} : Vec2i{0, -s.y};
            if (auto table = kiosk(c, rng, q, open, s, free)) {
                p.kiosks.push_back(*table);
                made++;
            }
        }
    }
}

} // namespace

// The hall first, then its band, then the fountain and its quay, which
// paint over the band, and last the columns, which keep clear of all of it.
Plaza plaza(Chunk& c, Rng& rng, Vec2i island) {
    const int m = rng.range(MARGIN_MIN, MARGIN_MAX);
    const Outline hall{Hall(rng.range(0, 2)), m, m, c.w - m, c.h - m};
    const Vec2i centre{c.w / 2, c.h / 2};
    c.paint = randomFloor(rng);
    const Floor band = randomFloor(rng);
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++)
            if (hall.in({x, y}, 0)) carve(c, {x, y});
    const int bandIn = rng.range(BAND_IN_MIN, BAND_IN_MAX), bandOut = bandIn + rng.range(BAND_WIDTH_MIN, BAND_WIDTH_MAX);
    for (int y = 1; y < c.h - 1; y++)
        for (int x = 1; x < c.w - 1; x++)
            if (c.get({x, y}) == Tile::Floor && hall.in({x, y}, bandIn) && !hall.in({x, y}, bandOut))
                c.setFloor({x, y}, band);

    Plaza p{centre, {}, {}, 0, {}, {}, {}};
    int reach = 0;
    if (island.x > 0 && island.y > 0) {
        std::tie(p.island, reach) = fountain(c, rng, centre, island, c.slots.x, band);
        p.islandSize = island;
    }

    // Columns stand on a grid through the centre, in a band near the edge as
    // wide as the grid's pitch, so each side gets one row of them.
    int inner = std::min(c.w, c.h) / 2 - m;
    // How far in from the hall's edge its furniture reaches.
    int edge = bandOut;
    if (rng.chance(COLONNADE_CHANCE)) {
        const int pitch = rng.range(COLUMN_PITCH_MIN, COLUMN_PITCH_MAX);
        edge = COLUMN_IN + pitch;
        for (int y = centre.y % pitch; y < c.h - COLUMN; y += pitch)
            for (int x = centre.x % pitch; x < c.w - COLUMN; x += pitch) {
                Vec2i q{x, y};
                if (!hall.in(q, COLUMN_IN) || !hall.in(q + Vec2i{COLUMN - 1, COLUMN - 1}, COLUMN_IN) ||
                    hall.in(q, COLUMN_IN + pitch))
                    continue;
                bool clear = true;
                for (int dy = -1; dy <= COLUMN && clear; dy++)
                    for (int dx = -1; dx <= COLUMN && clear; dx++) clear = c.get(q + Vec2i{dx, dy}) == Tile::Floor;
                if (!clear) continue;
                for (int dy = 0; dy < COLUMN; dy++)
                    for (int dx = 0; dx < COLUMN; dx++) c.set(q + Vec2i{dx, dy}, Tile::Wall);
            }
        inner -= COLUMN_IN + pitch;
    }
    // Halfway between the fountain's quay and the columns or the band.
    p.ring = (reach + QUAY + inner - bandOut) / 2;
    if (c.slots.x >= BIG_SLOTS) bigPlaza(c, rng, p, hall, reach, edge, band);
    return p;
}

} // namespace gen
