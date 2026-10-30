#include "world/world.h"
#include "world/gen.h"
#include "human/human.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <utility>

namespace {

/// Chance each free stretch of border gets an exit door, beyond the one
/// every chunk with an empty neighbour is sure to have.
constexpr float NEW_DOOR_CHANCE = 0.35f;
constexpr int DOOR_WIDTH = 3;
/// Doors keep at least this far from the corners of their slot.
constexpr int DOOR_MARGIN = 8;
/// How likely each kind of chunk is.
/// Plazas only come square.
constexpr ChunkKind KINDS[] = {ChunkKind::Rooms, ChunkKind::Maze, ChunkKind::WaterMaze, ChunkKind::Plaza};
const std::vector<float> KIND_WEIGHTS = {0.5f, 0.25f, 0.13f, 0.12f};
/// Mazes, of stone or water, come no bigger than this many slots (user).
constexpr int MAZE_SLOTS_MAX = 2;
/// Width of the corridors that join stray areas and doors to the rest.
constexpr int JOIN_WIDTH = 3;
/// Keys and the chunk's human start at least this far into the chunk from
/// where it was entered, as a fraction of the way to the farthest floor.
constexpr float KEY_DEPTH = 0.4f;
/// Cakes per slot a chunk is made with. They never come back once eaten.
constexpr int CAKES_MIN = 2, CAKES_MAX = 5;
/// Slam rooms per slot of a room network, at most.
constexpr int SLAM_ROOMS_MAX = 1;
/// Chance a cake is rigged with darts, where a wall faces it down a straight run.
constexpr float DART_BAIT_CHANCE = 0.3f;
constexpr int SPIKES_MIN = 0, SPIKES_MAX = 2;
/// Spikes keep this far into the chunk, and this far from slam room gates.
constexpr int SPIKE_MARGIN = 4;
constexpr int SPIKE_GATE_GAP = 2;
/// Seconds spikes stay up once they go off; then they sink, armed again.
constexpr float SPIKES_UP_TIME = 3.0f;
/// A dart plate stays down this long after a volley, then re-arms.
constexpr float DART_RELOAD_TIME = 5.0f;
constexpr int DART_VOLLEY = 3;
constexpr float DART_GAP = 0.12f;
/// Cells a second, and the farthest a dart flies from its wall.
constexpr float DART_SPEED = 30.0f;
constexpr int DART_FLIGHT_MAX = 16;
constexpr float DART_DAMAGE_MIN = 0.12f, DART_DAMAGE_MAX = 0.2f;

/// How likely each table shape is, in TableShape order.
const std::vector<float> TABLE_SHAPE_WEIGHTS = {3.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f};
/// How likely each table material is, in TableMaterial order.
const std::vector<float> TABLE_MATERIAL_WEIGHTS = {3.0f, 2.0f, 2.0f, 2.0f, 1.5f, 1.0f, 1.0f, 1.5f};
/// Chance a table is of its chunk's house material rather than any.
constexpr float TABLE_HOUSE_CHANCE = 0.65f;

/// How likely each statue kind and material is, in enum order.
const std::vector<float> STATUE_KIND_WEIGHTS = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

const std::vector<float> STATUE_MATERIAL_WEIGHTS = {1.0f, 1.0f, 0.8f, 1.2f};
constexpr float PLINTH_CHANCE = 0.5f;
/// Chance a room gets a statue in its middle, and the most per slot.
constexpr float STATUE_ROOM_CHANCE = 0.35f;
/// Chance a room without one in its middle gets one in a niche instead,
/// and the kinds small enough for niches.
constexpr float NICHE_CHANCE = 0.25f;
/// A plaza's middle is a fountain this often, else its grand statue stands
/// on the floor. Smaller statues stand round it, this many per slot across.
constexpr float FOUNTAIN_CHANCE = 0.7f;
constexpr int PLAZA_STATUES_MIN = 2, PLAZA_STATUES_MAX = 6;
/// Now and then a plaza is given over to toilets: every statue in it a
/// toilet, half as many again of them, and real ones all round its walls.
constexpr float TOILET_PLAZA_CHANCE = 0.2f;
constexpr int TOILET_PLAZA_STATUES_EXTRA = 3;
constexpr StatueKind NICHE_KINDS[] = {StatueKind::Spoon, StatueKind::Bulb, StatueKind::Mug, StatueKind::Lamp,
                                      StatueKind::Clock};

constexpr int STATUES_PER_SLOT_MAX = 3;
/// The spawn room gets a statue this often, at least this far from its middle.
constexpr float SPAWN_STATUE_CHANCE = 0.6f;
constexpr int SPAWN_STATUE_GAP = 14;
/// Cells per side of a basin. The spawn room's stands this far from the
/// human waking in its middle, near enough to be seen through the daze of
/// waking. A plaza with no fountain gets this many.
constexpr int BASIN_SIZE = 2;
constexpr int SPAWN_BASIN_GAP_MIN = 4, SPAWN_BASIN_GAP_MAX = 6;
constexpr int PLAZA_BASINS_MIN = 1, PLAZA_BASINS_MAX = 2;
/// A statue keeps at least this many cells from any door, and from the
/// cell it is told to keep clear.
constexpr int STATUE_DOOR_GAP = 3;
constexpr int STATUE_CLEAR_GAP = 2;
/// How far from the centre a statue may be nudged to fit.
constexpr int STATUE_NUDGE = 4;

/// Chunk footprints, and how likely each is.
struct ShapePick {
    Footprint parts;
    float weight;
};
const std::vector<ShapePick> SHAPES = {{boxFootprint({1, 1}), 4.0f},
                                       {boxFootprint({2, 1}), 2.0f},
                                       {boxFootprint({1, 2}), 2.0f},
                                       {boxFootprint({2, 2}), 1.0f},
                                       {boxFootprint({3, 1}), 0.6f},
                                       {boxFootprint({1, 3}), 0.6f},
                                       {boxFootprint({4, 1}), 0.3f},
                                       {boxFootprint({1, 4}), 0.3f},
                                       {boxFootprint({3, 3}), 0.25f}};

TableMaterial randomMaterial(Rng& rng) { return TableMaterial(rng.weighted(TABLE_MATERIAL_WEIGHTS)); }

} // namespace

