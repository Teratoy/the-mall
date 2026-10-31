// A screenshot of the game without a visible window: runs a scene for a while
// in a hidden raylib window and saves what it draws as a PNG.
//
// Usage: build/shot out.png [options]   (or: make shot, to build/shot.png)
//   --scene world|menu|create|obituary   which screen (world)
//   --seconds S    world time to run before the shot (8)
//   --seed N       the world's seed (7); the human you made is seeded 99
//   --zoom Z       world zoom level 0-3 (2)
//   --stats        open the followed human's stats
//   --log          open the followed human's log
//   --map          open the followed human's map
//   --reveal       show the whole world, not just what the human knows
//   --wide         a wide window, so the world takes the PC layout
//   --vending      grow the mall round the spawn until a vending machine
//                  stands in it, and put the followed human in front of it
//   --server       grow the mall round the spawn until a server room
//                  stands in it, and put the followed human inside it
//   --washroom     the same for a data centre's washroom, the human at its doorway
//   --centre-vending  the same for a machine in a data centre's corridor alcove
//   --pissed       grow the mall round the spawn until a toilet stands in it,
//                  put the followed human out in front of it, and just before the
//                  shot piss in the toilets more and more, left to right, to
//                  see their water yellow
//   --pile         put a heap of cakes and bottles down near the human at
//                  the start, to see how piles look
//   --smells       leave a big and a small pile of fresh feces near the
//                  human at the start, to see the flies over them
//
// The world is never put in the obituary: data/obituary.txt is left alone.
// The image is read back before EndDrawing (after the swap it is black).
#include "core/rng.h"
#include "human/human_look.h"
#include "render/canvas.h"
#include "scenes/create_scene.h"
#include "scenes/menu_scene.h"
#include "scenes/obituary_scene.h"
#include "scenes/world_scene.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <raylib.h>
#include "human/human.h"
#include <rlgl.h>
#include <algorithm>
#include <string>

