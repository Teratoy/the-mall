// Three server rooms laid out by hand, to judge how the machine room art
// sits together before any of it goes into the world: a closet, a room and
// a hall, at 4x and below at the size a phone shows them, each with a
// human for size. Nothing here is world generation; the rooms live only in
// the maps below.
//
// Usage: build/server_rooms [out.png] [seed] [seconds]
// Build, after make: g++ -std=c++20 -Isrc tools/server_rooms.cpp $(find build -path build/android -prune -o -name "*.o" ! -name main.o -print)
//                    -o build/server_rooms $(pkg-config --libs raylib) -lm
#include "core/rng.h"
#include "human/human_look.h"
#include "render/cable_art.h"
#include "render/computer_art.h"
#include "render/server_rack_art.h"
#include "world/chunk.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <raylib.h>
#include <vector>

namespace {

/// Blown up this many times for looking at, and as a phone shows the canvas.
constexpr int ZOOM = 4, PHONE = 2;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
/// The raised floor's tiles, their seams, and the dots of the perforated ones that blow cold air.
constexpr Rgba TILE = hex(0xa4a6aa), SEAM = hex(0x8a8c92), PERFORATION = hex(0x6a6c72);
/// The back wall's face, the line along its top, and the tops of the side and front walls.
constexpr Rgba WALL_FACE = hex(0x5a5462), WALL_EDGE = hex(0x3a3540), WALL_TOP = hex(0x2a2630);
/// The back wall stands this many rows tall.
constexpr int WALL_ROWS = 2;
/// The racks' clock at this many seconds in, unless told otherwise.
constexpr float SECONDS = 1.5f;

/// A room, a letter a cell. W wall, with a gap in the front wall for the
/// door. R a rack's left cell and r its right, t a terminal, D the console
/// desk's top-left cell and d the rest of it, all standing with their foot
/// in that row. # cable, v a perforated floor tile, H where a human stands.
struct Room {
    const char* name;
    std::vector<const char*> map;
};

const Room ROOMS[] = {
    {"closet 6x5",
     {
         "WWWWWWWW",
         "WWWWWWWW",
         "WRrRr.tW",
         "W#.#..#W",
         "W######W",
         "Wvvvvv#W",
         "W.H...#W",
         "WWW..WWW",
     }},
    {"room 12x9",
     {
         "WWWWWWWWWWWWWW",
         "WWWWWWWWWWWWWW",
         "WRrRrRrDdddddW",
         "W#.#.#.ddddddW",
         "W######ddddddW",
         "Wvv#vvvddddddW",
         "Wvv#vvv......W",
         "Wvv#vvv..H...W",
         "WRr#.Rrt.....W",
         "W#.#.#.#.....W",
         "W############W",
         "WWWWW..WWWWWWW",
     }},
    {"hall 20x14",
     {
         "WWWWWWWWWWWWWWWWWWWWWW",
         "WWWWWWWWWWWWWWWWWWWWWW",
         "WRrRrRrRr.Dddddd.RrRrW",
         "W#.#.#.#..dddddd.#.#.W",
         "W#########dddddd#####W",
         "Wvvvvvvvv#dddddd.....W",
         "Wvvvvvvvv#vvvvvvvvvvvW",
         "Wvvvvvvvv#vvvvvvvvvvvW",
         "W........#...........W",
         "W........#...........W",
         "WRrRrRrRr#.RrRrRrRrRrW",
         "W#.#.#.#.#.#.#.#.#.#.W",
         "W####################W",
         "W...#.#.....#........W",
         "W...t.t.....t...H....W",
         "W....................W",
         "WWWWWWWWWW..WWWWWWWWWW",
     }},
};

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

bool isCable(char c) { return c == '#'; }
bool isMachine(char c) { return c && std::strchr("RrtDd", c); }

/// The edges a cable leaves its cell by: towards the cables beside it, and
/// at the end of a run on into the machine or wall it reaches, straight on
/// if it can, else the first machine beside it, else the first wall.
uint8_t linksAt(const Room& room, int x, int y) {
    const int w = int(std::strlen(room.map[0])), h = int(room.map.size());
    auto at = [&](int cx, int cy) { return cx < 0 || cy < 0 || cx >= w || cy >= h ? 'W' : room.map[size_t(cy)][cx]; };
    constexpr uint8_t SIDES[4] = {CABLE_N, CABLE_E, CABLE_S, CABLE_W};
    constexpr int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    uint8_t links = 0;
    int cables = 0, last = 0;
    for (int i = 0; i < 4; i++)
        if (isCable(at(x + DX[i], y + DY[i]))) links |= SIDES[i], cables++, last = i;
    if (cables != 1) return links;
    int ahead = (last + 2) % 4;
    char c = at(x + DX[ahead], y + DY[ahead]);
    if (isMachine(c) || c == 'W') return links | SIDES[ahead];
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < 4; i++) {
            char n = at(x + DX[i], y + DY[i]);
            if (pass == 0 ? isMachine(n) : n == 'W') return links | SIDES[i];
        }
    return links;
}