World::World(uint64_t seed) : rng_(seed) {}

World::~World() = default;

const Chunk* World::chunkAt(Vec2i slot) const {
    auto it = slotChunk_.find(slot);
    return it == slotChunk_.end() ? nullptr : chunks_[size_t(it->second)].get();
}

Chunk* World::chunkAt(Vec2i slot) {
    return const_cast<Chunk*>(std::as_const(*this).chunkAt(slot));
}

Tile World::tile(Vec2i cell) const {
    const Chunk* c = chunkAt(slotOf(cell));
    return c ? c->get(cell - c->origin()) : Tile::Void;
}

Floor World::floor(Vec2i cell) const {
    const Chunk* c = chunkAt(slotOf(cell));
    return c ? c->floor(cell - c->origin()) : Floor{};
}

WallStyle World::wall(Vec2i cell) const {
    const Chunk* c = chunkAt(slotOf(cell));
    return c ? c->wall(cell - c->origin()) : WallStyle{};
}

const Table* World::table(Vec2i cell) const {
    auto it = tableCells_.find(cell);
    return it == tableCells_.end() ? nullptr : &tables_[size_t(it->second)];
}

const Statue* World::statue(Vec2i cell) const {
    auto it = statueCells_.find(cell);
    return it == statueCells_.end() ? nullptr : &statues_[size_t(it->second)];
}

Chunk& World::addChunk(ChunkKind kind, Vec2i slot, const Footprint& parts) {
    int id = int(chunks_.size());
    chunks_.push_back(std::make_unique<Chunk>(id, kind, slot, parts));
    // Walls start alike; generators restyle those round rooms.
    std::fill(chunks_.back()->walls.begin(), chunks_.back()->walls.end(), gen::randomWall(rng_));
    for (Vec2i p : parts) slotChunk_[slot + p] = id;
    return *chunks_.back();
}

Human& World::makeSpawn(const HumanLook& look, uint64_t seed, const std::string& name) {
    Chunk& c = addChunk(ChunkKind::Spawn, {0, 0}, boxFootprint({1, 1}));
    c.paint = gen::randomFloor(rng_);
    gen::squareRoom(c);
    Vec2i centre{c.w / 2, c.h / 2};
    placeDoors(c, centre, 0.0f, 1, 1);
    if (rng_.chance(SPAWN_STATUE_CHANCE))
        for (int t = 0; t < 40; t++) {
            Vec2i at{rng_.range(SPAWN_STATUE_GAP, c.w - SPAWN_STATUE_GAP),
                     rng_.range(SPAWN_STATUE_GAP, c.h - SPAWN_STATUE_GAP)};
            if (cheb(at, centre) >= SPAWN_STATUE_GAP && placeStatue(c, at, centre)) break;
        }
    // Something to drink before the first door, a few steps from where it wakes.
    for (int t = 0; t < 40; t++) {
        Vec2i at{rng_.range(-SPAWN_BASIN_GAP_MAX, SPAWN_BASIN_GAP_MAX),
                 rng_.range(-SPAWN_BASIN_GAP_MAX, SPAWN_BASIN_GAP_MAX)};
        if (cheb(at, {}) >= SPAWN_BASIN_GAP_MIN && placeBasin(c, c.origin() + centre + at)) break;
    }
    std::vector<Vec2i> spots = deepSpots(c, centre);
    TableMaterial house = randomMaterial(rng_);
    size_t next = 0;
    if (auto s = tableSpot(c, spots, next, house, true)) keys_.insert(*s);
    if (auto s = tableSpot(c, spots, next, house)) cakes_.insert(*s);
    return addHuman(look, c.origin() + centre, seed, name);
}

Human& World::addHuman(const HumanLook& look, Vec2i cell) { return addHuman(look, cell, rng_.next()); }

