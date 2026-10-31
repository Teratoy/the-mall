#include "render/cable_art.h"
#include "world/gen.h"
#include "world/world.h"
#include <algorithm>

// Server rooms are carved into solid wall once all other digging is done,
// so nothing cuts a second way in: a box of raised floor inside its walls,
// and on its south, east or west side an airlock, an inner door, a lobby
// two cells long and an outer door, then a short corridor out to the
// nearest floor. Inside, racks stand in rows facing south, the first
// against the back wall and the rest every six rows, so the aisle between
// shows; the console desk stands on the back wall at one end when the room
// is big enough; terminals stand in the open; and cables run on the floor
// in front of each row, joined by a trunk down one side and out through
// the front wall (user: grown to fit, not fixed maps). A room network now
// and then has one, and a data centre is a grid of corridors with as many
// as fit between them (user). All of it rolls dice of its own, so the rest
// of the mall comes out as it would without them.

namespace {

/// The three sizes of a server room's floor (user: closet, room, hall),
/// each a range of widths and depths, and how likely each is in a room
/// network and in a data centre.
struct Size {
    int wMin, wMax, hMin, hMax;
};
constexpr Size SIZES[] = {{6, 8, 5, 6}, {10, 13, 8, 10}, {15, 20, 12, 14}};
const std::vector<float> ROOMS_SIZE_WEIGHTS = {0.5f, 0.35f, 0.15f};
const std::vector<float> CENTRE_SIZE_WEIGHTS = {0.35f, 0.4f, 0.25f};
/// Cells of lobby between the two doors.
constexpr int LOBBY = 2;
/// The corridor from the outer door reaches floor within this many cells.
constexpr int SERVER_CORRIDOR_MAX = 10;
/// Tries at a spot for each room wanted.
constexpr int SERVER_TRIES = 60;
/// Rows of racks stand this far apart, so the aisle between shows over the
/// front row's tops (a rack hides the four rows behind it).
constexpr int RACK_ROW_PITCH = 6;
/// A row of racks this long or longer leaves a gap in its middle, this wide.
constexpr int RACK_RUN_GAP_FROM = 12, RACK_RUN_GAP = 2;
/// The desk stands on the back wall in rooms at least this wide and deep.
constexpr int DESK_ROOM_W = 8, DESK_ROOM_H = 7;
/// One terminal, then one more for each this many cells of floor.
constexpr int TERMINAL_FLOOR = 90;
/// The raised floor: a seam round every cell, in grey.
constexpr Floor SERVER_FLOOR{FloorPattern::Grid, 0};
/// The data centre's corridors: this wide, laid out on a grid this far apart.
constexpr int CENTRE_CORRIDOR = 4, CENTRE_PITCH_X = 28, CENTRE_PITCH_Y = 24;

constexpr Vec2i SIDES[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
constexpr uint8_t LINKS[] = {CABLE_N, CABLE_E, CABLE_S, CABLE_W};

/// Every cell of the box is interior wall.
bool solidBox(const Chunk& c, int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (!c.interior({xx, yy}) || c.get({xx, yy}) != Tile::Wall) return false;
    return true;
}

bool isMachineTile(Tile t) { return t == Tile::Rack || t == Tile::Console; }

} // namespace

namespace gen {

void dataCentre(Chunk& c, Rng& rng) {
    c.paint = randomFloor(rng);
    const int ox = rng.range(4, CENTRE_PITCH_X - 4), oy = rng.range(4, CENTRE_PITCH_Y - 4);
    for (int y = oy; y < c.h - 1; y += CENTRE_PITCH_Y) carveRect(c, 1, y, c.w - 2, CENTRE_CORRIDOR);
    for (int x = ox; x < c.w - 1; x += CENTRE_PITCH_X) carveRect(c, x, 1, CENTRE_CORRIDOR, c.h - 2);
}

} // namespace gen

int World::placeServerRooms(Chunk& c, Rng& dice, int count, const std::vector<gen::RoomBox>& keepOff) {
    const std::vector<float>& weights = c.kind == ChunkKind::DataCentre ? CENTRE_SIZE_WEIGHTS : ROOMS_SIZE_WEIGHTS;
    const Floor paint = c.paint;
    int made = 0;
    for (int t = 0; t < SERVER_TRIES * count && made < count; t++) {
        const Size& s = SIZES[size_t(dice.weighted(weights))];
        int w = dice.range(s.wMin, s.wMax), h = dice.range(s.hMin, s.hMax);
        int x = dice.range(2, c.w - 3 - w), y = dice.range(2, c.h - 3 - h);
        // The way in: south, east or west, never through the back wall.
        Vec2i in = SIDES[size_t(dice.range(1, 3))] * -1;
        Vec2i inner = in.y < 0 ? Vec2i{dice.range(x + 1, x + w - 2), y + h}
                    : in.x < 0 ? Vec2i{x + w, dice.range(y + 1, y + h - 1)}
                               : Vec2i{x - 1, dice.range(y + 1, y + h - 1)};
        Vec2i out = -in;
        Vec2i outer = inner + out * (LOBBY + 1);
        if (!solidBox(c, x - 1, y - 1, w + 2, h + 2)) continue;
        // The airlock and the wall either side of it are solid too.
        Vec2i across{out.y, out.x};
        bool clear = true;
        for (int i = 0; i <= LOBBY + 1 && clear; i++)
            for (int k = -1; k <= 1 && clear; k++) {
                Vec2i p = inner + out * i + across * k;
                clear = c.interior(p) && c.get(p) == Tile::Wall;
            }
        if (!clear) continue;
        std::vector<Vec2i> corridor;
        bool reached = false;
        for (Vec2i p = outer + out; c.interior(p) && int(corridor.size()) <= SERVER_CORRIDOR_MAX; p = p + out) {
            if (c.get(p) == Tile::Floor) {
                reached = std::none_of(keepOff.begin(), keepOff.end(), [&](const gen::RoomBox& b) {
                    return p.x >= b.x && p.y >= b.y && p.x < b.x + b.w && p.y < b.y + b.h;
                });
                break;
            }
            if (c.get(p) != Tile::Wall) break;
            corridor.push_back(p);
        }
        if (!reached) continue;

        c.paint = SERVER_FLOOR;
        gen::carveRect(c, x, y, w, h);
        for (int i = 1; i <= LOBBY; i++) gen::carve(c, inner + out * i);
        c.paint = paint;
        for (Vec2i p : corridor) gen::carve(c, p);
        const Vec2i o = c.origin();
        for (Vec2i door : {inner, outer}) {
            c.set(door, Tile::DoorLocked);
            c.setFloor(door, SERVER_FLOOR);
            airlocks_[o + door] = {in};
        }
        ServerRoom r{o.x + x, o.y + y, w, h, o + outer, o + inner};
        serverRooms_.push_back(r);
        furnishServerRoom(c, dice, r, o + inner + in);
        made++;
    }
    return made;
}

// Racks go in pairs of cells; whatever is left over at a row's end stays
// floor. Middle rows keep both end columns free, so the aisles join round
// them. Cables are laid last of all, from the machines they reach.
void World::furnishServerRoom(Chunk& c, Rng& dice, const ServerRoom& r, Vec2i keep) {
    const Vec2i o = c.origin();
    auto free = [&](Vec2i w) { return w != keep && r.contains(w) && c.get(w - o) == Tile::Floor; };
    std::vector<Vec2i> cable;
    auto lay = [&](Vec2i w) {
        if (r.contains(w) && c.get(w - o) == Tile::Floor) cable.push_back(w);
    };

    // The desk at one end of the back wall, away from a door in the side.
    int deskX = -1;
    if (r.w >= DESK_ROOM_W && r.h >= DESK_ROOM_H) {
        bool left = keep.x == r.x ? false : keep.x == r.x + r.w - 1 ? true : dice.chance(0.5f);
        deskX = left ? r.x : r.x + r.w - DESK_W;
        standPiece(c, Rig::Desk, {deskX, r.y}, uint32_t(dice.next()));
    }
    // The trunk runs down the side away from the desk.
    const int trunkX = deskX == r.x ? r.x + r.w - 1 : r.x;

    // Rows of racks: the back row against the wall, the rest every RACK_ROW_PITCH rows.
    for (int row = r.y; row + 2 <= r.y + r.h - 1; row += RACK_ROW_PITCH) {
        const bool back = row == r.y;
        int from = back ? r.x : r.x + 1, to = back ? r.x + r.w - 1 : r.x + r.w - 2;
        const int run = to - from + 1;
        const int gapAt = run >= RACK_RUN_GAP_FROM ? from + (run - RACK_RUN_GAP) / 2 : to + 1;
        std::vector<int> racks;
        auto inGap = [&](int cx) { return cx >= gapAt && cx < gapAt + RACK_RUN_GAP; };
        for (int cx = from; cx + 1 <= to; cx++) {
            if (inGap(cx) || inGap(cx + 1) || !free({cx, row}) || !free({cx + 1, row})) continue;
            standPiece(c, Rig::Rack, {cx, row}, uint32_t(dice.next()));
            racks.push_back(cx);
            cx++;
        }
        if (racks.empty()) continue;
        // A cable from under each rack's left cell, gathered by a run along
        // the row below, out to the trunk's side.
        for (int cx : racks) lay({cx, row + 1});
        int lo = std::min(racks.front(), trunkX), hi = std::max(racks.back(), trunkX);
        for (int cx = lo; cx <= hi; cx++) lay({cx, row + 2});
        if (back && deskX >= 0) {
            // The back row's run reaches the desk's wing, so the desk is wired in too.
            int toward = deskX == r.x ? deskX + DESK_W : deskX - 1;
            for (int cx = std::min(toward, lo); cx <= std::max(toward, hi); cx++) lay({cx, row + 2});
        }
    }
    // The trunk joins the runs and leaves through the front wall.
    int top = r.y + r.h, bottom = r.y + r.h - 1;
    for (Vec2i w : cable)
        if (w.x == trunkX) top = std::min(top, w.y);
    for (int cy = top; cy <= bottom; cy++) lay({trunkX, cy});
    std::sort(cable.begin(), cable.end(), [](Vec2i a, Vec2i b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    cable.erase(std::unique(cable.begin(), cable.end()), cable.end());
    std::unordered_set<Vec2i, Vec2iHash> laid(cable.begin(), cable.end());

    // Terminals in the open, clear of the cables and of the way in.
    int terminals = 1 + r.w * r.h / TERMINAL_FLOOR;
    for (int t = 0; t < 40 && terminals > 0; t++) {
        Vec2i w{dice.range(r.x + 1, r.x + r.w - 2), dice.range(r.y + 1, r.y + r.h - 2)};
        bool open = !laid.count(w) && cheb(w, keep) > 1;
        for (int dy = -1; dy <= 1 && open; dy++)
            for (int dx = -1; dx <= 1 && open; dx++) open = free(w + Vec2i{dx, dy});
        if (!open) continue;
        standPiece(c, Rig::Terminal, w, uint32_t(dice.next()));
        terminals--;
    }

    // Each cable joins the cables beside it; where a run ends it goes on
    // into the machine or wall it reaches: straight on if it can, else the
    // first machine beside it, else the first wall.
    for (Vec2i w : cable) {
        uint8_t links = 0;
        int cables = 0, last = 0;
        for (int i = 0; i < 4; i++)
            if (laid.count(w + SIDES[i])) links |= LINKS[i], cables++, last = i;
        if (cables == 1) {
            auto tileAt = [&](int i) { return c.get(w + SIDES[i] - o); };
            int ahead = (last + 2) % 4;
            if (isMachineTile(tileAt(ahead)) || tileAt(ahead) == Tile::Wall) {
                links |= LINKS[ahead];
            } else {
                int pick = -1;
                for (int i = 0; i < 4 && pick < 0; i++)
                    if (isMachineTile(tileAt(i))) pick = i;
                for (int i = 0; i < 4 && pick < 0; i++)
                    if (tileAt(i) == Tile::Wall) pick = i;
                if (pick >= 0) links |= LINKS[pick];
            }
        }
        cables_[w] = uint8_t(links | dice.range(0, CABLE_VARIANTS - 1) << 4);
    }
}

void World::standPiece(Chunk& c, Rig kind, Vec2i at, uint32_t seed) {
    const int id = int(pieces_.size());
    pieces_.push_back({kind, at, seed});
    const int w = kind == Rig::Desk ? DESK_W : kind == Rig::Rack ? 2 : 1, h = kind == Rig::Desk ? DESK_D : 1;
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++) {
            if (!ServerPiece::covers(kind, {dx, dy})) continue;
            Vec2i cell = at + Vec2i{dx, dy};
            pieceCells_[cell] = id;
            c.set(cell - c.origin(), kind == Rig::Rack ? Tile::Rack : Tile::Console);
        }
}
