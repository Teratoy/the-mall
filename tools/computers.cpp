// A look at the old computers and the cables on the floor. Top row: the
// terminal at a few ticks of its clock and the console desk, blown up, with
// a human for size. Middle row: every cable piece, blown up on the floor:
// the three straight looks across and down, the cross, the four corners and
// the four Ts. Bottom row: a small machine room wired up with them, the
// server rack in it, at 4x and at the size a phone shows it.
//
// Usage: build/computers [out.png] [seed]
// Build, after make: g++ -std=c++20 -Isrc tools/computers.cpp $(find build -path build/android -prune -o -name "*.o" ! -name main.o -print)
//                    -o build/computers $(pkg-config --libs raylib) -lm
#include "core/rng.h"
#include "human/human_look.h"
#include "render/cable_art.h"
#include "render/computer_art.h"
#include "render/server_rack_art.h"
#include "world/chunk.h"
#include <cstdio>
#include <cstdlib>
#include <raylib.h>
#include <vector>

namespace {

/// Blown up this many times for the machines, the cable pieces and the room; and as a phone shows the canvas.
constexpr int ZOOM = 8, CABLE_ZOOM = 10, ROOM_ZOOM = 4, PHONE = 2;
constexpr int GAP = 16;
constexpr Color BACKDROP{58, 56, 62, 255};
constexpr Rgba FLOOR = hex(0x8a8478), FLOOR_LINE = hex(0x7e786c), WALL = hex(0x4a4450);
/// The ticks the terminal is shown at.
constexpr int TICKS[] = {0, 7, 15};

/// The room, a letter a cell: M a machine's footprint or the chair, # cable, the top row the wall.
constexpr const char* ROOM[] = {
    "WWWWWWWWWWWWWW",
    ".MMMMMM.......",
    ".MMMMMM.......",
    "..MM.#...MM.M.",
    ".....#...#..#.",
    ".....#...#..#.",
    ".....####+####",
    ".........#....",
    ".........#....",
    ".........#....",
};
constexpr int ROOM_W = 14, ROOM_H = sizeof ROOM / sizeof ROOM[0];

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

/// Paints w x h pixels from px() with their top-left at (x, y), each one size screen pixels.
template <typename F> void blit(Image& img, int x, int y, int w, int h, int size, F px) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            Rgba c = px(i, j);
            if (c.solid()) ImageDrawRectangle(&img, x + i * size, y + j * size, size, size, toColor(c));
        }
}

/// The floor under a cable piece, with the cell's edge lines so the seams can be checked.
Rgba floorPx(int i, int j) { return i % CELL_PX == 0 || j % CELL_PX == 0 ? FLOOR_LINE : FLOOR; }

char roomAt(int x, int y) { return x < 0 || y < 0 || x >= ROOM_W || y >= int(ROOM_H) ? '#' : ROOM[y][x]; }

/// The edges a room cable leaves by: towards cable, a machine, or off the room's side.
uint8_t linksAt(int x, int y) {
    auto joins = [](char c) { return c == '#' || c == '+' || c == 'M'; };
    uint8_t links = 0;
    if (joins(roomAt(x, y - 1))) links |= CABLE_N;
    if (joins(roomAt(x + 1, y))) links |= CABLE_E;
    if (joins(roomAt(x, y + 1))) links |= CABLE_S;
    if (joins(roomAt(x - 1, y))) links |= CABLE_W;
    return links;
}