Human& World::addHuman(const HumanLook& look, Vec2i cell, uint64_t seed, const std::string& name) {
    int id = int(humans_.size());
    humans_.push_back(std::make_unique<Human>(id, look, cell, seed, name));
    Human& h = *humans_.back();
    occupied_[cell] = &h;
    h.born = time_;
    h.observe(*this);
    return h;
}

size_t World::livingCount() const {
    return size_t(std::count_if(humans_.begin(), humans_.end(), [](const auto& h) { return !h->dead(); }));
}

Human* World::occupant(Vec2i cell) const {
    auto it = occupied_.find(cell);
    return it == occupied_.end() ? nullptr : it->second;
}

void World::moveOccupant(Vec2i from, Vec2i to, Human* h) {
    occupied_.erase(from);
    occupied_[to] = h;
}

void World::ensureGenerated(Vec2i cell) {
    Vec2i s = slotOf(cell);
    if (!generated(s)) generate(s, cell);
}

// Picks a footprint and placement that covers the slot without overlapping
// any chunk, fills it with a maze or a room network, then doors, keys and a
// human, and cakes, each key and cake on a table, then traps. A water maze is dug as a plain one and its
// walls turned to water once everything is joined up. Each new door gets a
// key, and so does each slam room, so there are as many keys as locked
// doors and gates that can be.
void World::generate(Vec2i slot, Vec2i entry) {
    // Each slot of each footprint in turn is tried on the one entered.
    std::vector<std::pair<Vec2i, const Footprint*>> options;
    std::vector<float> weights;
    for (const ShapePick& sp : SHAPES)
        for (Vec2i on : sp.parts) {
            Vec2i origin = slot - on;
            bool free = std::none_of(sp.parts.begin(), sp.parts.end(),
                                     [&](Vec2i p) { return generated(origin + p); });
            if (!free) continue;
            options.push_back({origin, &sp.parts});
            weights.push_back(sp.weight);
        }
    auto [origin, parts] = options[size_t(rng_.weighted(weights))];

    std::vector<float> kindWeights = KIND_WEIGHTS;
    const Vec2i box = footprintBox(*parts);
    if (box.x != box.y || int(parts->size()) != box.x * box.y) kindWeights[3] = 0.0f;
    if (int(parts->size()) > MAZE_SLOTS_MAX) kindWeights[1] = kindWeights[2] = 0.0f;
    Chunk& c = addChunk(KINDS[rng_.weighted(kindWeights)], origin, *parts);
    const int slotCount = c.slotCount();
    markDoors(c);
    std::vector<gen::RoomBox> rooms;
    // A plaza's grand statue is rolled first, so its fountain's island fits it.
    const bool toiletPlaza = c.kind == ChunkKind::Plaza && rng_.chance(TOILET_PLAZA_CHANCE);
    Statue grand = rollStatue(true, toiletPlaza ? std::optional{StatueKind::Toilet} : std::nullopt);
    grand.plinth = true;
    grand.size = statueSize(grand);
    gen::Plaza plaza{};
    if (c.kind == ChunkKind::Rooms)
        rooms = gen::rooms(c, rng_);
    else if (c.kind == ChunkKind::Plaza)
        plaza = gen::plaza(c, rng_, rng_.chance(FOUNTAIN_CHANCE) ? grand.size + Vec2i{2, 2} : Vec2i{});
    else
        gen::maze(c, rng_);
    int fresh = placeDoors(c, entry - c.origin(), NEW_DOOR_CHANCE, 1, 4 * slotCount);
    gen::connectRegions(c, JOIN_WIDTH);
    if (c.kind == ChunkKind::WaterMaze) gen::flood(c);
    // Statues once all digging is done, as nothing digs round them.
    placeStatues(c, rooms);
    placeToilets(c);
    if (c.kind == ChunkKind::Plaza) placePlazaStatues(c, plaza, grand, toiletPlaza);
    // After the statues, so the toilets take none of their room.
    if (toiletPlaza) placePlazaToilets(c);
    // Slam rooms go in last, so nothing digs a second way into them.
    std::vector<gen::SlamRoom> slams;
    if (c.kind == ChunkKind::Rooms) slams = gen::slamRooms(c, rng_, rng_.range(0, SLAM_ROOMS_MAX) * slotCount);
    // A plaza without a fountain has basins to drink from instead.
    if (c.kind == ChunkKind::Plaza && plaza.islandSize.x == 0) {
        int basins = rng_.range(PLAZA_BASINS_MIN, PLAZA_BASINS_MAX);
        for (Vec2i s : deepSpots(c, entry - c.origin()))
            if (basins > 0 && placeBasin(c, s)) basins--;
    }
    std::vector<Vec2i> spots = deepSpots(c, entry - c.origin());
    // Nothing lies in a slam room but its bait.
    std::erase_if(spots, [&](Vec2i s) {
        return std::any_of(slams.begin(), slams.end(),
                           [&](const gen::SlamRoom& r) { return r.contains(s - c.origin()); });
    });
    // Kiosk tables come first: those deep enough take the keys, and the
    // rest the first cakes.
    std::vector<Vec2i> kioskCakes;
    std::vector<Vec2i> deepKiosks;
    for (Vec2i k : plaza.kiosks) {
        auto it = std::find(spots.begin(), spots.end(), c.origin() + k);
        if (it == spots.end()) {
            kioskCakes.push_back(c.origin() + k);
            continue;
        }
        spots.erase(it);
        deepKiosks.push_back(c.origin() + k);
    }
    spots.insert(spots.begin(), deepKiosks.begin(), deepKiosks.end());
    TableMaterial house = randomMaterial(rng_);
    size_t next = 0, nextKiosk = 0;
    int keys = fresh + int(slams.size());
    for (int i = 0; i < keys; i++)
        if (auto s = tableSpot(c, spots, next, house, true)) keys_.insert(*s);
    // Tables took some spots, and their room round them is best left clear.
    for (; next < spots.size(); next++) {
        Vec2i s = spots[next];
        if (tile(s) != Tile::Floor || occupant(s)) continue;
        Rng looks(rng_.next());
        addHuman(HumanLook::random(looks), s);
        next++;
        break;
    }
    int cakes = rng_.range(CAKES_MIN, CAKES_MAX) * slotCount;
    for (; cakes > 0; cakes--) {
        auto s = tableSpot(c, kioskCakes, nextKiosk, house);
        if (!s) s = tableSpot(c, spots, next, house);
        if (!s) break;
        cakes_.insert(*s);
        if (!rng_.chance(DART_BAIT_CHANCE)) continue;
        if (auto d = gen::dartShooter(c, rng_, *s - c.origin()))
            traps_[*s] = {TrapKind::Darts, c.origin() + d->first, d->second};
    }
    // The bait is on a table too, when one fits; else on the plate itself.
    for (const gen::SlamRoom& r : slams) {
        Vec2i plate = c.origin() + r.plate;
        placeTable(c, plate, house);
        traps_[plate] = {TrapKind::Slam, c.origin() + r.gate, {}};
        cakes_.insert(plate);
    }
    placeSpikes(c, slams);
}

