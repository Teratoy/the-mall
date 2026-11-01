#include "world/world.h"
#include "world/gen.h"
#include "world/gen_tuning.h"
#include "human/human.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <tuple>
#include <utility>

namespace {

/// Smells are gathered this often, in seconds.
constexpr float SMELL_EVERY = 1.0f;
/// Feces smell until this fresh (Mess::freshness), as dry as no longer to
/// stick to a foot; the fresher and the bigger, the stronger.
constexpr float FECES_SMELL_DRY = 0.4f;
/// Water this foul smells, as foul as to sicken (the user), at least this
/// strongly, fouler water more.
constexpr float WATER_SMELL_FOUL = 0.5f, WATER_SMELL_MIN = 0.2f;

/// Chance each free stretch of border gets an exit door, beyond the one
/// every chunk with an empty neighbour is sure to have.
tune::Float NEW_DOOR_CHANCE{"world", "NEW_DOOR_CHANCE"};
tune::Int DOOR_WIDTH{"world", "DOOR_WIDTH"};
/// Doors keep at least this far from the corners of their slot.
tune::Int DOOR_MARGIN{"world", "DOOR_MARGIN"};
/// How likely each kind of chunk is.
/// Plazas only come square.
constexpr ChunkKind KINDS[] = {ChunkKind::Rooms, ChunkKind::Maze, ChunkKind::WaterMaze, ChunkKind::Plaza};
tune::List KIND_WEIGHTS{"world", "KIND_WEIGHTS", 4};
/// Chance a chunk is a data centre, rolled with dice of its own after its
/// kind, so other chunks come out as they would without (user: uncommon).
tune::Float DATA_CENTRE_CHANCE{"world", "DATA_CENTRE_CHANCE"};
/// Server rooms a data centre wants for each slot it covers.
tune::Int CENTRE_ROOMS_PER_SLOT{"world", "CENTRE_ROOMS_PER_SLOT"};
/// Chance a room network has a server room (user: rare).
tune::Float SERVER_ROOM_CHANCE{"world", "SERVER_ROOM_CHANCE"};
/// An airlock door shuts and locks this long after the last one through it stepped out of it.
constexpr float AIRLOCK_SHUT_TIME = 1.0f;
/// Mazes, of stone or water, come no bigger than this many slots (user).
tune::Int MAZE_SLOTS_MAX{"world", "MAZE_SLOTS_MAX"};
/// Width of the corridors that join stray areas and doors to the rest.
tune::Int JOIN_WIDTH{"world", "JOIN_WIDTH"};
/// Keys and the chunk's human start at least this far into the chunk from
/// where it was entered, as a fraction of the way to the farthest floor.
tune::Float KEY_DEPTH{"world", "KEY_DEPTH"};
/// A chunk makes one spare key for every this many locks it brings, to the
/// nearest: 1 lock gets 1 key, 2 get 3, 3 get 4, 4 get 5, 5 get 7 (user).
tune::Int LOCKS_PER_SPARE_KEY{"world", "LOCKS_PER_SPARE_KEY"};
/// Cakes per slot a chunk is made with. They never come back once eaten.
tune::Int CAKES_MIN{"world", "CAKES_MIN"}, CAKES_MAX{"world", "CAKES_MAX"};
/// Each human wakes BOTTLE_STEPS_MIN to BOTTLE_STEPS_MAX steps from a bottle
/// this full, lying on the floor.
constexpr int BOTTLE_STEPS_MIN = 2, BOTTLE_STEPS_MAX = 3;
constexpr float WAKE_BOTTLE = 0.5f;
/// Slam rooms per slot of a room network, at most.
tune::Int SLAM_ROOMS_MAX{"world", "SLAM_ROOMS_MAX"};
/// Chance a cake is rigged with darts, where a wall faces it down a straight run.
tune::Float DART_BAIT_CHANCE{"world", "DART_BAIT_CHANCE"};
tune::Int SPIKES_MIN{"world", "SPIKES_MIN"}, SPIKES_MAX{"world", "SPIKES_MAX"};
/// Spikes keep this far into the chunk, and this far from slam room gates.
tune::Int SPIKE_MARGIN{"world", "SPIKE_MARGIN"};
tune::Int SPIKE_GATE_GAP{"world", "SPIKE_GATE_GAP"};
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
tune::List TABLE_SHAPE_WEIGHTS{"world", "TABLE_SHAPE_WEIGHTS", 7};
/// How likely each table material is, in TableMaterial order.
tune::List TABLE_MATERIAL_WEIGHTS{"world", "TABLE_MATERIAL_WEIGHTS", 8};
/// Chance a table is of its chunk's house material rather than any.
tune::Float TABLE_HOUSE_CHANCE{"world", "TABLE_HOUSE_CHANCE"};

/// How likely each statue kind and material is, in enum order.
tune::List STATUE_KIND_WEIGHTS{"world", "STATUE_KIND_WEIGHTS", 10};

tune::List STATUE_MATERIAL_WEIGHTS{"world", "STATUE_MATERIAL_WEIGHTS", 4};
tune::Float PLINTH_CHANCE{"world", "PLINTH_CHANCE"};
/// Chance a room gets a statue in its middle, and the most per slot.
tune::Float STATUE_ROOM_CHANCE{"world", "STATUE_ROOM_CHANCE"};
/// Chance a room without one in its middle gets one in a niche instead,
/// and the kinds small enough for niches.
tune::Float NICHE_CHANCE{"world", "NICHE_CHANCE"};
/// A plaza's middle is a fountain this often, else its grand statue stands
/// on the floor. Smaller statues stand round it, this many per slot across.
tune::Float FOUNTAIN_CHANCE{"world", "FOUNTAIN_CHANCE"};
tune::Int PLAZA_STATUES_MIN{"world", "PLAZA_STATUES_MIN"}, PLAZA_STATUES_MAX{"world", "PLAZA_STATUES_MAX"};
/// Now and then a plaza is given over to toilets: every statue in it a
/// toilet, half as many again of them, and real ones all round its walls.
tune::Float TOILET_PLAZA_CHANCE{"world", "TOILET_PLAZA_CHANCE"};
tune::Int TOILET_PLAZA_STATUES_EXTRA{"world", "TOILET_PLAZA_STATUES_EXTRA"};
constexpr StatueKind NICHE_KINDS[] = {StatueKind::Spoon, StatueKind::Bulb, StatueKind::Mug, StatueKind::Lamp,
                                      StatueKind::Clock};

tune::Int STATUES_PER_SLOT_MAX{"world", "STATUES_PER_SLOT_MAX"};
/// Cells per side of the spawn room: 30% of the floor the whole chunk once gave.
tune::Int SPAWN_ROOM{"world", "SPAWN_ROOM"};
/// The spawn room gets a statue this often, at least this far from its middle.
tune::Float SPAWN_STATUE_CHANCE{"world", "SPAWN_STATUE_CHANCE"};
tune::Int SPAWN_STATUE_GAP{"world", "SPAWN_STATUE_GAP"};
/// Cells per side of a basin. A plaza with no fountain gets this many.
tune::Int BASIN_SIZE{"world", "BASIN_SIZE"};
tune::Int PLAZA_BASINS_MIN{"world", "PLAZA_BASINS_MIN"}, PLAZA_BASINS_MAX{"world", "PLAZA_BASINS_MAX"};
/// Washrooms a data centre has for each slot it covers. It has no other
/// water than their toilets (user).
tune::Int CENTRE_WASHROOMS_PER_SLOT{"world", "CENTRE_WASHROOMS_PER_SLOT"};
/// A statue keeps at least this many cells from any door, and from the
/// cell it is told to keep clear.
tune::Int STATUE_DOOR_GAP{"world", "STATUE_DOOR_GAP"};
tune::Int STATUE_CLEAR_GAP{"world", "STATUE_CLEAR_GAP"};
/// How far from the centre a statue may be nudged to fit.
tune::Int STATUE_NUDGE{"world", "STATUE_NUDGE"};

/// Chunk footprints; how likely each is is in SHAPE_WEIGHTS, in this order.
const std::vector<Footprint> SHAPES = {boxFootprint({1, 1}), boxFootprint({2, 1}), boxFootprint({1, 2}),
                                       boxFootprint({2, 2}), boxFootprint({3, 1}), boxFootprint({1, 3}),
                                       boxFootprint({4, 1}), boxFootprint({1, 4}), boxFootprint({3, 3})};
tune::List SHAPE_WEIGHTS{"world", "SHAPE_WEIGHTS", 9};

TableMaterial randomMaterial(Rng& rng) { return TableMaterial(rng.weighted(TABLE_MATERIAL_WEIGHTS)); }

} // namespace

