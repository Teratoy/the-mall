// The world generator editor: the knobs of data/worldgen.txt on the left,
// a chunk grown with them on the right, grown again on the same seed each
// time a knob moves, so what changes is that knob's doing, not the dice.
// See AI/worldgen_editor_plan.md.
//
// Usage: build/genview [--drop]   (or: make genview, or ./genview.sh from the host)
//   --drop  drops humans on the chunk as soon as it is grown, as H does
//
// Knobs: drag a slider, or double-click its value to type one (Enter keeps
// it, Esc drops it). Left and Right step the knob under the mouse. A knob
// changed from the file shows yellow; right-click it to put it back.
// Click a section's name to fold it. Ctrl+Z undoes a change.
// Top bar (click for the next, right-click for the one before):
//   kind       rolled as the game does, or spawn, rooms, maze, water maze,
//              plaza, data centre
//   footprint  1x1 to 3x3, L, T, S; turn and mirror
//   seed       < and > step it, R rolls a new one
//   view (V)   the game's art or a flat map
//   mode       one chunk; before and after (C): the file's values beside
//              the changed ones; or nine seeds at once (G), click one to
//              take its seed
//   Save (S)   writes the changed values into data/worldgen.txt
// The view pans with a drag and zooms with the wheel; F fits the chunk.
// Second row, overlays, any together (or keys 1 to 5):
//   1 doors and keys   open doors orange, locked red; keys yellow, cakes
//                      pink, where it was entered white
//   2 water sides      each side of the water its own colour
//   3 out of reach     floor that can't be walked to from where it was
//                      entered, locks as open: orange if swimming gets
//                      there, red if nothing does
//   4 traps            spikes red; dart plates pink, their shooters and
//                      line cyan; slam plates pink, their gates red
//   5 deep spots       where keys and the chunk's human may start
//   H drop humans      runs the sim on the chunk shown (both, before and
//                      after) with this many humans dropped on its floor
//                      for this long (click the count and the minutes to
//                      change them), the world round it growing as they
//                      wander, and paints a heatmap of where they walked,
//                      slept, got stuck, bled or died (J picks which). It
//                      runs while the window stays live, and a change to
//                      the chunk drops it.
//   T readout          what the chunk has: floor and water, the longest
//                      walk, keys, cakes, traps, machines... In before/after
//                      what differs shows yellow; with nine seeds, the
//                      least, the mean and the most over them. Whatever
//                      genprobe would fail it for shows in red on the
//                      chunk, readout or not.
//
// The file is watched: changed on disk, by hand or by Claude in a chat, it
// is read again within a second and the chunk regrown, keeping knobs
// changed here but not yet saved. After each regrow, once things settle,
// the window is saved to build/genview.png and what it shows to
// build/genview.txt (seed, kind, footprint, view, mode, unsaved knobs), so
// Claude can see what the user sees. That file is read back at start, so a
// rebuild opens on the same chunk.
#include "../tests/probe.h"
#include "chunk_checks.h"
#include "core/rng.h"
#include "human/human.h"
#include "human/human_look.h"
#include "render/floor_tiles.h"
#include "render/statue_art.h"
#include "render/world_view.h"
#include "world/gen.h"
#include "world/gen_tuning.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <deque>
#include <memory>
#include <raylib.h>
#include <rlgl.h>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// Where what the window shows is saved, for Claude to look at.
constexpr const char* SHOT_PATH = "build/genview.png";
constexpr const char* STATE_PATH = "build/genview.txt";
/// The window saves itself this long after the last change.
constexpr double SHOT_DELAY = 0.6;
/// How often the file is looked at for changes, in seconds.
constexpr double WATCH_EVERY = 0.5;

constexpr int FONT = 20;
constexpr int ROW = 26;
/// The top bar: a row of what is grown and how it is shown, then a row
/// of overlays.
constexpr int BAR_ROW = 36, BAR_H = 2 * BAR_ROW;
constexpr int STATUS_H = 30;
constexpr int PAD = 8;
/// The knob panel's slider and value columns; the name column is as wide
/// as the longest name.
constexpr int SLIDER_W = 150, VALUE_W = 90;

const Color BG{22, 20, 26, 255}, PANEL{32, 29, 38, 255}, LINE{52, 48, 60, 255}, INK{222, 216, 200, 255},
    DIM{140, 134, 124, 255}, CHANGED{250, 206, 80, 255}, TRACK{66, 61, 76, 255}, FILL{126, 112, 160, 255},
    HOT{60, 55, 72, 255}, BAD{230, 90, 70, 255};

struct KindPick {
    const char* name;
    ChunkKind kind;
    bool rolled;
};
const KindPick KINDS[] = {{"rolled", ChunkKind::Rooms, true},     {"spawn", ChunkKind::Spawn, false},
                          {"rooms", ChunkKind::Rooms, false},     {"maze", ChunkKind::Maze, false},
                          {"water maze", ChunkKind::WaterMaze, false}, {"plaza", ChunkKind::Plaza, false},
                          {"data centre", ChunkKind::DataCentre, false}};
constexpr int KIND_COUNT = int(sizeof KINDS / sizeof KINDS[0]);

const char* kindName(ChunkKind k) {
    for (const KindPick& p : KINDS)
        if (!p.rolled && p.kind == k) return p.name;
    return "?";
}

struct Shape {
    const char* name;
    Footprint parts;
};
/// The footprints the game grows, then the odd ones it may one day.
const std::vector<Shape> SHAPES = {
    {"1x1", boxFootprint({1, 1})}, {"2x1", boxFootprint({2, 1})}, {"1x2", boxFootprint({1, 2})},
    {"2x2", boxFootprint({2, 2})}, {"3x1", boxFootprint({3, 1})}, {"1x3", boxFootprint({1, 3})},
    {"4x1", boxFootprint({4, 1})}, {"1x4", boxFootprint({1, 4})}, {"3x3", boxFootprint({3, 3})},
    {"L3", {{0, 0}, {0, 1}, {1, 1}}},  {"L4", {{0, 0}, {0, 1}, {0, 2}, {1, 2}}},
    {"T4", {{0, 0}, {1, 0}, {2, 0}, {1, 1}}}, {"S4", {{1, 0}, {2, 0}, {0, 1}, {1, 1}}},
};