// Shapes are tried likeliest first, each turned at random and slid every
// way that keeps the cell under it. Every cell under the table must be
// free floor, and every cell round it, the inside of a corner's bend
// included, floor: then walking round it joins up whatever it stands in.
bool World::placeTable(Chunk& c, Vec2i cell, TableMaterial house) {
    std::vector<float> weights = TABLE_SHAPE_WEIGHTS;
    std::vector<TableShape> order;
    for (size_t i = 0; i < weights.size(); i++) {
        int s = rng_.weighted(weights);
        order.push_back(TableShape(s));
        weights[size_t(s)] = 0.0f;
    }
    TableMaterial material = rng_.chance(TABLE_HOUSE_CHANCE) ? house : randomMaterial(rng_);
    for (TableShape shape : order) {
        Table t{shape, material, {}, TABLE_SIZES[size_t(shape)], uint8_t(rng_.range(0, 3)), uint32_t(rng_.next())};
        if (shape != TableShape::Corner && rng_.chance(0.5f)) t.size = {t.size.y, t.size.x};
        std::vector<Vec2i> under;
        for (int y = 0; y < t.size.y; y++)
            for (int x = 0; x < t.size.x; x++)
                if (t.coversLocal({x, y})) under.push_back({x, y});
        rng_.shuffle(under);
        for (Vec2i u : under) {
            t.at = cell - u;
            bool fits = true;
            for (int y = -1; y <= t.size.y && fits; y++)
                for (int x = -1; x <= t.size.x && fits; x++) {
                    Vec2i w = t.at + Vec2i{x, y}, l = w - c.origin();
                    if (!c.inside(l) || c.get(l) != Tile::Floor) {
                        fits = false;
                        break;
                    }
                    if (t.coversLocal({x, y}))
                        fits = !hasKey(w) && !hasCake(w) && !occupant(w) && !traps_.count(w);
                }
            if (!fits) continue;
            int id = int(tables_.size());
            tables_.push_back(t);
            for (Vec2i v : under) {
                tableCells_[t.at + v] = id;
                c.set(t.at + v - c.origin(), Tile::Table);
            }
            return true;
        }
    }
    return false;
}

// Floor all round, as for tables, so it walls nothing off.
bool World::placeBasin(Chunk& c, Vec2i cell) {
    for (int y = -1; y <= BASIN_SIZE; y++)
        for (int x = -1; x <= BASIN_SIZE; x++) {
            Vec2i w = cell + Vec2i{x, y}, l = w - c.origin();
            if (!c.inside(l) || c.get(l) != Tile::Floor || behindStatues_.count(w)) return false;
            if (x >= 0 && y >= 0 && x < BASIN_SIZE && y < BASIN_SIZE &&
                (hasKey(w) || hasCake(w) || occupant(w) || traps_.count(w)))
                return false;
        }
    for (int y = 0; y < BASIN_SIZE; y++)
        for (int x = 0; x < BASIN_SIZE; x++) c.set(cell + Vec2i{x, y} - c.origin(), Tile::Basin);
    return true;
}