/// The room as a picture of canvas pixels: floor and walls, the cables on
/// the floor, then the machines and the human, nearest the front drawn last.
std::vector<Rgba> draw(const Room& room, const SpriteFrame& human, int tick) {
    const int cw = int(std::strlen(room.map[0])), ch = int(room.map.size());
    const int w = cw * CELL_PX, h = ch * CELL_PX;
    std::vector<Rgba> px(size_t(w * h));
    auto put = [&](int x, int y, Rgba c) {
        if (c.solid() && x >= 0 && y >= 0 && x < w && y < h) px[size_t(y * w + x)] = c;
    };
    for (int cy = 0; cy < ch; cy++)
        for (int cx = 0; cx < cw; cx++) {
            char c = room.map[size_t(cy)][cx];
            for (int j = 0; j < CELL_PX; j++)
                for (int i = 0; i < CELL_PX; i++) {
                    Rgba ink = i == 0 || j == 0 ? SEAM : TILE;
                    if (c == 'v' && i % 2 == 0 && j % 2 == 0 && i && j) ink = PERFORATION;
                    if (c == 'W') ink = cy < WALL_ROWS ? (cy == 0 && j < 2 ? WALL_EDGE : WALL_FACE) : WALL_TOP;
                    put(cx * CELL_PX + i, cy * CELL_PX + j, ink);
                }
            if (isCable(c))
                for (int j = 0; j < CELL_PX; j++)
                    for (int i = 0; i < CELL_PX; i++)
                        put(cx * CELL_PX + i, cy * CELL_PX + j, cablePixel(linksAt(room, cx, cy), (cx * 7 + cy * 3) % 3, i, j));
        }

    struct Thing {
        int foot, x, y;
        char kind;
    };
    std::vector<Thing> things;
    for (int cy = 0; cy < ch; cy++)
        for (int cx = 0; cx < cw; cx++) {
            char c = room.map[size_t(cy)][cx];
            if (c == 'D') things.push_back({(cy + CONSOLE_CELLS_D) * CELL_PX, cx, cy, c});
            else if (c == 'R' || c == 't' || c == 'H') things.push_back({(cy + 1) * CELL_PX, cx, cy, c});
        }
    std::stable_sort(things.begin(), things.end(), [](const Thing& a, const Thing& b) { return a.foot < b.foot; });
    auto stand = [&](int left, int foot, int gw, int gh, auto pixel) {
        for (int j = 0; j < gh; j++)
            for (int i = 0; i < gw; i++) put(left + i, foot - gh + j, pixel(i, j));
    };
    for (const Thing& t : things) {
        const int left = t.x * CELL_PX;
        switch (t.kind) {
        case 'D':
            stand(left, t.foot, CONSOLE_W, CONSOLE_H, [&](int i, int j) { return consoleDeskPixel(i, j, tick); });
            break;
        case 'R':
            stand(left, t.foot, SERVER_RACK_W, SERVER_RACK_H,
                  [&](int i, int j) { return serverRackPixel(i, j, tick + t.x * 5); });
            break;
        case 't':
            stand(left, t.foot, TERMINAL_W, TERMINAL_H, [&](int i, int j) { return terminalPixel(i, j, tick + t.x); });
            break;
        default:
            stand(left + (CELL_PX - SPRITE_W) / 2, t.foot, SPRITE_W, SPRITE_H,
                  [&](int i, int j) { return human.px[size_t(j * SPRITE_W + i)]; });
        }
    }
    return px;
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/server_rooms.png";
    Rng rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3);
    const float seconds = argc > 3 ? std::strtof(argv[3], nullptr) : SECONDS;
    const SpriteFrame human = HumanLook::random(rng).idle();
    const int tick = int(seconds * 8.0f);

    int width = GAP, tallest = 0;
    for (const Room& r : ROOMS) {
        width += int(std::strlen(r.map[0])) * CELL_PX * ZOOM + GAP;
        tallest = std::max(tallest, int(r.map.size()) * CELL_PX);
    }
    const int phoneY = GAP + tallest * ZOOM + GAP;
    Image img = GenImageColor(width, phoneY + tallest * PHONE + GAP, BACKDROP);
    int x = GAP, phoneX = GAP;
    for (const Room& r : ROOMS) {
        const int w = int(std::strlen(r.map[0])) * CELL_PX, h = int(r.map.size()) * CELL_PX;
        const std::vector<Rgba> pic = draw(r, human, tick);
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                Rgba c = pic[size_t(j * w + i)];
                ImageDrawRectangle(&img, x + i * ZOOM, GAP + j * ZOOM, ZOOM, ZOOM, toColor(c));
                ImageDrawRectangle(&img, phoneX + i * PHONE, phoneY + j * PHONE, PHONE, PHONE, toColor(c));
            }
        x += w * ZOOM + GAP;
        phoneX += w * PHONE + GAP;
    }
    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