World::World(uint64_t seed) : rng_(seed), seed_(seed) {}

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
    const Glowing unlit = glowing();
    c.paint = gen::randomFloor(rng_);
    gen::squareRoom(c, SPAWN_ROOM);
    Vec2i centre{c.w / 2, c.h / 2};
    const int lo = (c.w - SPAWN_ROOM) / 2, hi = lo + SPAWN_ROOM - 1;
    placeDoors(c, centre, 0.0f, 1, 1);
    if (rng_.chance(SPAWN_STATUE_CHANCE))
        for (int t = 0; t < 40; t++) {
            Vec2i at{rng_.range(lo, hi), rng_.range(lo, hi)};
            if (cheb(at, centre) >= SPAWN_STATUE_GAP && placeStatue(c, at, centre)) break;
        }
    std::vector<Vec2i> spots = deepSpots(c, centre);
    TableMaterial house = randomMaterial(rng_);
    size_t next = 0;
    if (auto s = tableSpot(c, spots, next, house, true)) keys_.insert(*s);
    if (auto s = tableSpot(c, spots, next, house)) cakes_.insert(*s);
    lightUp(c, unlit);
    Human& h = addHuman(look, c.origin() + centre, seed, name);
    layBottle(h);
    return h;
}

Human& World::addHuman(const HumanLook& look, Vec2i cell) { return addHuman(look, cell, rng_.next()); }