// One kind is rolled and tried on its plinth if it rolled one, then off
// it, at the spot nearest the centre it fits in. A kind that fits nowhere
// gives way to none, so the small ones don't crowd out the big. Every cell
// under it and round it must be floor, so walking round it joins up
// whatever it stands in, as with tables.
Statue World::rollStatue(bool grand, std::optional<StatueKind> kind) {
    StatueKind k = StatueKind(rng_.weighted(STATUE_KIND_WEIGHTS));
    Statue s{kind.value_or(k), StatueMaterial(rng_.weighted(STATUE_MATERIAL_WEIGHTS)),
             {}, {}, rng_.chance(0.5f), rng_.chance(PLINTH_CHANCE), uint32_t(rng_.next()), grand};
    s.size = statueSize(s);
    return s;
}

// The grand statue stands a cell in from its island's shore, or in the
// middle of the floor when there is no fountain. A big plaza's statues
// line its avenues, and each fountain court's stands on its plinth in the
// middle of its island. Otherwise the rest stand evenly round a ring about
// the middle, each where it fits nearest its place.
void World::placePlazaStatues(Chunk& c, const gen::Plaza& plaza, Statue grand, bool toilets) {
    std::optional<StatueKind> kind;
    if (toilets) kind = StatueKind::Toilet;
    if (plaza.islandSize.x > 0)
        standStatue(c, grand, plaza.island + Vec2i{1, 1});
    else
        placeStatue(c, plaza.centre, {-100, -100}, true, kind);
    for (Vec2i island : plaza.courts) {
        Statue s = rollStatue(false, kind);
        s.plinth = true;
        s.size = statueSize(s);
        const Vec2i room = gen::COURT_ISLAND - s.size;
        standStatue(c, s, island + Vec2i{room.x / 2, room.y / 2});
    }
    if (!plaza.lining.empty()) {
        for (Vec2i at : plaza.lining) placeStatue(c, at, {-100, -100}, false, kind);
        return;
    }
    const int count = (rng_.range(PLAZA_STATUES_MIN, PLAZA_STATUES_MAX) + (toilets ? TOILET_PLAZA_STATUES_EXTRA : 0)) *
                      c.slots.x;
    const float turn = rng_.range(0.0f, 6.2832f);
    for (int i = 0; i < count; i++) {
        float a = turn + 6.2832f * float(i) / float(count);
        Vec2i at = plaza.centre + Vec2i{int(std::cos(a) * plaza.ring), int(std::sin(a) * plaza.ring)};
        placeStatue(c, at, {-100, -100}, false, kind);
    }
}

bool World::placeStatue(Chunk& c, Vec2i centre, Vec2i clear, bool grand, std::optional<StatueKind> kind) {
    std::vector<Vec2i> nudges;
    for (int y = -STATUE_NUDGE; y <= STATUE_NUDGE; y++)
        for (int x = -STATUE_NUDGE; x <= STATUE_NUDGE; x++) nudges.push_back({x, y});
    rng_.shuffle(nudges);
    std::stable_sort(nudges.begin(), nudges.end(), [](Vec2i a, Vec2i b) { return cheb(a, {}) < cheb(b, {}); });
    Statue s = rollStatue(grand, kind);
    const bool rolled = s.plinth;

    for (int pass = 0; pass < (rolled ? 2 : 1); pass++) {
        s.plinth = rolled && pass == 0;
        s.size = statueSize(s);
        for (Vec2i nudge : nudges) {
            Vec2i at = centre - Vec2i{s.size.x / 2, s.size.y / 2} + nudge;
            if (!statueFits(c, s.size, at, clear)) continue;
            standStatue(c, s, at);
            return true;
        }
    }
    return false;
}

bool World::statueFits(const Chunk& c, Vec2i size, Vec2i at, Vec2i clear) const {
    for (int y = -STATUE_DOOR_GAP; y < size.y + STATUE_DOOR_GAP; y++)
        for (int x = -STATUE_DOOR_GAP; x < size.x + STATUE_DOOR_GAP; x++) {
            Vec2i l = at + Vec2i{x, y}, w = c.origin() + l;
            bool ring = x >= -1 && y >= -1 && x <= size.x && y <= size.y;
            if (!c.inside(l)) {
                if (ring) return false;
                continue;
            }
            Tile tile = c.get(l);
            if (tile == Tile::DoorOpen || tile == Tile::DoorLocked) return false;
            if (!ring) continue;
            if (tile != Tile::Floor || hasKey(w) || hasCake(w) || occupant(w) || traps_.count(w)) return false;
            if (cheb(l, clear) <= STATUE_CLEAR_GAP) return false;
        }
    return true;
}