/// The footprint turned a quarter clockwise `turns` times, mirrored first
/// if asked, and slid back so its box starts at {0, 0}.
Footprint turned(Footprint parts, int turns, bool mirror) {
    for (Vec2i& p : parts)
        if (mirror) p.x = -p.x;
    for (int t = 0; t < turns; t++)
        for (Vec2i& p : parts) p = {-p.y, p.x};
    Vec2i lo{1 << 20, 1 << 20};
    for (Vec2i p : parts) lo = {std::min(lo.x, p.x), std::min(lo.y, p.y)};
    for (Vec2i& p : parts) p = p - lo;
    std::sort(parts.begin(), parts.end(), [](Vec2i a, Vec2i b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return parts;
}

/// A number as short as it can be written and still read back the same.
std::string shortest(float v, bool whole) {
    char buf[32];
    auto end = std::to_chars(buf, buf + sizeof buf, whole ? std::round(v) : v).ptr;
    return std::string(buf, end);
}

void text(const std::string& s, int x, int y, Color c) { DrawText(s.c_str(), x, y, FONT, c); }
int width(const std::string& s) { return MeasureText(s.c_str(), FONT); }
bool over(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

/// A flat map of the chunk, a pixel a cell.
Texture2D flatMap(const World& w, const Chunk& c) {
    Image img = GenImageColor(c.w, c.h, BLACK);
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            Color col = BLACK;
            switch (c.get({x, y})) {
            case Tile::Floor: col = {200, 190, 170, 255}; break;
            case Tile::Wall: col = {70, 60, 80, 255}; break;
            case Tile::Water: col = {60, 120, 200, 255}; break;
            case Tile::DoorOpen: col = {230, 160, 40, 255}; break;
            case Tile::DoorLocked: col = {200, 60, 40, 255}; break;
            case Tile::Statue: col = {250, 250, 250, 255}; break;
            case Tile::Table: col = {140, 90, 50, 255}; break;
            case Tile::Basin: col = {60, 120, 200, 255}; break;
            case Tile::Toilet: col = {235, 235, 210, 255}; break;
            case Tile::Vending: col = {120, 220, 200, 255}; break;
            case Tile::Rack: col = {40, 200, 80, 255}; break;
            case Tile::Console: col = {160, 230, 120, 255}; break;
            case Tile::Dispenser: col = {110, 200, 250, 255}; break;
            default: break;
            }
            Vec2i at = c.origin() + Vec2i{x, y};
            if (w.hasCake(at)) col = {250, 80, 160, 255};
            if (w.hasKey(at)) col = {250, 230, 0, 255};
            if (!c.owns({x, y})) col = BLACK;
            ImageDrawPixel(&img, x, y, col);
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

enum Overlay { DOORS, SIDES, REACH, TRAPS, DEEP, OVERLAYS };
const char* const OVERLAY_NAMES[] = {"Doors & keys", "Water sides", "Out of reach", "Traps", "Deep spots"};
/// How see-through an overlay is over the chunk.
constexpr unsigned char TINT = 150;

/// An overlay a pixel a cell, see-through but where painted.
struct Paint {
    Image img;
    const Chunk& c;
    Paint(const Chunk& chunk) : img(GenImageColor(chunk.w, chunk.h, BLANK)), c(chunk) {}
    /// A world cell, if it is the chunk's.
    void cell(Vec2i at, Color col) {
        Vec2i l = at - c.origin();
        if (c.owns(l)) ImageDrawPixel(&img, l.x, l.y, col);
    }
    /// A ring round a world cell, and the cell.
    void ring(Vec2i at, Color col) {
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) cell(at + Vec2i{x, y}, col);
    }
    Texture2D done() {
        Texture2D t = LoadTextureFromImage(img);
        UnloadImage(img);
        return t;
    }
};

/// Steps from start to every cell walkable from it, locks as open; -1 where
/// it can't get. Local cells.
std::vector<int> steps(const Chunk& c, Vec2i start) {
    std::vector<int> d(size_t(c.w * c.h), -1);
    std::deque<Vec2i> open{start};
    d[size_t(start.y * c.w + start.x)] = 0;
    while (!open.empty()) {
        Vec2i p = open.front();
        open.pop_front();
        for (Vec2i dir : DIRS4) {
            Vec2i n = p + dir;
            if (!c.owns(n) || d[size_t(n.y * c.w + n.x)] >= 0) continue;
            Tile k = c.get(n);
            if (!passable(k) && k != Tile::DoorLocked) continue;
            d[size_t(n.y * c.w + n.x)] = d[size_t(p.y * c.w + p.x)] + 1;
            open.push_back(n);
        }
    }
    return d;
}

/// The overlays of a chunk grown in w, entered at entry.
void overlays(World& w, const Chunk& c, Vec2i entry, Texture2D (&out)[OVERLAYS]) {
    const Color yellow{250, 230, 0, 255}, pink{250, 80, 160, 255}, red{235, 50, 40, 255}, orange{250, 150, 30, 255},
        cyan{60, 230, 240, 255}, white{255, 255, 255, 255};
    {
        Paint p(c);
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                Tile k = c.get({x, y});
                if (k == Tile::DoorOpen) p.cell(c.origin() + Vec2i{x, y}, orange);
                if (k == Tile::DoorLocked) p.cell(c.origin() + Vec2i{x, y}, red);
            }
        for (Vec2i k : w.cakes()) p.ring(k, pink);
        for (Vec2i k : w.keys()) p.ring(k, yellow);
        p.ring(entry, white);
        out[DOORS] = p.done();
    }
    {
        Paint p(c);
        const std::vector<int> side = gen::sides(c);
        const Color hues[] = {{80, 200, 120, TINT}, {200, 90, 220, TINT}, {240, 200, 60, TINT}, {70, 160, 250, TINT},
                              {250, 110, 90, TINT}, {120, 240, 230, TINT}, {180, 180, 250, TINT}, {250, 160, 200, TINT}};
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                int l = side[size_t(y * c.w + x)];
                if (l >= 0 && c.get({x, y}) != Tile::Wall) p.cell(c.origin() + Vec2i{x, y}, hues[l % 8]);
            }
        out[SIDES] = p.done();
    }
    {
        Paint p(c);
        const std::vector<bool> walked = reach(c, entry, false), swum = reach(c, entry, true);
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) {
                size_t i = size_t(y * c.w + x);
                if (!passable(c.get({x, y})) || walked[i]) continue;
                p.cell(c.origin() + Vec2i{x, y}, swum[i] ? Color{250, 150, 30, 200} : Color{235, 50, 40, 220});
            }
        out[REACH] = p.done();
    }
    {
        Paint p(c);
        for (const auto& [at, t] : w.traps()) {
            if (w.chunkIdAt(at) != c.id) continue;
            if (t.kind == TrapKind::Spikes) {
                p.ring(at, red);
            } else if (t.kind == TrapKind::Darts) {
                for (Vec2i q = t.from; q != at && cheb(q, t.from) <= 32; q = q + t.dir) p.cell(q, Fade(cyan, 0.6f));
                p.ring(t.from, cyan);
                p.ring(at, pink);
            } else {
                p.ring(t.from, red);
                p.ring(at, pink);
            }
        }
        out[TRAPS] = p.done();
    }
    {
        Paint p(c);
        for (Vec2i s : GenViewProbe::deepSpots(w, c, entry)) p.cell(s, Color{90, 250, 120, 200});
        p.ring(entry, white);
        out[DEEP] = p.done();
    }
}

/// What a chunk has, a row each, and what genprobe would fail it for.
struct Readout {
    std::vector<std::pair<std::string, float>> rows;
    std::string fault;
};