/// The room as a picture of canvas pixels, the machines at tick and the human beside the cross.
std::vector<Rgba> room(const SpriteFrame& human, int tick) {
    const int w = ROOM_W * CELL_PX, h = int(ROOM_H) * CELL_PX;
    std::vector<Rgba> px(size_t(w * h));
    auto put = [&](int x, int y, Rgba c) {
        if (c.solid() && x >= 0 && y >= 0 && x < w && y < h) px[size_t(y * w + x)] = c;
    };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) put(x, y, y < CELL_PX ? WALL : floorPx(x, y));
    for (int cy = 0; cy < int(ROOM_H); cy++)
        for (int cx = 0; cx < ROOM_W; cx++) {
            char c = ROOM[cy][cx];
            if (c != '#' && c != '+') continue;
            int variant = (cx * 7 + cy * 3) % CABLE_VARIANTS;
            for (int j = 0; j < CELL_PX; j++)
                for (int i = 0; i < CELL_PX; i++)
                    put(cx * CELL_PX + i, cy * CELL_PX + j, cablePixel(linksAt(cx, cy), variant, i, j));
        }
    auto stand = [&](int left, int foot, int gw, int gh, auto pixel) {
        for (int j = 0; j < gh; j++)
            for (int i = 0; i < gw; i++) put(left + i, foot - gh + j, pixel(i, j));
    };
    stand(CELL_PX, CELL_PX + CONSOLE_H - CONSOLE_RISE, CONSOLE_W, CONSOLE_H,
          [&](int i, int j) { return consoleDeskPixel(i, j, tick); });
    stand(9 * CELL_PX, 4 * CELL_PX, SERVER_RACK_W, SERVER_RACK_H,
          [&](int i, int j) { return serverRackPixel(i, j, tick); });
    stand(12 * CELL_PX, 4 * CELL_PX, TERMINAL_W, TERMINAL_H, [&](int i, int j) { return terminalPixel(i, j, tick); });
    stand(10 * CELL_PX - 4, 9 * CELL_PX, SPRITE_W, SPRITE_H,
          [&](int i, int j) { return human.px[size_t(j * SPRITE_W + i)]; });
    return px;
}

} // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "build/computers.png";
    Rng rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3);
    const SpriteFrame human = HumanLook::random(rng).idle();
    auto humanPx = [&](int i, int j) { return human.px[size_t(j * SPRITE_W + i)]; };

    struct Piece {
        uint8_t links;
        int variant;
    };
    std::vector<Piece> pieces;
    for (int v = 0; v < CABLE_VARIANTS; v++) pieces.push_back({CABLE_E | CABLE_W, v});
    for (int v = 0; v < CABLE_VARIANTS; v++) pieces.push_back({CABLE_N | CABLE_S, v});
    pieces.push_back({CABLE_N | CABLE_E | CABLE_S | CABLE_W, 0});
    for (uint8_t c : {CABLE_N | CABLE_E, CABLE_E | CABLE_S, CABLE_S | CABLE_W, CABLE_W | CABLE_N})
        pieces.push_back({c, 0});
    for (uint8_t c : {CABLE_N, CABLE_E, CABLE_S, CABLE_W}) pieces.push_back({uint8_t(15 & ~c), 0});

    const int piece = CELL_PX * CABLE_ZOOM;
    const int machinesH = CONSOLE_H * ZOOM, roomW = ROOM_W * CELL_PX, roomH = int(ROOM_H) * CELL_PX;
    const int width = GAP + int(pieces.size()) * (piece + GAP);
    const int cablesY = GAP + machinesH + GAP, roomY = cablesY + piece + 2 * GAP;
    Image img = GenImageColor(width, roomY + roomH * ROOM_ZOOM + GAP, BACKDROP);

    int x = GAP;
    for (int tick : TICKS) {
        blit(img, x, GAP + machinesH - TERMINAL_H * ZOOM, TERMINAL_W, TERMINAL_H, ZOOM,
             [&](int i, int j) { return terminalPixel(i, j, tick); });
        x += TERMINAL_W * ZOOM + GAP;
    }
    blit(img, x, GAP, CONSOLE_W, CONSOLE_H, ZOOM, [&](int i, int j) { return consoleDeskPixel(i, j, TICKS[1]); });
    x += CONSOLE_W * ZOOM + GAP;
    blit(img, x, GAP + machinesH - SPRITE_H * ZOOM, SPRITE_W, SPRITE_H, ZOOM, humanPx);

    for (size_t p = 0; p < pieces.size(); p++) {
        int px = GAP + int(p) * (piece + GAP);
        blit(img, px, cablesY, CELL_PX, CELL_PX, CABLE_ZOOM, floorPx);
        blit(img, px, cablesY, CELL_PX, CELL_PX, CABLE_ZOOM,
             [&](int i, int j) { return cablePixel(pieces[p].links, pieces[p].variant, i, j); });
    }

    const std::vector<Rgba> pic = room(human, TICKS[1]);
    auto roomPx = [&](int i, int j) { return pic[size_t(j * roomW + i)]; };
    blit(img, GAP, roomY, roomW, roomH, ROOM_ZOOM, roomPx);
    blit(img, GAP + roomW * ROOM_ZOOM + GAP, roomY, roomW, roomH, PHONE, roomPx);

    bool ok = ExportImage(img, out);
    UnloadImage(img);
    std::printf("%s %s\n", ok ? "saved" : "could not save", out);
    return ok ? 0 : 1;
}