void World::standStatue(Chunk& c, Statue s, Vec2i at) {
    s.at = c.origin() + at;
    int id = int(statues_.size());
    statues_.push_back(s);
    for (int y = 0; y < s.size.y; y++)
        for (int x = 0; x < s.size.x; x++) {
            statueCells_[s.at + Vec2i{x, y}] = id;
            c.set(at + Vec2i{x, y}, Tile::Statue);
        }
    for (int y = 1; y <= statueOverhang(s); y++)
        for (int x = 0; x < s.size.x; x++) behindStatues_.insert(s.at + Vec2i{x, -y});
}

void World::placeStatues(Chunk& c, const std::vector<gen::RoomBox>& rooms) {
    std::vector<size_t> order(rooms.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    rng_.shuffle(order);
    int left = STATUES_PER_SLOT_MAX * c.slotCount();
    for (size_t i : order) {
        if (left <= 0) break;
        if (rng_.chance(STATUE_ROOM_CHANCE) && placeStatue(c, rooms[i].centre, {-100, -100}))
            left--;
        else if (rng_.chance(NICHE_CHANCE) && placeNiche(c, rooms[i]))
            left--;
    }
}

// The niche opens onto a run of the room's floor as wide as the statue,
// all of it the topmost floor of its column in the room's box. The recess
// and a cell of wall round it, all but its open side, must be solid, so
// the statue in it blocks no way through. It takes the floor in front.
bool World::placeNiche(Chunk& c, const gen::RoomBox& room) {
    std::vector<int> topmost(size_t(room.w), -1);
    for (int x = 0; x < room.w; x++)
        for (int y = room.y; y < room.y + room.h; y++)
            if (c.get({room.x + x, y}) == Tile::Floor) {
                topmost[size_t(x)] = y;
                break;
            }
    Statue s{rng_.pick(NICHE_KINDS), StatueMaterial(rng_.weighted(STATUE_MATERIAL_WEIGHTS)),
             {}, {}, rng_.chance(0.5f), false, uint32_t(rng_.next())};
    s.size = STATUE_BASES[size_t(s.kind)];
    std::vector<int> starts;
    for (int x = 0; x + s.size.x <= room.w; x++) starts.push_back(x);
    rng_.shuffle(starts);
    for (int x0 : starts) {
        int front = topmost[size_t(x0)];
        if (front < 0) continue;
        bool fits = true;
        for (int x = x0; x < x0 + s.size.x && fits; x++) fits = topmost[size_t(x)] == front;
        Vec2i at{room.x + x0, front - s.size.y};
        for (int y = at.y - 1; y < front && fits; y++)
            for (int x = at.x - 1; x <= at.x + s.size.x && fits; x++)
                fits = c.interior({x, y}) && c.get({x, y}) == Tile::Wall;

        if (!fits) continue;
        for (int y = at.y; y < front; y++)
            for (int x = at.x; x < at.x + s.size.x; x++) {
                c.set({x, y}, Tile::Floor);
                c.setFloor({x, y}, c.floor({x, front}));
            }
        standStatue(c, s, at);
        return true;
    }
    return false;
}

std::optional<Vec2i> World::tableSpot(Chunk& c, const std::vector<Vec2i>& spots, size_t& next, TableMaterial house,
                                       bool orFloor) {
    while (next < spots.size()) {
        Vec2i s = spots[next++];
        if (placeTable(c, s, house)) return s;
    }
    if (orFloor)
        for (Vec2i s : spots)
            if (tile(s) == Tile::Floor && !hasKey(s) && !hasCake(s) && !occupant(s)) return s;
    return std::nullopt;
}

// Only where there is room to walk round them, so a known one never cuts a
// way off, and far enough in that nobody coming through a door, or out of
// a slam room, steps straight onto one.
void World::placeSpikes(Chunk& c, const std::vector<gen::SlamRoom>& slams) {
    int count = rng_.range(SPIKES_MIN, SPIKES_MAX) * c.slotCount();
    std::vector<Vec2i> open;
    for (int y = SPIKE_MARGIN; y < c.h - SPIKE_MARGIN; y++)
        for (int x = SPIKE_MARGIN; x < c.w - SPIKE_MARGIN; x++) {
            Vec2i l{x, y}, cell = c.origin() + l;
            // The margin holds round an odd footprint's inside corners too.
            if (!c.interiorRect(x - SPIKE_MARGIN + 1, y - SPIKE_MARGIN + 1, 2 * SPIKE_MARGIN - 1,
                                2 * SPIKE_MARGIN - 1) ||
                !gen::openAround(c, l) || hasKey(cell) || hasCake(cell) || occupant(cell) || behindStatues_.count(cell))
                continue;
            if (std::none_of(slams.begin(), slams.end(), [&](const gen::SlamRoom& r) {
                    return r.contains(l) || cheb(l, r.gate) <= SPIKE_GATE_GAP;
                }))
                open.push_back(cell);
        }
    rng_.shuffle(open);
    for (Vec2i cell : open) {
        if (count <= 0) break;
        // Never two side by side, which could wall a way off between them.
        bool alone = true;
        for (Vec2i d : DIRS8) alone &= traps_.count(cell + d) == 0;
        if (!alone) continue;
        traps_[cell] = {TrapKind::Spikes, {}, {}};
        count--;
    }
}

std::vector<World::Edge> World::perimeter(const Chunk& c) const {
    std::vector<Edge> edges;
    for (Vec2i p : c.parts) {
        Vec2i s = c.slot + p;
        for (Vec2i d : DIRS4)
            if (!c.covers(s + d)) edges.push_back({edgeBetween(s, s + d), s, s + d});
    }
    return edges;
}

void World::markDoors(Chunk& c) {
    for (const Edge& e : perimeter(c)) {
        auto it = doors_.find(e.key);
        if (it == doors_.end()) continue;
        const Door& d = it->second;
        for (Vec2i cell : d.cellsIn(e.in)) c.set(cell - c.origin(), d.locked ? Tile::DoorLocked : Tile::DoorOpen);
    }
}

int World::placeDoors(Chunk& c, Vec2i from, float chance, int minNew, int maxNew) {
    const std::vector<int> side = gen::sides(c);
    // from may be in the border, as a door is.
    int home = -1;
    for (Vec2i d : {Vec2i{0, 0}, DIRS4[0], DIRS4[1], DIRS4[2], DIRS4[3]}) {
        Vec2i p = from + d;
        if (c.inside(p) && side[size_t(p.y * c.w + p.x)] >= 0) {
            home = side[size_t(p.y * c.w + p.x)];
            break;
        }
    }
    std::vector<Edge> free;
    for (const Edge& e : perimeter(c)) {
        if (!generated(e.out)) {
            free.push_back(e);
            continue;
        }
        auto it = doors_.find(e.key);
        if (it != doors_.end()) drawDoor(c, it->second);
    }
    rng_.shuffle(free);
    int made = 0;
    for (const Edge& e : free) {
        if (made >= maxNew) break;
        if (made >= minNew && !rng_.chance(chance)) continue;
        Door d{e.key.slot, e.key.side, 0, DOOR_WIDTH, true};
        Vec2i inward = e.in == d.slot ? -d.dir() : d.dir();
        // Every cell just inside the door on from's side of the water.
        auto reachable = [&](int offset) {
            d.offset = offset;
            for (Vec2i cell : d.cellsIn(e.in)) {
                Vec2i p = cell - c.origin() + inward;
                if (side[size_t(p.y * c.w + p.x)] != home) return false;
            }
            return true;
        };
        std::vector<int> offsets;
        for (int o = DOOR_MARGIN; o <= CHUNK - DOOR_MARGIN - DOOR_WIDTH; o++) offsets.push_back(o);
        rng_.shuffle(offsets);
        auto fit = std::find_if(offsets.begin(), offsets.end(), reachable);
        if (fit == offsets.end()) continue;
        d.offset = *fit;
        doors_.insert_or_assign(e.key, d);
        for (Vec2i s : {d.slot, d.other()})
            for (Vec2i cell : d.cellsIn(s)) doorCells_.insert_or_assign(cell, e.key);
        drawDoor(c, d);
        made++;
    }
    return made;
}

void World::drawDoor(Chunk& c, const Door& d) {
    for (Vec2i s : {d.slot, d.other()}) {
        if (!c.covers(s)) continue;
        std::vector<Vec2i> cells = d.cellsIn(s);
        for (Vec2i cell : cells) c.set(cell - c.origin(), d.locked ? Tile::DoorLocked : Tile::DoorOpen);
        Vec2i inward = s == d.slot ? -d.dir() : d.dir();
        gen::connectDoor(c, cells[cells.size() / 2] - c.origin() + inward, d.width);
    }
}

std::vector<Vec2i> World::deepSpots(const Chunk& c, Vec2i from) {
    std::vector<int> dist(size_t(c.w * c.h), -1);
    std::deque<Vec2i> open{from};
    dist[size_t(from.y * c.w + from.x)] = 0;
    int far = 0;
    while (!open.empty()) {
        Vec2i p = open.front();
        open.pop_front();
        int d = dist[size_t(p.y * c.w + p.x)];
        far = std::max(far, d);
        for (Vec2i dir : DIRS4) {
            Vec2i n = p + dir;
            if (!c.inside(n) || !passable(c.get(n)) || dist[size_t(n.y * c.w + n.x)] != -1) continue;
            dist[size_t(n.y * c.w + n.x)] = d + 1;
            open.push_back(n);
        }
    }
    std::vector<Vec2i> spots;
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            Vec2i cell = c.origin() + Vec2i{x, y};
            if (c.get({x, y}) == Tile::Floor && dist[size_t(y * c.w + x)] >= KEY_DEPTH * far &&
                !hasKey(cell) && !hasCake(cell) && !occupant(cell) && !behindStatues_.count(cell))
                spots.push_back(cell);

        }
    rng_.shuffle(spots);
    return spots;
}