Readout readout(const World& w, const Chunk& c, Vec2i entry) {
    Readout r;
    Tally t;
    r.fault = check(w, c, entry, t);
    int owned = 0, floor = 0, water = 0, basins = 0, toilets = 0;
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++) {
            if (!c.owns({x, y}) || !c.interior({x, y})) continue;
            owned++;
            Tile k = c.get({x, y});
            floor += k == Tile::Floor;
            water += k == Tile::Water;
            basins += k == Tile::Basin;
            toilets += k == Tile::Toilet;
        }
    // The longest walk between two of its doors, and from the entry to the
    // farthest floor, locks as open.
    std::vector<Vec2i> doors;
    for (const auto& [key, d] : SoakProbe::doors(w))
        for (Vec2i s : {d.slot, d.other()})
            if (c.covers(s)) doors.push_back(d.cellsIn(s)[size_t(d.width / 2)] - c.origin());
    int longest = 0, farthest = 0;
    for (Vec2i a : doors) {
        std::vector<int> d = steps(c, a);
        for (Vec2i b : doors) longest = std::max(longest, d[size_t(b.y * c.w + b.x)]);
    }
    for (int d : steps(c, entry - c.origin())) farthest = std::max(farthest, d);
    auto in = [&](Vec2i at) { return w.chunkIdAt(at) == c.id; };
    int tables = 0, statues = 0, machines = 0, servers = 0, dispensers = 0, spikes = 0, darts = 0, slams = 0;
    for (const Table& x : w.tables()) tables += in(x.at);
    for (const Statue& x : w.statues()) statues += in(x.at);
    for (const Machine& x : w.machines()) machines += in(x.at);
    for (const ServerRoom& x : w.serverRooms()) servers += in({x.x, x.y});
    for (const Dispenser& x : w.dispensers()) dispensers += in(x.at);
    for (const auto& [at, x] : w.traps())
        if (in(at)) (x.kind == TrapKind::Spikes ? spikes : x.kind == TrapKind::Darts ? darts : slams)++;
    r.rows = {{"floor %", owned ? 100.0f * floor / owned : 0},
              {"water %", owned ? 100.0f * water / owned : 0},
              {"longest door to door", float(longest)},
              {"farthest from entry", float(farthest)},
              {"doors", float(doors.size())},
              {"new doors", float(t.doors)},
              {"keys", float(t.keys)},
              {"cakes", float(t.cakes)},
              {"cakes over water", float(t.overWater)},
              {"tables", float(tables)},
              {"statues", float(statues)},
              {"machines", float(machines)},
              {"toilets", float(toilets)},
              {"basin cells", float(basins)},
              {"server rooms", float(servers)},
              {"dispensers", float(dispensers)},
              {"spikes", float(spikes)},
              {"dart plates", float(darts)},
              {"slam rooms", float(slams)}};
    return r;
}

/// A number for the readout: whole, or to one place.
std::string shown(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, v == std::floor(v) ? "%.0f" : "%.1f", double(v));
    return buf;
}

enum Heat { WALKED, SLEPT, STUCK, BLED, DIED, HEATS };
const char* const HEAT_NAMES[] = {"walked", "slept", "stuck", "bled", "died"};
/// What can be picked for a run: humans dropped, and minutes.
const int CROWDS[] = {0, 2, 4, 6, 8, 12, 16, 24};
const int MINUTES[] = {1, 2, 5, 10, 15, 30};
/// The sim's step, as the soak's.
constexpr float SIM_DT = 1.0f / 30.0f;
/// Seconds of each frame spent running the sim, so the window stays live.
constexpr double SIM_BUDGET = 0.014;
/// The heatmap is drawn again this often while it runs.
constexpr double HEAT_EVERY = 0.25;
/// Rethinks in a row that count a walk as stuck.
constexpr int STUCK_RETHINKS = 2;

/// Humans dropped in a chunk and the sim run on its world, with the time
/// spent in each of its cells walking, asleep, stuck or bleeding, and where
/// they died.
struct Run {
    std::vector<float> heat[HEATS];
    float t = 0, length = 0;
    /// Seconds of the window's time spent running it.
    double wall = 0;
    int dropped = 0, died = 0;
    /// Deaths in the chunk by what the life log says killed them: thirst,
    /// hunger, bleeding, anything else.
    int thirst = 0, starved = 0, bled = 0, other = 0;
    std::vector<bool> dead;
    bool drawn = false;
    double drawnAt = 0;
    Texture2D tex{};
    int texHeat = -1;

    ~Run() {
        if (tex.id) UnloadTexture(tex);
    }
    bool done() const { return t >= length; }
};

/// One chunk grown, with its world, and drawn both ways.
struct Grown {
    std::unique_ptr<World> world;
    const Chunk* chunk = nullptr;
    RenderTexture2D art{};
    Texture2D map{};
    Texture2D overlay[OVERLAYS]{};
    Readout readout;
    std::unique_ptr<Run> run;
    /// Where it was entered: the cell past the spawn's door.
    Vec2i entry{};
    uint64_t seed = 0;
    std::string label, note;

    Grown() = default;
    Grown(const Grown&) = delete;
    Grown& operator=(const Grown&) = delete;
    ~Grown() {
        if (art.id) UnloadRenderTexture(art);
        if (map.id) UnloadTexture(map);
        for (Texture2D& t : overlay)
            if (t.id) UnloadTexture(t);
    }
    /// The chunk's size in world pixels.
    Vector2 size() const { return chunk ? Vector2{float(chunk->w * CELL_PX), float(chunk->h * CELL_PX)} : Vector2{}; }
};

/// Drops humans on free floor of the chunk, the dice its seed's own, each
/// waking beside a bottle as every human does, and starts the run.
void startRun(Grown& g, int crowd, int minutes) {
    World& w = *g.world;
    const Chunk& c = *g.chunk;
    auto r = std::make_unique<Run>();
    for (auto& h : r->heat) h.assign(size_t(c.w * c.h), 0.0f);
    r->length = float(minutes * 60);
    Rng rng{g.seed ^ 0xA24BAED4963EE407ull};
    for (int tries = 0; r->dropped < crowd && tries < 5000; tries++) {
        Vec2i l{rng.range(1, c.w - 2), rng.range(1, c.h - 2)};
        Vec2i at = c.origin() + l;
        if (!c.owns(l) || w.tile(at) != Tile::Floor || w.occupant(at) || w.table(at) || w.statue(at)) continue;
        GenViewProbe::wake(w, HumanLook::random(rng), at, rng.next());
        r->dropped++;
    }
    r->dead.assign(w.humans().size(), false);
    for (const auto& h : w.humans()) r->dead[size_t(h->id)] = h->dead();
    g.run = std::move(r);
}

/// One step of the sim, its time added to the cells of the chunk.
void stepRun(Grown& g) {
    World& w = *g.world;
    const Chunk& c = *g.chunk;
    Run& r = *g.run;
    w.update(SIM_DT);
    r.t += SIM_DT;
    if (r.dead.size() < w.humans().size()) r.dead.resize(w.humans().size(), false);
    for (const auto& hp : w.humans()) {
        const Human& h = *hp;
        Vec2i l = h.cell - c.origin();
        if (!c.owns(l)) continue;
        size_t i = size_t(l.y * c.w + l.x);
        if (h.dead()) {
            if (!r.dead[size_t(h.id)]) {
                r.dead[size_t(h.id)] = true;
                r.heat[DIED][i] += 1.0f;
                r.died++;
                bool thirst = false, starve = false, bled = false;
                for (const auto& e : h.log.entries()) {
                    thirst |= e.what == "DIED OF THIRST";
                    starve |= e.what == "STARVED TO DEATH";
                    bled |= e.what == "BLED TO DEATH";
                }
                (thirst ? r.thirst : starve ? r.starved : bled ? r.bled : r.other)++;
            }
            continue;
        }
        r.heat[WALKED][i] += SIM_DT;
        if (h.state == Human::State::Sleeping) r.heat[SLEPT][i] += SIM_DT;
        if (!h.wounds.empty()) r.heat[BLED][i] += SIM_DT;
        bool blocked = h.state == Human::State::Walking && !h.moving && !SoakProbe::path(h).empty();
        const Human* by = blocked ? w.occupant(SoakProbe::path(h).front()) : nullptr;
        if ((by && by != &h) || SoakProbe::stuck(h) >= STUCK_RETHINKS) r.heat[STUCK][i] += SIM_DT;
    }
}