// Walked out from where it lies, so the bottle is never behind a wall or a
// table from it; any free floor that far, picked by dice of its own, so the
// rest of the mall comes out as it would without it.
void World::layBottle(Human& h) {
    std::unordered_map<Vec2i, int, Vec2iHash> dist{{h.cell, 0}};
    std::deque<Vec2i> open{h.cell};
    std::vector<Vec2i> free;
    while (!open.empty()) {
        Vec2i at = open.front();
        open.pop_front();
        int d = dist[at];
        if (d >= BOTTLE_STEPS_MIN && !traps_.count(at) && !piles_.count(at) && !occupant(at) &&
            !behindStatues_.count(at))
            free.push_back(at);
        if (d == BOTTLE_STEPS_MAX) continue;
        for (Vec2i n : {at + Vec2i{1, 0}, at + Vec2i{-1, 0}, at + Vec2i{0, 1}, at + Vec2i{0, -1}})
            if (tile(n) == Tile::Floor && dist.emplace(n, d + 1).second) open.push_back(n);
    }
    if (free.empty()) return;
    Rng pick(uint64_t(h.id) * 0x9E3779B97F4A7C15ull ^ uint64_t(uint32_t(h.cell.x)) << 32 ^ uint32_t(h.cell.y));
    putDown(free[size_t(pick.range(0, int(free.size()) - 1))], 0, {Bottle{WAKE_BOTTLE}});
    h.observe(*this);
}

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
// key, and so does each slam room, and a third more are added as spares, so
// there are a few more keys than locked doors and gates that can be.
void World::generate(Vec2i slot, Vec2i entry) {
    // Each slot of each footprint in turn is tried on the one entered.
    std::vector<std::pair<Vec2i, const Footprint*>> options;
    std::vector<float> weights;
    for (size_t i = 0; i < SHAPES.size(); i++)
        for (Vec2i on : SHAPES[i]) {
            Vec2i origin = slot - on;
            bool free = std::none_of(SHAPES[i].begin(), SHAPES[i].end(),
                                     [&](Vec2i p) { return generated(origin + p); });
            if (!free) continue;
            options.push_back({origin, &SHAPES[i]});
            weights.push_back(SHAPE_WEIGHTS.values()[i]);
        }
    auto [origin, parts] = options[size_t(rng_.weighted(weights))];

    std::vector<float> kindWeights = KIND_WEIGHTS;
    for (size_t i = 0; i < kindWeights.size(); i++)
        if (!grows(KINDS[i], *parts)) kindWeights[i] = 0.0f;
    grow(KINDS[rng_.weighted(kindWeights)], origin, *parts, entry, true);
}

bool World::grows(ChunkKind kind, const Footprint& parts) {
    const Vec2i box = footprintBox(parts);
    if (kind == ChunkKind::Plaza) return box.x == box.y && int(parts.size()) == box.x * box.y;
    if (kind == ChunkKind::Maze || kind == ChunkKind::WaterMaze) return int(parts.size()) <= MAZE_SLOTS_MAX;
    return kind != ChunkKind::Spawn;
}