/// Sets up the world scene the way the options ask; a friend of WorldScene.
struct ShotProbe {
    static void neverRecord(WorldScene& s) { s.recorded_ = true; }
    static void set(WorldScene& s, int zoom, bool stats, bool log, bool reveal, bool map) {
        s.zoom_ = zoom;
        s.statsOpen_ = stats;
        s.logOpen_ = log && !stats;
        s.mapOpen_ = map && !stats && !log;
        s.reveal_ = reveal;
    }
    /// Fresh feces, a full pile and a small one, a few cells either side of
    /// the followed human, for the flies.
    static void smells(WorldScene& s) {
        World& w = s.world_;
        Vec2i at = w.human(s.follow_).cell;
        uint32_t n = 0;
        for (Vec2i d : {Vec2i{3, 0}, Vec2i{-3, 0}}) {
            Vec2i c = at + d;
            if (w.tile(c) != Tile::Floor) continue;
            float size = n == 0 ? 1.0f : 0.3f;
            w.mess().drop(MessKind::Feces, c, (c.x + 0.5f) * CELL_PX, (c.y + 0.7f) * CELL_PX, size, w.time(), -1, 7u + n);
            n++;
        }
    }
    /// Cakes and bottles, clean, foul and bloody, on the free floor nearest
    /// a few cells right of the followed human.
    static void pile(WorldScene& s) {
        World& w = s.world_;
        Vec2i from = w.human(s.follow_).cell + Vec2i{3, 0};
        for (int r = 0; r < 10; r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) {
                    Vec2i c = from + Vec2i{x, y};
                    if (w.tile(c) != Tile::Floor || w.occupant(c) || w.hasCake(c) || w.hasKey(c)) continue;
                    w.putDown(c, 4, {Bottle{1.0f}, Bottle{0.5f, 0.7f, 0.0f}, Bottle{0.3f, 0.0f, 0.8f}});
                    return;
                }
    }
    /// Grows slots round the spawn, nearest first, until one holds a
    /// machine, then moves the followed human two cells out from one, to its side,
    /// facing the viewer if any does.
    static void vending(WorldScene& s) {
        World& w = s.world_;
        auto facingUs = [&] { return std::any_of(w.machines().begin(), w.machines().end(), [](const Machine& m) { return !m.side(); }); };
        for (int r = 1; r <= 6 && !facingUs(); r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) w.ensureGenerated(Vec2i{x, y} * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
        if (w.machines().empty()) return;
        // One facing the viewer, if any, so its stock shows.
        const Machine* pick = &w.machines().front();
        for (const Machine& m : w.machines())
            if (!m.side()) {
                pick = &m;
                break;
            }
        const Machine& m = *pick;
        Human& h = w.human(s.follow_);
        Vec2i to = m.at + m.facing * 2 - m.along() * 2;
        if (w.tile(to) != Tile::Floor || w.occupant(to)) return;
        w.moveOccupant(h.cell, to, &h);
        h.cell = h.prev = to;
        h.observe(w);
    }

    /// Grows the mall round the spawn until a server room stands in it, the
    /// biggest of those found, and puts the followed human on its floor a
    /// few cells in from its door.
    static void server(WorldScene& s) {
        World& w = s.world_;
        for (int r = 1; r <= 6 && w.serverRooms().empty(); r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) w.ensureGenerated(Vec2i{x, y} * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
        if (w.serverRooms().empty()) return;
        const ServerRoom* pick = &w.serverRooms().front();
        for (const ServerRoom& r : w.serverRooms())
            if (r.w * r.h > pick->w * pick->h) pick = &r;
        Human& h = w.human(s.follow_);
        Vec2i in = w.airlock(pick->inner)->in;
        for (Vec2i to = pick->inner + in; pick->contains(to); to = to + in) {
            if (w.tile(to) != Tile::Floor || w.occupant(to) || std::abs((to - pick->inner).x + (to - pick->inner).y) < 3)
                continue;
            w.moveOccupant(h.cell, to, &h);
            h.cell = h.prev = to;
            h.observe(w);
            return;
        }
    }

    /// Grows the mall round the spawn until a data centre stands in it with
    /// a machine facing the viewer in a corridor alcove, and puts the
    /// followed human in the corridor a little in front of it.
    static void centreVending(WorldScene& s) {
        World& w = s.world_;
        // Only a data centre's machines stand in alcoves, with wall past their ends.
        auto inCentre = [&](const Machine& m) { return !m.side() && w.tile(m.at - m.along()) == Tile::Wall; };
        auto found = [&] { return std::any_of(w.machines().begin(), w.machines().end(), inCentre); };
        for (int r = 1; r <= 6 && !found(); r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) w.ensureGenerated(Vec2i{x, y} * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
        auto it = std::find_if(w.machines().begin(), w.machines().end(), inCentre);
        if (it == w.machines().end()) return;
        Human& h = w.human(s.follow_);
        Vec2i to = it->at + it->facing * 3 - it->along();
        if (w.tile(to) != Tile::Floor || w.occupant(to)) return;
        w.moveOccupant(h.cell, to, &h);
        h.cell = h.prev = to;
        h.observe(w);
    }

    /// Grows the mall round the spawn until a data centre stands in it, and
    /// puts the followed human in a doorway of one of its washrooms: the
    /// floor cell walled on both sides two in front of a toilet.
    static void washroom(WorldScene& s) {
        World& w = s.world_;
        // In a chunk with a server room: the restrooms of mazes have none.
        auto inCentre = [&](Vec2i c) {
            return std::any_of(w.serverRooms().begin(), w.serverRooms().end(),
                               [&](const ServerRoom& r) { return w.chunkIdAt({r.x, r.y}) == w.chunkIdAt(c); });
        };
        auto found = [&] {
            for (const auto& [at, t] : w.toilets()) if (inCentre(at)) return true;
            return false;
        };
        for (int r = 1; r <= 6 && !found(); r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) w.ensureGenerated(Vec2i{x, y} * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
        std::vector<Vec2i> doors;
        for (const auto& [at, t] : w.toilets()) {
            if (!inCentre(at)) continue;
            Vec2i out = t.front - at, door = at + out * 3, side{out.y, out.x};
            if (w.tile(door) == Tile::Floor && w.tile(door + side) == Tile::Wall && w.tile(door - side) == Tile::Wall)
                doors.push_back(door);
        }
        if (doors.empty()) return;
        std::sort(doors.begin(), doors.end(), [](Vec2i a, Vec2i b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
        Human& h = w.human(s.follow_);
        Vec2i to = doors.front();
        if (w.occupant(to)) return;
        w.moveOccupant(h.cell, to, &h);
        h.cell = h.prev = to;
        h.observe(w);
    }

    /// Grows the mall round the spawn until a toilet stands in it, and puts
    /// the followed human a little way out in front of the one nearest the
    /// spawn.
    static void toilet(WorldScene& s) {
        World& w = s.world_;
        for (int r = 1; r <= 6 && w.toilets().empty(); r++)
            for (int y = -r; y <= r; y++)
                for (int x = -r; x <= r; x++) w.ensureGenerated(Vec2i{x, y} * CHUNK + Vec2i{CHUNK / 2, CHUNK / 2});
        Human& h = w.human(s.follow_);
        const Toilet* best = nullptr;
        Vec2i bestAt;
        for (const auto& [at, t] : w.toilets())
            if (!best || cheb(t.front, h.cell) < cheb(best->front, h.cell) ||
                (cheb(t.front, h.cell) == cheb(best->front, h.cell) &&
                 (t.front.y != best->front.y ? t.front.y < best->front.y : t.front.x < best->front.x)))
                best = &t, bestAt = at;
        if (!best) return;
        // Two steps out from it, so its map symbol shows, else at it.
        Vec2i to = best->front + (best->front - bestAt) * 2;
        if (w.tile(to) != Tile::Floor || w.occupant(to)) to = best->front;
        if (w.occupant(to)) return;
        w.moveOccupant(h.cell, to, &h);
        h.cell = h.prev = to;
        h.observe(w);
    }

    /// Pisses in every toilet, most in the one nearest the followed human and
    /// less in each further off: yellow through, then paler, then none. The
    /// human sees what it can of them.
    static void pissed(WorldScene& s) {
        World& w = s.world_;
        Vec2i from = w.human(s.follow_).cell;
        std::vector<Vec2i> at;
        for (const auto& [c, t] : w.toilets()) at.push_back(c);
        std::sort(at.begin(), at.end(), [&](Vec2i a, Vec2i b) {
            if (cheb(a, from) != cheb(b, from)) return cheb(a, from) < cheb(b, from);
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        });
        constexpr float PISS[] = {0.35f, 0.2f, 0.1f, 0.04f};
        for (size_t i = 0; i < at.size() && i < std::size(PISS); i++) w.soilToilet(at[i], PISS[i], true);
        // So it knows, for its map.
        w.human(s.follow_).observe(w);
    }
};

int main(int argc, char** argv) {
    if (argc < 2 || argv[1][0] == '-') {
        std::fprintf(stderr, "usage: %s out.png [--scene world|menu|create|obituary] [--seconds S] [--seed N] "
                             "[--zoom 0-3] [--stats] [--log] [--map] [--reveal] [--wide] [--pile] [--smells] "
                             "[--vending] [--server] [--washroom] [--pissed]\n", argv[0]);
        return 1;
    }
    const char* out = argv[1];
    std::string scene = "world";
    float seconds = 8.0f;
    uint64_t seed = 7;
    int zoom = 2;
    bool stats = false, log = false, reveal = false, wide = false, pile = false, smelly = false, vending = false,
         map = false, server = false, washroom = false, centreVending = false, pissed = false;
    for (int i = 2; i < argc; i++) {
        auto value = [&]() -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", argv[i]);
                std::exit(1);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--scene")) scene = value();
        else if (!std::strcmp(argv[i], "--seconds")) seconds = float(std::atof(value()));
        else if (!std::strcmp(argv[i], "--seed")) seed = std::strtoull(value(), nullptr, 10);
        else if (!std::strcmp(argv[i], "--zoom")) zoom = std::atoi(value());
        else if (!std::strcmp(argv[i], "--stats")) stats = true;
        else if (!std::strcmp(argv[i], "--log")) log = true;
        else if (!std::strcmp(argv[i], "--map")) map = true;
        else if (!std::strcmp(argv[i], "--reveal")) reveal = true;
        else if (!std::strcmp(argv[i], "--wide")) wide = true;
        else if (!std::strcmp(argv[i], "--pile")) pile = true;
        else if (!std::strcmp(argv[i], "--smells")) smelly = true;
        else if (!std::strcmp(argv[i], "--vending")) vending = true;
        else if (!std::strcmp(argv[i], "--server")) server = true;
        else if (!std::strcmp(argv[i], "--washroom")) washroom = true;
        else if (!std::strcmp(argv[i], "--centre-vending")) centreVending = true;
        else if (!std::strcmp(argv[i], "--pissed")) pissed = true;
        else {
            std::fprintf(stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow((wide ? Canvas::WIDE_W : Canvas::W) * 2, Canvas::H * 2, "shot");
    bool ok = false;
    {
        Canvas canvas;
        Rng rng(42);
        std::unique_ptr<Scene> s;
        WorldScene* world = nullptr;
        if (scene == "world") {
            auto w = std::make_unique<WorldScene>(HumanLook::random(rng), 99, seed);
            ShotProbe::neverRecord(*w);
            if (pile) ShotProbe::pile(*w);
            if (smelly) ShotProbe::smells(*w);
            if (vending) ShotProbe::vending(*w);
            if (server) ShotProbe::server(*w);
            if (washroom) ShotProbe::washroom(*w);
            if (centreVending) ShotProbe::centreVending(*w);
            if (pissed) ShotProbe::toilet(*w);
            world = w.get();
            s = std::move(w);
        } else if (scene == "menu") {
            s = std::make_unique<MenuScene>();
        } else if (scene == "create") {
            s = std::make_unique<CreateScene>(seed);
        } else if (scene == "obituary") {
            s = std::make_unique<ObituaryScene>();
        } else {
            std::fprintf(stderr, "unknown scene %s\n", scene.c_str());
        }
        if (s) {
            canvas.fit(s->canBeWide());
            for (int f = 0; f < int(seconds * 60.0f); f++) s->update(1.0f / 60.0f, canvas);
            if (world && pissed) ShotProbe::pissed(*world);
            if (world) ShotProbe::set(*world, zoom, stats, log, reveal, map);
            canvas.begin();
            s->draw(canvas);
            canvas.end();
            BeginDrawing();
            ClearBackground(BLACK);
            canvas.present();
            rlDrawRenderBatchActive();
            Image img = LoadImageFromScreen();
            EndDrawing();
            ok = ExportImage(img, out);
            UnloadImage(img);
            if (ok) std::printf("wrote %s\n", out);
        }
    }
    CloseWindow();
    return ok ? 0 : 1;
}