/// The heatmap a pixel a cell: from clear through yellow to red, by the
/// square root of each cell's share of the most, so the faint shows; where
/// they died, a ring.
Texture2D heatMap(const Run& r, const Chunk& c, int heat) {
    Image img = GenImageColor(c.w, c.h, BLANK);
    const std::vector<float>& v = r.heat[heat];
    float most = 0;
    for (float x : v) most = std::max(most, x);
    for (int y = 0; y < c.h && most > 0; y++)
        for (int x = 0; x < c.w; x++) {
            float f = v[size_t(y * c.w + x)];
            if (f <= 0) continue;
            if (heat == DIED) {
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (c.owns({x + dx, y + dy})) ImageDrawPixel(&img, x + dx, y + dy, Color{250, 40, 200, 255});
                continue;
            }
            float k = std::sqrt(f / most);
            ImageDrawPixel(&img, x, y,
                           Color{250, (unsigned char)(230 - 200 * k), 40, (unsigned char)(90 + 165 * k)});
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

class GenView {
public:
    GenView() {
        for (const tune::Knob& k : tune::all()) file_.push_back(k.values);
        cur_ = file_;
        folded_.assign(sections().size(), false);
        for (const tune::Knob& k : tune::all()) nameW_ = std::max(nameW_, width(k.name) + 4 * PAD);
        panelW_ = nameW_ + SLIDER_W + VALUE_W;
        readState();
        watched_ = stamp();
    }

    void frame() {
        watch();
        if (dirty_) regrow();
        runs();
        keys();
        BeginDrawing();
        ClearBackground(BG);
        drawView();
        drawReadouts();
        drawPanel();
        drawBar();
        drawStatus();
        if (shotAt_ > 0 && GetTime() >= shotAt_ && !dragging()) {
            // What is still batched is drawn first, or the last of it is missing.
            rlDrawRenderBatchActive();
            Image img = LoadImageFromScreen();
            ExportImage(img, SHOT_PATH);
            UnloadImage(img);
            writeState();
            shotAt_ = 0;
        }
        EndDrawing();
    }

    void close() { writeState(); }
    /// Drops humans as soon as the chunk is grown (--drop).
    void dropAtStart() { rerun_ = true; }

private:
    enum class Mode { Single, Compare, Grid };

    std::vector<std::vector<float>> file_, cur_;
    std::vector<std::pair<size_t, std::vector<float>>> undo_;
    std::vector<bool> folded_;
    std::vector<std::unique_ptr<Grown>> panes_;
    FloorTiles floorTiles_;
    StatueArt statueArt_;
    uint64_t seed_ = 1;
    int kind_ = 2, shape_ = 0, turns_ = 0;
    bool mirror_ = false, map_ = false, dirty_ = true, readout_ = false;
    bool shown_[OVERLAYS]{};
    /// What a run drops and for how long, as places in CROWDS and MINUTES,
    /// and which heatmap shows.
    int crowd_ = 3, minutes_ = 2, heat_ = WALKED;
    /// The top of the bar row being drawn.
    int barY_ = 0;
    Mode mode_ = Mode::Single;
    /// The view's centre, in the chunk's own world pixels, and screen
    /// pixels per world pixel; fitted again when the chunk's size changes.
    Vector2 centre_{};
    float scale_ = 0.0f;
    Vector2 fittedFor_{};
    float scroll_ = 0.0f;
    int nameW_ = 0, panelW_ = 0;
    int dragKnob_ = -1, dragValue_ = 0;
    bool panning_ = false;
    int editKnob_ = -1, editValue_ = 0;
    /// A run asked for once the chunk is grown again.
    bool rerun_ = false;
    std::string editText_;
    double lastClick_ = 0.0;
    int lastClickKnob_ = -1;
    std::string status_ = "Loaded data/worldgen.txt";
    std::string hint_;
    double shotAt_ = 0.0, watchAt_ = 0.0;
    std::filesystem::file_time_type watched_{};

    // Knobs.

    /// The sections in the file's order, with the knobs in each.
    std::vector<std::pair<std::string, std::vector<size_t>>> sections() const {
        std::vector<std::pair<std::string, std::vector<size_t>>> out;
        const auto& all = tune::all();
        for (size_t i = 0; i < all.size(); i++) {
            if (out.empty() || out.back().first != all[i].section) out.push_back({all[i].section, {}});
            out.back().second.push_back(i);
        }
        return out;
    }
    bool changed(size_t k) const { return cur_[k] != file_[k]; }
    int changedCount() const {
        int n = 0;
        for (size_t k = 0; k < cur_.size(); k++) n += changed(k);
        return n;
    }
    /// Puts the file's values or the changed ones in the knob table.
    void use(bool file) {
        for (size_t k = 0; k < cur_.size(); k++) tune::set(k, file ? file_[k] : cur_[k]);
    }
    void setValue(size_t k, size_t i, float v) {
        const tune::Knob& knob = tune::all()[k];
        v = knob.whole ? std::round(v) : std::round(v * 1000.0f) / 1000.0f;
        if (cur_[k][i] == v) return;
        cur_[k][i] = v;
        tune::set(k, cur_[k]);
        dirty_ = true;
    }
    void remember(size_t k) { undo_.push_back({k, cur_[k]}); }
    void reset(size_t k) {
        if (!changed(k)) return;
        remember(k);
        cur_[k] = file_[k];
        tune::set(k, cur_[k]);
        dirty_ = true;
    }
    void undo() {
        if (undo_.empty()) return;
        auto [k, values] = undo_.back();
        undo_.pop_back();
        cur_[k] = values;
        tune::set(k, values);
        dirty_ = true;
        status_ = "Undid " + tune::all()[k].name;
    }
    void save() {
        use(false);
        if (!tune::save()) {
            status_ = "Couldn't write " + tune::source();
            return;
        }
        int n = changedCount();
        file_ = cur_;
        watched_ = stamp();
        status_ = "Saved " + std::to_string(n) + " knob" + (n == 1 ? "" : "s") + " to " + tune::source() +
                  ": make check will fail, make golden after";
    }

    // The file on disk.

    std::filesystem::file_time_type stamp() const {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(tune::source(), ec);
        return ec ? std::filesystem::file_time_type{} : t;
    }
    // Read again when it changes, keeping what was changed here unsaved.
    void watch() {
        if (GetTime() < watchAt_) return;
        watchAt_ = GetTime() + WATCH_EVERY;
        auto now = stamp();
        if (now == watched_) return;
        watched_ = now;
        std::vector<bool> mine(cur_.size());
        for (size_t k = 0; k < cur_.size(); k++) mine[k] = changed(k);
        tune::reload();
        const auto& all = tune::all();
        file_.resize(all.size());
        cur_.resize(all.size());
        mine.resize(all.size(), false);
        for (size_t k = 0; k < all.size(); k++) {
            file_[k] = all[k].values;
            if (!mine[k]) cur_[k] = file_[k];
        }
        if (folded_.size() < sections().size()) folded_.resize(sections().size(), false);
        use(false);
        dirty_ = true;
        status_ = "data/worldgen.txt changed on disk: read again";
    }

    // What the window shows, kept for next time and for Claude.

    void writeState() const {
        std::ofstream out(STATE_PATH);
        out << "# What tools/genview.cpp shows, read back at its start.\n";
        out << "seed " << seed_ << "\n";
        out << "kind " << kind_ << " " << KINDS[kind_].name << "\n";
        out << "footprint " << SHAPES[size_t(shape_)].name << " turn " << turns_ << " mirror " << mirror_ << "\n";
        out << "view " << (map_ ? "map" : "art") << " scale " << scale_ << " centre " << centre_.x << " " << centre_.y
            << "\n";
        out << "overlays";
        for (int o = 0; o < OVERLAYS; o++)
            if (shown_[o]) out << " " << o + 1;
        out << "\nreadout " << readout_ << "\n";
        out << "drop " << CROWDS[crowd_] << " humans " << MINUTES[minutes_] << " min heat " << HEAT_NAMES[heat_] << "\n";
        out << "mode " << (mode_ == Mode::Single ? "single" : mode_ == Mode::Compare ? "compare" : "grid") << "\n";
        if (!panes_.empty() && panes_.front()->chunk) {
            const Chunk& c = *panes_.front()->chunk;
            out << "shown " << panes_.front()->label << " at slot " << c.slot.x << " " << c.slot.y << "\n";
        }
        for (const auto& g : panes_) {
            if (!g->chunk) continue;
            out << "readout seed " << g->seed << ":";
            for (const auto& [name, v] : g->readout.rows) out << " " << name << "=" << shown(v) << ";";
            if (!g->readout.fault.empty()) out << " FAULT " << g->readout.fault;
            out << "\n";
            if (g->run) {
                const Run& r = *g->run;
                out << "run seed " << g->seed << ": " << r.dropped << " dropped, " << shown(r.t) << " of "
                    << shown(r.length) << " s, " << r.died << " died (" << r.thirst << " thirst, " << r.starved
                    << " hunger, " << r.bled << " bled, " << r.other << " other); seconds spent in the chunk:";
                for (int h = 0; h < DIED; h++) {
                    float sum = 0;
                    for (float v : r.heat[h]) sum += v;
                    out << " " << HEAT_NAMES[h] << "=" << shown(sum) << ";";
                }
                out << "\n";
            }
        }
        out << "# Knobs changed here and not yet saved: section, name, values.\n";
        const auto& all = tune::all();
        for (size_t k = 0; k < cur_.size(); k++) {
            if (!changed(k)) continue;
            out << "knob " << all[k].section << " " << all[k].name;
            for (float v : cur_[k]) out << " " << shortest(v, all[k].whole);
            out << "\n";
        }
    }
    void readState() {
        std::ifstream in(STATE_PATH);
        std::string row;
        const auto& all = tune::all();
        while (std::getline(in, row)) {
            std::istringstream w(row);
            std::string what;
            w >> what;
            if (what == "seed") w >> seed_;
            else if (what == "overlays") {
                for (int o; w >> o;)
                    if (o >= 1 && o <= OVERLAYS) shown_[o - 1] = true;
            } else if (what == "readout") w >> readout_;
            else if (what == "drop") {
                int crowd = 0, minutes = 0;
                std::string humans, min, heat, name;
                w >> crowd >> humans >> minutes >> min >> heat >> name;
                for (int i = 0; i < int(sizeof CROWDS / sizeof CROWDS[0]); i++)
                    if (CROWDS[i] == crowd) crowd_ = i;
                for (int i = 0; i < int(sizeof MINUTES / sizeof MINUTES[0]); i++)
                    if (MINUTES[i] == minutes) minutes_ = i;
                for (int i = 0; i < HEATS; i++)
                    if (name == HEAT_NAMES[i]) heat_ = i;
            }
            else if (what == "kind") w >> kind_;
            else if (what == "view") {
                std::string v;
                w >> v;
                map_ = v == "map";
            } else if (what == "mode") {
                std::string v;
                w >> v;
                mode_ = v == "compare" ? Mode::Compare : v == "grid" ? Mode::Grid : Mode::Single;
            } else if (what == "footprint") {
                std::string name, t, m;
                w >> name >> t >> turns_ >> m >> mirror_;
                for (size_t i = 0; i < SHAPES.size(); i++)
                    if (name == SHAPES[i].name) shape_ = int(i);
            } else if (what == "knob") {
                std::string section, name;
                w >> section >> name;
                for (size_t k = 0; k < all.size(); k++) {
                    if (all[k].section != section || all[k].name != name) continue;
                    std::vector<float> values;
                    for (float v; w >> v;) values.push_back(v);
                    if (values.size() == cur_[k].size()) cur_[k] = values;
                }
            }
        }
        kind_ = std::clamp(kind_, 0, KIND_COUNT - 1);
        turns_ = ((turns_ % 4) + 4) % 4;
        use(false);
    }

    // Growing.

    Footprint footprint() const { return turned(SHAPES[size_t(shape_)].parts, turns_, mirror_); }

    std::unique_ptr<Grown> grow(uint64_t seed) {
        auto g = std::make_unique<Grown>();
        g->seed = seed;
        g->world = std::make_unique<World>(seed);
        Rng looks{seed};
        g->world->makeSpawn(HumanLook::random(looks), seed);
        const KindPick& pick = KINDS[kind_];
        const Footprint parts = footprint();
        g->entry = GenViewProbe::spawnDoor(*g->world).second;
        g->chunk = pick.rolled ? GenViewProbe::rolled(*g->world) : GenViewProbe::grow(*g->world, pick.kind, parts);
        if (!g->chunk) {
            g->note = std::string("The game never grows ") + pick.name + " on " + SHAPES[size_t(shape_)].name;
            return g;
        }
        const Chunk& c = *g->chunk;
        if (c.kind == ChunkKind::Spawn) g->entry = c.origin() + Vec2i{c.w / 2, c.h / 2};
        Vec2i box = footprintBox(c.parts);
        g->label = std::string(kindName(c.kind)) + " " + std::to_string(box.x) + "x" + std::to_string(box.y) +
                   (int(c.parts.size()) != box.x * box.y ? " (" + std::to_string(c.parts.size()) + " slots)" : "");
        const Vector2 px = g->size();
        g->art = LoadRenderTexture(int(px.x), int(px.y));
        BeginTextureMode(g->art);
        ClearBackground(BLACK);
        const Human& viewer = g->world->human(0);
        Vector2 focus{float(c.origin().x * CELL_PX) + px.x / 2, float(c.origin().y * CELL_PX) + px.y / 2 + SPRITE_H / 2.0f};
        drawWorld(*g->world, viewer, [](const Human&) -> const HumanSprites& { std::abort(); }, floorTiles_,
                  statueArt_, true, 1.0f, Rectangle{0, 0, px.x, px.y}, focus, false);
        // Only the chunk: the spawn may lie in an odd footprint's box.
        for (int sy = 0; sy < box.y; sy++)
            for (int sx = 0; sx < box.x; sx++)
                if (!c.ownsSlot({sx, sy}))
                    DrawRectangle(sx * CHUNK * CELL_PX, sy * CHUNK * CELL_PX, CHUNK * CELL_PX, CHUNK * CELL_PX, BLACK);
        EndTextureMode();
        g->map = flatMap(*g->world, c);
        g->readout = readout(*g->world, c, g->entry);
        overlays(*g->world, c, g->entry, g->overlay);
        return g;
    }

    bool running() const {
        for (const auto& g : panes_)
            if (g->run && !g->run->done()) return true;
        return false;
    }
    bool hasRun() const {
        for (const auto& g : panes_)
            if (g->run) return true;
        return false;
    }
    /// Starts a run on every pane but the nine seeds', or stops those
    /// running.
    void toggleRun() {
        if (running()) {
            for (auto& g : panes_)
                if (g->run) g->run->length = g->run->t;
            status_ = "Stopped";
            return;
        }
        if (mode_ == Mode::Grid) {
            status_ = "Humans are dropped on one chunk, or on before and after, not on nine";
            return;
        }
        for (auto& g : panes_)
            if (g->chunk) {
                if (g->run) {
                    // The world has moved on: grow it again before dropping more.
                    dirty_ = true;
                    rerun_ = true;
                    return;
                }
                startRun(*g, CROWDS[crowd_], MINUTES[minutes_]);
            }
        status_ = "Running " + std::to_string(CROWDS[crowd_]) + " humans for " + std::to_string(MINUTES[minutes_]) +
                  " min";
    }
    void runs() {
        if (rerun_ && !dirty_) {
            rerun_ = false;
            toggleRun();
        }
        const double start = GetTime();
        while (running() && GetTime() - start < SIM_BUDGET)
            for (auto& g : panes_)
                if (g->run && !g->run->done()) {
                    const double before = GetTime();
                    stepRun(*g);
                    g->run->wall += GetTime() - before;
                }
        for (auto& g : panes_) {
            Run* r = g->run.get();
            if (!r) continue;
            if (r->texHeat == heat_ && (r->drawn || GetTime() - r->drawnAt < HEAT_EVERY)) continue;
            if (r->tex.id) UnloadTexture(r->tex);
            r->tex = heatMap(*r, *g->chunk, heat_);
            r->texHeat = heat_;
            r->drawnAt = GetTime();
            r->drawn = r->done();
            shotAt_ = GetTime() + SHOT_DELAY;
            if (r->done() && !running()) status_ = "Run done: J picks the heatmap";
        }
    }

    void regrow() {
        dirty_ = false;
        panes_.clear();
        if (mode_ == Mode::Single) {
            panes_.push_back(grow(seed_));
        } else if (mode_ == Mode::Compare) {
            use(true);
            panes_.push_back(grow(seed_));
            use(false);
            panes_.push_back(grow(seed_));
            panes_[0]->label = "file: " + panes_[0]->label;
            panes_[1]->label = "changed: " + panes_[1]->label;
        } else {
            for (uint64_t i = 0; i < 9; i++) panes_.push_back(grow(seed_ + i));
        }
        Vector2 size = panes_.front()->size();
        if (size.x != fittedFor_.x || size.y != fittedFor_.y || scale_ <= 0) fit();
        shotAt_ = GetTime() + SHOT_DELAY;
    }

    // The view.

    Rectangle viewArea() const {
        return {float(panelW_), float(BAR_H), float(GetScreenWidth() - panelW_),
                float(GetScreenHeight() - BAR_H - STATUS_H)};
    }
    std::vector<Rectangle> paneAreas() const {
        Rectangle v = viewArea();
        std::vector<Rectangle> out;
        if (mode_ == Mode::Single) {
            out.push_back(v);
        } else if (mode_ == Mode::Compare) {
            float w = (v.width - PAD) / 2;
            out.push_back({v.x, v.y, w, v.height});
            out.push_back({v.x + w + PAD, v.y, w, v.height});
        } else {
            float w = (v.width - 2 * PAD) / 3, h = (v.height - 2 * PAD) / 3;
            for (int i = 0; i < 9; i++) out.push_back({v.x + (i % 3) * (w + PAD), v.y + (i / 3) * (h + PAD), w, h});
        }
        return out;
    }
    void fit() {
        if (panes_.empty()) return;
        Vector2 size = panes_.front()->size();
        fittedFor_ = size;
        if (size.x <= 0) return;
        Rectangle a = paneAreas().front();
        scale_ = std::min((a.width - 2 * PAD) / size.x, (a.height - 2 * PAD - ROW) / size.y);
        centre_ = {size.x / 2, size.y / 2};
    }

    void drawPane(const Grown& g, Rectangle a, bool own) {
        DrawRectangleRec(a, BLACK);
        BeginScissorMode(int(a.x), int(a.y), int(a.width), int(a.height));
        if (g.chunk) {
            Vector2 size = g.size();
            float s = scale_;
            Vector2 c = centre_;
            if (own) {
                s = std::min((a.width - 2 * PAD) / size.x, (a.height - 2 * PAD - ROW) / size.y);
                c = {size.x / 2, size.y / 2};
            }
            Rectangle dest{a.x + a.width / 2 - c.x * s, a.y + ROW + (a.height - ROW) / 2 - c.y * s, size.x * s,
                           size.y * s};
            if (map_) {
                SetTextureFilter(g.map, TEXTURE_FILTER_POINT);
                DrawTexturePro(g.map, {0, 0, float(g.map.width), float(g.map.height)}, dest, {0, 0}, 0, WHITE);
            } else {
                SetTextureFilter(g.art.texture, s < 1.0f ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
                DrawTexturePro(g.art.texture, {0, 0, size.x, -size.y}, dest, {0, 0}, 0, WHITE);
            }
            for (int o = 0; o < OVERLAYS; o++)
                if (shown_[o])
                    DrawTexturePro(g.overlay[o], {0, 0, float(g.overlay[o].width), float(g.overlay[o].height)}, dest,
                                   {0, 0}, 0, WHITE);
            if (g.run && g.run->tex.id) {
                if (g.run->texHeat != heat_) g.run->drawn = false;
                DrawTexturePro(g.run->tex, {0, 0, float(g.run->tex.width), float(g.run->tex.height)}, dest, {0, 0},
                               0, WHITE);
            }
        } else {
            text(g.note, int(a.x + PAD), int(a.y + a.height / 2), BAD);
        }
        DrawRectangle(int(a.x), int(a.y), int(a.width), ROW, Fade(BLACK, 0.7f));
        text(g.label + "   seed " + std::to_string(g.seed), int(a.x + PAD), int(a.y + 3), INK);
        if (g.run) {
            // The run on a line of its own under the chunk's name.
            const Run& r = *g.run;
            char buf[160];
            std::snprintf(buf, sizeof buf, "%d dropped, %d:%02d of %d:00, %d died (%d thirst, %d hunger, %d bled, %d other), x%.0f%s",
                          r.dropped, int(r.t) / 60, int(r.t) % 60, int(r.length) / 60, r.died, r.thirst, r.starved,
                          r.bled, r.other, r.wall > 0 ? r.t / r.wall : 0.0, r.done() ? "" : "...");
            DrawRectangle(int(a.x), int(a.y) + ROW, int(a.width), ROW, Fade(BLACK, 0.7f));
            text(buf, int(a.x + PAD), int(a.y) + ROW + 3, r.done() ? INK : CHANGED);
        }
        if (!g.readout.fault.empty()) {
            DrawRectangle(int(a.x), int(a.y + a.height) - ROW, int(a.width), ROW, Fade(BAD, 0.85f));
            text("genprobe would fail it: " + g.readout.fault, int(a.x + PAD), int(a.y + a.height) - ROW + 3, WHITE);
        }
        EndScissorMode();
    }

    /// The readout in a box at the top right of an area: a column of values
    /// for each, headed, those that differ from the first column's yellow.
    void drawReadout(Rectangle a, const std::vector<std::string>& heads,
                     const std::vector<std::vector<float>>& columns, bool mark) {
        if (columns.empty() || panes_.empty()) return;
        const auto& rows = panes_.front()->readout.rows;
        int labelW = 0;
        for (const auto& row : rows) labelW = std::max(labelW, width(row.first));
        labelW += 2 * PAD;
        const int colW = 80;
        int w = labelW + int(columns.size()) * colW + 3 * PAD, h = int(rows.size() + 1) * ROW + PAD;
        Rectangle box{a.x + a.width - float(w) - PAD, a.y + ROW + PAD, float(w), float(h)};
        DrawRectangleRec(box, Fade(PANEL, 0.92f));
        DrawRectangleLinesEx(box, 1, LINE);
        for (size_t j = 0; j < heads.size(); j++)
            text(heads[j], int(box.x) + PAD + labelW + PAD + int(j) * colW, int(box.y) + 4, DIM);
        for (size_t i = 0; i < rows.size(); i++) {
            int y = int(box.y) + 4 + int(i + 1) * ROW;
            text(rows[i].first, int(box.x) + PAD, y, INK);
            for (size_t j = 0; j < columns.size(); j++) {
                if (i >= columns[j].size()) continue;
                bool differs = mark && j > 0 && columns[j][i] != columns[0][i];
                text(shown(columns[j][i]), int(box.x) + PAD + labelW + PAD + int(j) * colW, y, differs ? CHANGED : INK);
            }
        }
    }

    void drawReadouts() {
        if (!readout_ || panes_.empty()) return;
        auto values = [](const Grown& g) {
            std::vector<float> v;
            for (const auto& row : g.readout.rows) v.push_back(row.second);
            return v;
        };
        std::vector<Rectangle> areas = paneAreas();
        if (mode_ == Mode::Single) {
            if (panes_[0]->chunk) drawReadout(areas[0], {}, {values(*panes_[0])}, false);
        } else if (mode_ == Mode::Compare) {
            if (panes_[0]->chunk && panes_[1]->chunk)
                drawReadout(areas[1], {"file", "changed"}, {values(*panes_[0]), values(*panes_[1])}, true);
        } else {
            // The least, the mean and the most over the nine.
            std::vector<float> lo, mean, hi;
            int n = 0;
            for (const auto& g : panes_) {
                if (!g->chunk) continue;
                std::vector<float> v = values(*g);
                if (n++ == 0) {
                    lo = hi = mean = v;
                    continue;
                }
                for (size_t i = 0; i < v.size(); i++) {
                    lo[i] = std::min(lo[i], v[i]);
                    hi[i] = std::max(hi[i], v[i]);
                    mean[i] += v[i];
                }
            }
            if (!n) return;
            for (float& m : mean) m /= float(n);
            drawReadout(viewArea(), {"least", "mean", "most"}, {lo, mean, hi}, false);
        }
    }

    void drawView() {
        std::vector<Rectangle> areas = paneAreas();
        for (size_t i = 0; i < panes_.size() && i < areas.size(); i++) {
            drawPane(*panes_[i], areas[i], mode_ == Mode::Grid);
            if (mode_ == Mode::Grid && over(areas[i])) {
                DrawRectangleLinesEx(areas[i], 2, CHANGED);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    seed_ = panes_[i]->seed;
                    mode_ = Mode::Single;
                    dirty_ = true;
                    return;
                }
            }
        }
        if (mode_ == Mode::Grid || !over(viewArea())) {
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) panning_ = false;
        } else {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) panning_ = true;
            if (float wheel = GetMouseWheelMove(); wheel != 0 && scale_ > 0) {
                Rectangle a = areas.front();
                for (Rectangle r : areas)
                    if (over(r)) a = r;
                Vector2 m = GetMousePosition();
                Vector2 mid{a.x + a.width / 2, a.y + ROW + (a.height - ROW) / 2};
                Vector2 at{centre_.x + (m.x - mid.x) / scale_, centre_.y + (m.y - mid.y) / scale_};
                scale_ = std::clamp(scale_ * std::pow(1.2f, wheel), 0.05f, 8.0f);
                centre_ = {at.x - (m.x - mid.x) / scale_, at.y - (m.y - mid.y) / scale_};
            }
        }
        if (panning_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && scale_ > 0) {
            Vector2 d = GetMouseDelta();
            centre_ = {centre_.x - d.x / scale_, centre_.y - d.y / scale_};
        } else {
            panning_ = false;
        }
    }

    // The top bar and the status line.

    /// A button; returns 1 when clicked, -1 when right-clicked.
    int button(int& x, const std::string& label, bool on = false) {
        Rectangle r{float(x), float(barY_ + 4), float(width(label) + 2 * PAD), float(BAR_ROW - 8)};
        x += int(r.width) + 4;
        bool hot = over(r);
        DrawRectangleRec(r, on ? FILL : hot ? HOT : PANEL);
        text(label, int(r.x + PAD), int(r.y + 4), INK);
        if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 1;
        if (hot && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) return -1;
        return 0;
    }

    void drawBar() {
        DrawRectangle(0, 0, GetScreenWidth(), BAR_H, PANEL);
        DrawLine(0, BAR_H - 1, GetScreenWidth(), BAR_H - 1, LINE);
        int x = PAD;
        if (int d = button(x, std::string("Kind: ") + KINDS[kind_].name)) {
            kind_ = (kind_ + d + KIND_COUNT) % KIND_COUNT;
            dirty_ = true;
        }
        if (int d = button(x, std::string("Footprint: ") + SHAPES[size_t(shape_)].name)) {
            shape_ = (shape_ + d + int(SHAPES.size())) % int(SHAPES.size());
            turns_ = 0;
            mirror_ = false;
            dirty_ = true;
        }
        if (int d = button(x, "Turn " + std::to_string(turns_ * 90))) {
            turns_ = (turns_ + d + 4) % 4;
            dirty_ = true;
        }
        if (button(x, "Mirror", mirror_)) {
            mirror_ = !mirror_;
            dirty_ = true;
        }
        x += PAD;
        if (button(x, "<")) step(-1);
        text("seed " + std::to_string(seed_), x, barY_ + 10, INK);
        x += width("seed " + std::to_string(seed_)) + 4;
        if (button(x, ">")) step(1);
        if (button(x, "R")) reroll();
        x += PAD;
        if (button(x, map_ ? "View: map" : "View: art")) map_ = !map_;
        const char* modes[] = {"One", "Before/after", "Nine seeds"};
        if (int d = button(x, std::string("Mode: ") + modes[int(mode_)])) {
            mode_ = Mode((int(mode_) + d + 3) % 3);
            dirty_ = true;
        }
        x += PAD;
        int n = changedCount();
        if (button(x, n ? "Save " + std::to_string(n) : "Save", n > 0)) save();
        barY_ = BAR_ROW;
        x = PAD;
        for (int o = 0; o < OVERLAYS; o++)
            if (button(x, std::to_string(o + 1) + " " + OVERLAY_NAMES[o], shown_[o])) shown_[o] = !shown_[o];
        x += PAD;
        if (button(x, "T Readout", readout_)) readout_ = !readout_;
        x += PAD;
        if (button(x, running() ? "H Stop" : "H Drop", running())) toggleRun();
        if (int d = button(x, std::to_string(CROWDS[crowd_]) + " humans")) {
            const int n = int(sizeof CROWDS / sizeof CROWDS[0]);
            crowd_ = (crowd_ + d + n) % n;
        }
        if (int d = button(x, std::to_string(MINUTES[minutes_]) + " min")) {
            const int n = int(sizeof MINUTES / sizeof MINUTES[0]);
            minutes_ = (minutes_ + d + n) % n;
        }
        if (int d = button(x, std::string("J ") + HEAT_NAMES[heat_], hasRun())) heat_ = (heat_ + d + HEATS) % HEATS;
        barY_ = 0;
    }

    void drawStatus() {
        int y = GetScreenHeight() - STATUS_H;
        DrawRectangle(0, y, GetScreenWidth(), STATUS_H, PANEL);
        DrawLine(0, y, GetScreenWidth(), y, LINE);
        text(hint_.empty() ? status_ : hint_, PAD, y + 5, hint_.empty() ? DIM : INK);
    }

    void step(int d) {
        seed_ = uint64_t(int64_t(seed_) + d);
        dirty_ = true;
    }
    void reroll() {
        Rng r{seed_ * 0x9E3779B97F4A7C15ull + 1};
        seed_ = uint64_t(r.range(1, 999999));
        dirty_ = true;
    }

    // The knob panel.

    bool dragging() const { return dragKnob_ >= 0 || panning_; }

    void drawPanel() {
        Rectangle panel{0, float(BAR_H), float(panelW_), float(GetScreenHeight() - BAR_H - STATUS_H)};
        DrawRectangleRec(panel, PANEL);
        DrawLine(panelW_ - 1, BAR_H, panelW_ - 1, GetScreenHeight() - STATUS_H, LINE);
        hint_.clear();
        if (over(panel)) scroll_ -= GetMouseWheelMove() * ROW * 3;
        BeginScissorMode(0, BAR_H, panelW_, int(panel.height));
        const auto& all = tune::all();
        auto secs = sections();
        float y = panel.y + PAD - scroll_;
        for (size_t s = 0; s < secs.size(); s++) {
            const auto& [name, knobs] = secs[s];
            int n = 0;
            for (size_t k : knobs) n += changed(k);
            Rectangle head{0, y, float(panelW_), float(ROW)};
            if (over(head) && over(panel)) {
                DrawRectangleRec(head, HOT);
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) folded_[s] = !folded_[s];
            }
            text(std::string(folded_[s] ? "+ [" : "- [") + name + "]" + (n ? "  " + std::to_string(n) + " changed" : ""),
                 PAD, int(y + 3), n ? CHANGED : INK);
            y += ROW;
            if (folded_[s]) continue;
            for (size_t k : knobs) {
                const tune::Knob& knob = all[k];
                const bool list = knob.values.size() > 1;
                if (list) {
                    Rectangle r{0, y, float(panelW_), float(ROW)};
                    knobName(k, r, panel);
                    y += ROW;
                }
                for (size_t i = 0; i < knob.values.size(); i++) {
                    Rectangle r{0, y, float(panelW_), float(ROW)};
                    if (!list) knobName(k, r, panel);
                    else text(std::to_string(i + 1), 3 * PAD, int(y + 3), DIM);
                    slider(k, i, r, panel);
                    y += ROW;
                }
            }
            y += PAD;
        }
        EndScissorMode();
        float total = y + scroll_ - panel.y;
        scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, total - panel.height + PAD));
    }

    void knobName(size_t k, Rectangle r, Rectangle panel) {
        const tune::Knob& knob = tune::all()[k];
        bool hot = over(r) && over(panel);
        if (hot) {
            DrawRectangleRec(r, HOT);
            hint_ = knob.name + ": " + knob.comment;
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) reset(k);
        }
        text(knob.name, 2 * PAD, int(r.y + 3), changed(k) ? CHANGED : INK);
    }

    void slider(size_t k, size_t i, Rectangle r, Rectangle panel) {
        const tune::Knob& knob = tune::all()[k];
        const float v = cur_[k][i];
        Rectangle track{float(nameW_), r.y + ROW / 2.0f - 4, float(SLIDER_W), 8};
        Rectangle hit{track.x - 4, r.y, track.width + 8, float(ROW)};
        bool hot = over(r) && over(panel);
        if (hot && knob.values.size() > 1) {
            DrawRectangleRec(r, HOT);
            hint_ = knob.name + " " + std::to_string(i + 1) + ": " + knob.comment;
        }
        const float span = knob.hi - knob.lo;
        float f = span > 0 ? std::clamp((v - knob.lo) / span, 0.0f, 1.0f) : 0.0f;
        DrawRectangleRec(track, TRACK);
        DrawRectangle(int(track.x), int(track.y), int(track.width * f), int(track.height), FILL);
        if (file_[k][i] != v && span > 0) {
            float ff = std::clamp((file_[k][i] - knob.lo) / span, 0.0f, 1.0f);
            DrawRectangle(int(track.x + track.width * ff) - 1, int(track.y) - 3, 2, int(track.height) + 6, DIM);
        }
        DrawRectangle(int(track.x + track.width * f) - 3, int(track.y) - 4, 6, int(track.height) + 8,
                      file_[k][i] != v ? CHANGED : INK);
        if (hot && over(hit) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            remember(k);
            dragKnob_ = int(k);
            dragValue_ = int(i);
        }
        if (dragKnob_ == int(k) && dragValue_ == int(i)) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                float t = std::clamp((GetMousePosition().x - track.x) / track.width, 0.0f, 1.0f);
                setValue(k, i, knob.lo + t * span);
            } else {
                dragKnob_ = -1;
            }
        }
        // Left and Right step the knob under the mouse.
        if (hot && editKnob_ < 0) {
            float stepBy = knob.whole ? 1.0f : span <= 2.0f ? 0.01f : 0.1f;
            int d = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) ? 1
                    : IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) ? -1
                                                                              : 0;
            if (d) {
                remember(k);
                setValue(k, i, v + d * stepBy);
            }
        }
        // The value, or the text being typed over it.
        Rectangle box{track.x + track.width + 2 * PAD, r.y, float(panelW_) - track.x - track.width - 3 * PAD,
                      float(ROW)};
        bool editing = editKnob_ == int(k) && editValue_ == int(i);
        if (editing) {
            DrawRectangleRec(box, BLACK);
            text(editText_ + (int(GetTime() * 2) % 2 ? "_" : ""), int(box.x + 4), int(r.y + 3), CHANGED);
        } else {
            text(shortest(v, knob.whole), int(box.x + 4), int(r.y + 3), file_[k][i] != v ? CHANGED : INK);
        }
        if (hot && over(box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int id = int(k * 64 + i);
            if (lastClickKnob_ == id && GetTime() - lastClick_ < 0.4) {
                editKnob_ = int(k);
                editValue_ = int(i);
                editText_ = shortest(v, knob.whole);
            }
            lastClickKnob_ = id;
            lastClick_ = GetTime();
        }
    }

    // Keys.

    void keys() {
        if (editKnob_ >= 0) {
            for (int c = GetCharPressed(); c; c = GetCharPressed())
                if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '/') editText_ += char(c);
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !editText_.empty())
                editText_.pop_back();
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                float v = 0;
                size_t slash = editText_.find('/');
                char* end = nullptr;
                bool ok;
                if (slash != std::string::npos) {
                    float a = std::strtof(editText_.substr(0, slash).c_str(), nullptr);
                    float b = std::strtof(editText_.substr(slash + 1).c_str(), nullptr);
                    ok = b != 0;
                    v = ok ? a / b : 0;
                } else {
                    v = std::strtof(editText_.c_str(), &end);
                    ok = !editText_.empty() && end == editText_.c_str() + editText_.size();
                }
                if (ok) {
                    remember(size_t(editKnob_));
                    setValue(size_t(editKnob_), size_t(editValue_), v);
                }
                editKnob_ = -1;
            }
            if (IsKeyPressed(KEY_ESCAPE)) editKnob_ = -1;
            return;
        }
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (ctrl && IsKeyPressed(KEY_Z)) undo();
        if (!ctrl && IsKeyPressed(KEY_S)) save();
        if (IsKeyPressed(KEY_R)) reroll();
        if (IsKeyPressed(KEY_V)) map_ = !map_;
        if (IsKeyPressed(KEY_F)) fit();
        if (IsKeyPressed(KEY_T)) readout_ = !readout_;
        if (IsKeyPressed(KEY_H)) toggleRun();
        if (IsKeyPressed(KEY_J)) heat_ = (heat_ + 1) % HEATS;
        for (int o = 0; o < OVERLAYS; o++)
            if (IsKeyPressed(KeyboardKey(KEY_ONE + o))) shown_[o] = !shown_[o];
        if (IsKeyPressed(KEY_C)) {
            mode_ = mode_ == Mode::Compare ? Mode::Single : Mode::Compare;
            dirty_ = true;
        }
        if (IsKeyPressed(KEY_G)) {
            mode_ = mode_ == Mode::Grid ? Mode::Single : Mode::Grid;
            dirty_ = true;
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 960, "The Mall: world generator");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    {
        GenView view;
        for (int i = 1; i < argc; i++)
            if (std::string(argv[i]) == "--drop") view.dropAtStart();
        while (!WindowShouldClose()) view.frame();
        view.close();
    }
    CloseWindow();
    return 0;
}