Vec2i World::spawnEntry() const {
    const auto& [key, d] = *std::min_element(doors_.begin(), doors_.end(), [](const auto& a, const auto& b) {
        return std::tuple(a.first.slot.y, a.first.slot.x, int(a.first.side)) <
               std::tuple(b.first.slot.y, b.first.slot.x, int(b.first.side));
    });
    const Vec2i into = generated(d.slot) ? d.other() : d.slot;
    return d.cellsIn(into)[size_t(d.width / 2)];
}

const Chunk* World::growPastSpawn(ChunkKind kind, const Footprint& parts) {
    if (kind == ChunkKind::Spawn) return chunks_.front().get();
    if (!grows(kind, parts) || chunks_.size() != 1) return nullptr;
    const Vec2i entry = spawnEntry(), into = slotOf(entry);
    for (Vec2i on : parts) {
        const Vec2i origin = into - on;
        if (std::any_of(parts.begin(), parts.end(), [&](Vec2i p) { return generated(origin + p); })) continue;
        grow(kind, origin, parts, entry, false);
        return chunks_.back().get();
    }
    return nullptr;
}

// The data centre's dice are rolled even when the kind is given, so the
// chunk comes out as it would have had the game rolled that kind.
void World::grow(ChunkKind kind, Vec2i origin, const Footprint& parts, Vec2i entry, bool mayCentre) {
    Rng serverDice(seed_ ^ uint64_t(chunks_.size() + 1) * 0xD1B54A32D192ED03ull);
    if (serverDice.chance(DATA_CENTRE_CHANCE) && mayCentre) kind = ChunkKind::DataCentre;
    Chunk& c = addChunk(kind, origin, parts);
    const Glowing unlit = glowing();
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
    else if (c.kind == ChunkKind::DataCentre)
        gen::dataCentre(c, rng_);
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
    // Server rooms after the slam rooms: nothing digs after either.
    int servers = 0;
    if (c.kind == ChunkKind::DataCentre) {
        // Its washrooms first, so the server rooms fit round them.
        std::vector<gen::RoomBox> washrooms = placeWashrooms(c, serverDice, CENTRE_WASHROOMS_PER_SLOT * slotCount);
        servers = placeServerRooms(c, serverDice, CENTRE_ROOMS_PER_SLOT * slotCount, washrooms);
    }
    else if (c.kind == ChunkKind::Rooms && serverDice.chance(SERVER_ROOM_CHANCE))
        servers = placeServerRooms(c, serverDice, 1);
    // A plaza without a fountain has basins to drink from instead.
    if (c.kind == ChunkKind::Plaza && plaza.islandSize.x == 0) {
        int basins = rng_.range(PLAZA_BASINS_MIN, PLAZA_BASINS_MAX);
        for (Vec2i s : deepSpots(c, entry - c.origin()))
            if (basins > 0 && placeBasin(c, s)) basins--;
    }
    std::vector<Vec2i> spots = deepSpots(c, entry - c.origin());
    // Nothing lies in a slam room but its bait, nor in a server room or its airlock.
    std::erase_if(spots, [&](Vec2i s) {
        return std::any_of(slams.begin(), slams.end(),
                           [&](const gen::SlamRoom& r) { return r.contains(s - c.origin()); }) ||
               std::any_of(serverRooms_.end() - servers, serverRooms_.end(),
                           [&](const ServerRoom& r) { return r.encloses(s); });
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
    // A server room brings one key, though getting in takes two (user).
    int keys = fresh + int(slams.size()) + servers;
    keys += (keys + LOCKS_PER_SPARE_KEY / 2) / LOCKS_PER_SPARE_KEY;
    for (int i = 0; i < keys; i++)
        if (auto s = tableSpot(c, spots, next, house, true)) keys_.insert(*s);
    // Tables took some spots, and their room round them is best left clear.
    Human* waker = nullptr;
    for (; next < spots.size(); next++) {
        Vec2i s = spots[next];
        if (tile(s) != Tile::Floor || occupant(s)) continue;
        Rng looks(rng_.next());
        waker = &addHuman(HumanLook::random(looks), s);
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
    // Last but for the bottle, so no key or cake lies where one stands.
    placeMachines(c, slams);
    lightUp(c, unlit);
    // Last, so its bottle lies clear of every table and trap.
    if (waker) layBottle(*waker);
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

bool World::unlock(Vec2i cell) {
    if (auto it = airlocks_.find(cell); it != airlocks_.end()) {
        if (tile(cell) != Tile::DoorLocked) return false;
        setTile(cell, Tile::DoorOpen);
        it->second.open = AIRLOCK_SHUT_TIME;
        return true;
    }
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

bool World::pushOpen(Vec2i door, Vec2i from) {
    auto it = airlocks_.find(door);
    if (it == airlocks_.end() || from != door + it->second.in) return false;
    setTile(door, Tile::DoorOpen);
    it->second.open = AIRLOCK_SHUT_TIME;
    return true;
}

const ServerRoom* World::serverRoom(Vec2i cell) const {
    for (const ServerRoom& r : serverRooms_)
        if (r.contains(cell)) return &r;
    return nullptr;
}

// Held open while anyone stands in it, it shuts a moment after they step out.
void World::updateAirlocks(float dt) {
    for (auto& [cell, d] : airlocks_) {
        if (tile(cell) != Tile::DoorOpen) continue;
        if (occupant(cell)) d.open = AIRLOCK_SHUT_TIME;
        else if ((d.open -= dt) <= 0.0f) setTile(cell, Tile::DoorLocked);
    }
}

// A door that opens lets light through, and one that shuts stops it.
void World::setTile(Vec2i cell, Tile t) {
    Chunk* c = chunkAt(slotOf(cell));
    if (!c) return;
    bool was = opaque(c->get(cell - c->origin()));
    c->set(cell - c->origin(), t);
    if (opaque(t) != was) relightAround(cell, cell);
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

void World::updateMachines(float dt) {
    for (Machine& m : machines_) {
        if (m.stock >= MACHINE_SLOTS || (m.restock -= dt) > 0.0f) continue;
        m.stock++;
        m.restock = MACHINE_DAY;
    }
}

// The bottom of what it holds goes first, so the rest stays shelved from the top.
bool World::vend(Vec2i cell) {
    auto it = machineCells_.find(cell);
    if (it == machineCells_.end() || machines_[size_t(it->second)].stock <= 0) return false;
    machines_[size_t(it->second)].stock--;
    return true;
}

// Humans in the order they came, feces in the order they were left, then
// water by its cell, so the same world gathers them the same way.
void World::gatherSmells() {
    smells_.clear();
    for (const auto& h : humans_)
        if (float s = h->smellStrength(); s > 0.0f)
            smells_.push_back({h->dead() ? SmellKind::Body : SmellKind::Human, h->cell, s, h->id});
    for (const Dropping& d : mess_.all()) {
        if (d.kind != MessKind::Feces) continue;
        float fresh = Mess::freshness(d, time_);
        if (fresh > FECES_SMELL_DRY)
            smells_.push_back({SmellKind::Feces, d.cell, std::min(1.0f, d.size * (fresh - FECES_SMELL_DRY) / (1.0f - FECES_SMELL_DRY)), d.who});
    }
    std::vector<Smell> water;
    for (const auto& [c, f] : mess_.fouled())
        if (f >= WATER_SMELL_FOUL && pooled(tile(c)))
            water.push_back({SmellKind::Water, c, std::min(1.0f, WATER_SMELL_MIN + (f - WATER_SMELL_FOUL) / WATER_SMELL_FOUL), -1});
    std::sort(water.begin(), water.end(), [](const Smell& a, const Smell& b) {
        return a.cell.y != b.cell.y ? a.cell.y < b.cell.y : a.cell.x < b.cell.x;
    });
    smells_.insert(smells_.end(), water.begin(), water.end());
}

// By index: a human stepping into an empty slot adds a chunk and a human.
void World::update(float dt) {
    time_ += dt;
    for (auto& c : chunks_) c->simulate(dt);
    updateTraps(dt);
    updateMachines(dt);
    updateAirlocks(dt);
    blood_.update(dt);
    for (const auto& [c, f] : blood_.takeFoul()) mess_.foulWater(c, f, true);
    mess_.update(dt, [&](Vec2i c) { return pooled(tile(c)); });
    if ((smellIn_ -= dt) <= 0.0f) {
        smellIn_ += SMELL_EVERY;
        gatherSmells();
    }
    for (size_t i = 0; i < humans_.size(); i++) humans_[i]->update(*this, dt);
}
