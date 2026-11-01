// The world generator editor: the knobs of data/worldgen.txt on the left,
// a chunk grown with them on the right, grown again on the same seed each
// time a knob moves, so what changes is that knob's doing, not the dice.
// See AI/worldgen_editor_plan.md.
//
// Usage: build/genview   (or: make genview, or ./genview.sh from the host)
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
//
// The file is watched: changed on disk, by hand or by Claude in a chat, it
// is read again within a second and the chunk regrown, keeping knobs
// changed here but not yet saved. After each regrow, once things settle,
// the window is saved to build/genview.png and what it shows to
// build/genview.txt (seed, kind, footprint, view, mode, unsaved knobs), so
// Claude can see what the user sees. That file is read back at start, so a
// rebuild opens on the same chunk.
#include "../tests/probe.h"
#include "core/rng.h"
#include "human/human_look.h"
#include "render/floor_tiles.h"
#include "render/statue_art.h"
#include "render/world_view.h"
#include "world/gen_tuning.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
constexpr int BAR_H = 36;
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

/// One chunk grown, with its world, and drawn both ways.
struct Grown {
    std::unique_ptr<World> world;
    const Chunk* chunk = nullptr;
    RenderTexture2D art{};
    Texture2D map{};
    uint64_t seed = 0;
    std::string label, note;

    Grown() = default;
    Grown(const Grown&) = delete;
    Grown& operator=(const Grown&) = delete;
    ~Grown() {
        if (art.id) UnloadRenderTexture(art);
        if (map.id) UnloadTexture(map);
    }
    /// The chunk's size in world pixels.
    Vector2 size() const { return chunk ? Vector2{float(chunk->w * CELL_PX), float(chunk->h * CELL_PX)} : Vector2{}; }
};

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
        keys();
        BeginDrawing();
        ClearBackground(BG);
        drawView();
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
    bool mirror_ = false, map_ = false, dirty_ = true;
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
        out << "view " << (map_ ? "map" : "art") << "\n";
        out << "mode " << (mode_ == Mode::Single ? "single" : mode_ == Mode::Compare ? "compare" : "grid") << "\n";
        if (!panes_.empty() && panes_.front()->chunk) {
            const Chunk& c = *panes_.front()->chunk;
            out << "shown " << panes_.front()->label << " at slot " << c.slot.x << " " << c.slot.y << "\n";
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
        g->chunk = pick.rolled ? GenViewProbe::rolled(*g->world) : GenViewProbe::grow(*g->world, pick.kind, parts);
        if (!g->chunk) {
            g->note = std::string("The game never grows ") + pick.name + " on " + SHAPES[size_t(shape_)].name;
            return g;
        }
        const Chunk& c = *g->chunk;
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
        return g;
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
        } else {
            text(g.note, int(a.x + PAD), int(a.y + a.height / 2), BAD);
        }
        DrawRectangle(int(a.x), int(a.y), int(a.width), ROW, Fade(BLACK, 0.7f));
        text(g.label + "   seed " + std::to_string(g.seed), int(a.x + PAD), int(a.y + 3), INK);
        EndScissorMode();
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
        Rectangle r{float(x), 4, float(width(label) + 2 * PAD), float(BAR_H - 8)};
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
        text("seed " + std::to_string(seed_), x, 10, INK);
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

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 960, "The Mall: world generator");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    {
        GenView view;
        while (!WindowShouldClose()) view.frame();
        view.close();
    }
    CloseWindow();
    return 0;
}