void World::putDown(Vec2i cell, int cakes, const std::vector<Bottle>& bottles) {
    if (cakes <= 0 && bottles.empty()) return;
    Pile& p = piles_[cell];
    p.cakes += std::max(0, cakes);
    p.bottles.insert(p.bottles.end(), bottles.begin(), bottles.end());
}

int World::takePileCakes(Vec2i cell, int n) {
    auto it = piles_.find(cell);
    if (it == piles_.end()) return 0;
    int got = std::clamp(n, 0, it->second.cakes);
    it->second.cakes -= got;
    if (it->second.empty()) piles_.erase(it);
    return got;
}

Bottle World::takePileBottle(Vec2i cell, size_t i) {
    auto it = piles_.find(cell);
    if (it == piles_.end() || i >= it->second.bottles.size()) return {};
    Bottle b = it->second.bottles[i];
    it->second.bottles.erase(it->second.bottles.begin() + long(i));
    if (it->second.empty()) piles_.erase(it);
    return b;
}

float World::pourFromPile(Vec2i cell, size_t i, Bottle& into, float amount) {
    auto it = piles_.find(cell);
    if (it == piles_.end() || i >= it->second.bottles.size()) return 0.0f;
    return it->second.bottles[i].pourInto(into, amount);
}

bool World::unlock(Vec2i cell) {
    if (gates_.erase(cell) > 0) {
        setTile(cell, Tile::DoorOpen);
        return true;
    }
    auto it = doorCells_.find(cell);
    if (it == doorCells_.end()) return false;
    Door& d = doors_.at(it->second);
    if (!d.locked) return false;
    d.locked = false;
    for (Vec2i s : {d.slot, d.other()})
        if (Chunk* c = chunkAt(s))
            for (Vec2i p : d.cellsIn(s)) c->set(p - c->origin(), Tile::DoorOpen);
    return true;
}

void World::setTile(Vec2i cell, Tile t) {
    if (Chunk* c = chunkAt(slotOf(cell))) c->set(cell - c->origin(), t);
}

const Trap* World::trap(Vec2i cell) const {
    auto it = traps_.find(cell);
    return it == traps_.end() ? nullptr : &it->second;
}

// The one who set it off always knows; anyone else has to see the cell.
void World::setOff(Vec2i cell, Human& h) {
    auto it = traps_.find(cell);
    if (it == traps_.end() || !it->second.armed()) return;
    Trap& t = it->second;
    for (auto& o : humans_)
        if (!o->dead() && (o.get() == &h || o->sight.sees(cell))) o->learnTrap(cell, t.kind, o.get() == &h);
    switch (t.kind) {
    case TrapKind::Spikes:
        t.sprung = SPIKES_UP_TIME;
        h.impale(*this);
        break;
    case TrapKind::Slam:
        t.spent = true;
        t.shutting = true;
        h.shutIn(t.from);
        break;
    case TrapKind::Darts:
        // Lifted off a table, they come at whoever reached for it, from a
        // wall facing them if there is one in range.
        if (h.cell != cell)
            if (Chunk* c = chunkAt(slotOf(h.cell)))
                if (auto s = gen::dartShooter(*c, rng_, h.cell - c->origin())) {
                    t.from = c->origin() + s->first;
                    t.dir = s->second;
                }
        t.sprung = DART_RELOAD_TIME;
        t.volley = DART_VOLLEY;
        t.reload = 0.0f;
        break;
    }
}

// A gate someone stands in waits for them to step out of it. A dart checks
// every cell it reached this tick, and stops at the first wall or in the
// first human.
void World::updateTraps(float dt) {
    for (auto& [cell, t] : traps_) {
        t.sprung = std::max(0.0f, t.sprung - dt);
        if (t.shutting && !occupant(t.from)) {
            t.shutting = false;
            setTile(t.from, Tile::DoorLocked);
            gates_.insert(t.from);
        }
        if (t.volley > 0 && (t.reload -= dt) <= 0.0f) {
            darts_.push_back({t.from, t.dir});
            t.volley--;
            t.reload = DART_GAP;
        }
    }
    std::erase_if(darts_, [&](Dart& d) {
        int was = int(d.flown);
        d.flown += DART_SPEED * dt;
        for (int i = was + 1; i <= int(d.flown); i++) {
            Vec2i cell = d.from + d.dir * i;
            if (i > DART_FLIGHT_MAX || opaque(tile(cell))) return true;
            if (Human* h = occupant(cell)) {
                h->hitByDart(rng_.range(DART_DAMAGE_MIN, DART_DAMAGE_MAX), d.dir);
                return true;
            }
        }
        return false;
    });
}

// By index: a human stepping into an empty slot adds a chunk and a human.
void World::update(float dt) {
    time_ += dt;
    for (auto& c : chunks_) c->simulate(dt);
    updateTraps(dt);
    blood_.update(dt);
    for (const auto& [c, f] : blood_.takeFoul()) mess_.foulWater(c, f);
    mess_.update(dt, [&](Vec2i c) { return drinkable(tile(c)); });
    for (size_t i = 0; i < humans_.size(); i++) humans_[i]->update(*this, dt);
}
