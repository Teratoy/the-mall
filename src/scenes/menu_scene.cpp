#include "scenes/menu_scene.h"
#include "render/canvas.h"
#include "scenes/create_scene.h"
#include "scenes/obituary_scene.h"
#include "ui/pixel_ui.h"
#include <algorithm>
#include <ctime>

namespace {

/// The name, in the default font at three times the UI's size.
constexpr const char* TITLE = "THE MALL";
constexpr float TITLE_SIZE = 30.0f, TITLE_SPACING = 3.0f;
constexpr int TITLE_Y = 150;
/// The dark plate behind the name, so the maze doesn't run through it.
constexpr int TITLE_PAD = 6;
constexpr int BUTTON_W = 120, BUTTON_H = 24, BUTTON_GAP = 12;
constexpr int BUTTONS_Y = 260;
constexpr Rectangle ENTER{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y, BUTTON_W, BUTTON_H};
constexpr Rectangle OBITUARY{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y + BUTTON_H + BUTTON_GAP, BUTTON_W, BUTTON_H};

/// The maze is drawn in 6 px cells, walls as thick as corridors. Its
/// width and height are odd, so walls ring it; what's left over of the
/// canvas is split evenly around it.
constexpr int MAZE_CELL = 6;
constexpr int MAZE_W = (Canvas::W / MAZE_CELL - 1) | 1, MAZE_H = (Canvas::H / MAZE_CELL - 1) | 1;
constexpr int MAZE_X = (Canvas::W - MAZE_W * MAZE_CELL) / 2, MAZE_Y = (Canvas::H - MAZE_H * MAZE_CELL) / 2;
/// Carving steps a second, forward or back.
constexpr float CARVE_RATE = 40.0f;
/// A dug cell stands this long, then fades over MAZE_FADE back into wall.
constexpr float MAZE_LIFE = 10.0f, MAZE_FADE = 4.0f;
/// Corridors just lighter than the paper, so the maze stays behind; the
/// carving end in the ground's grey.
constexpr Color MAZE_FLOOR = {38, 38, 38, 255};
constexpr Color MAZE_HEAD = ui::GROUND;

} // namespace

MenuScene::MenuScene() : mazeRng_(uint64_t(std::time(nullptr))) { startMaze(); }

void MenuScene::startMaze() {
    dugAt_.assign(size_t(MAZE_W * MAZE_H), -1.0f);
    // Corridor cells sit on odd columns and rows.
    int x = 2 * mazeRng_.range(0, MAZE_W / 2 - 1) + 1, y = 2 * mazeRng_.range(0, MAZE_H / 2 - 1) + 1;
    trail_.assign(1, y * MAZE_W + x);
    dig(trail_.back());
}

void MenuScene::dig(int cell) { dugAt_[size_t(cell)] = clock_; }

bool MenuScene::isOpen(int cell) const {
    float at = dugAt_[size_t(cell)];
    return at >= 0.0f && clock_ - at < MAZE_LIFE + MAZE_FADE;
}

// The recursive backtracker: knock through to a random cell two away that
// is wall, or step back along the trail when there is none. Going back
// digs the way afresh. Where the way back has faded, it waits where it is
// until the walls around fade into room to carve.
void MenuScene::carveStep() {
    int at = trail_.back(), x = at % MAZE_W, y = at / MAZE_W;
    dig(at);
    constexpr int DX[] = {2, -2, 0, 0}, DY[] = {0, 0, 2, -2};
    int ways[4], n = 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], ny = y + DY[d];
        if (nx > 0 && ny > 0 && nx < MAZE_W - 1 && ny < MAZE_H - 1 && !isOpen(ny * MAZE_W + nx)) ways[n++] = d;
    }
    if (n == 0) {
        if (trail_.size() < 2) return;
        int back = trail_[trail_.size() - 2];
        if (!isOpen(back)) {
            trail_.assign(1, at);
            return;
        }
        dig((at + back) / 2);
        trail_.pop_back();
        dig(back);
        return;
    }
    int d = ways[mazeRng_.range(0, n - 1)];
    dig((y + DY[d] / 2) * MAZE_W + x + DX[d] / 2);
    trail_.push_back((y + DY[d]) * MAZE_W + x + DX[d]);
    dig(trail_.back());
}

void MenuScene::update(float dt, const Canvas&) {
    clock_ += dt;
    for (carveDebt_ += dt * CARVE_RATE; carveDebt_ >= 1.0f; carveDebt_ -= 1.0f) carveStep();
    if (IsKeyPressed(KEY_ENTER)) enter_ = true;
    if (enter_) next = std::make_unique<CreateScene>(uint64_t(std::time(nullptr)));
    else if (obituary_) next = std::make_unique<ObituaryScene>();
}

void MenuScene::drawMaze() const {
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++) {
            int cell = y * MAZE_W + x;
            if (!isOpen(cell)) continue;
            float fade = std::max(0.0f, clock_ - dugAt_[size_t(cell)] - MAZE_LIFE) / MAZE_FADE;
            DrawRectangle(MAZE_X + x * MAZE_CELL, MAZE_Y + y * MAZE_CELL, MAZE_CELL, MAZE_CELL,
                          ColorLerp(MAZE_FLOOR, ui::PAPER, fade));
        }
    int at = trail_.back();
    DrawRectangle(MAZE_X + at % MAZE_W * MAZE_CELL, MAZE_Y + at / MAZE_W * MAZE_CELL, MAZE_CELL, MAZE_CELL, MAZE_HEAD);
}

// Buttons report clicks while drawing; they act on the next update.
void MenuScene::draw(const Canvas& canvas) {
    ClearBackground(ui::PAPER);
    drawMaze();
    Font font = GetFontDefault();
    Vector2 size = MeasureTextEx(font, TITLE, TITLE_SIZE, TITLE_SPACING);
    float titleX = (Canvas::W - size.x) / 2;
    DrawRectangle(int(titleX) - TITLE_PAD, TITLE_Y - TITLE_PAD, int(size.x) + 2 * TITLE_PAD,
                  int(size.y) + 2 * TITLE_PAD, ui::PAPER);
    DrawTextEx(font, TITLE, {titleX, float(TITLE_Y)}, TITLE_SIZE, TITLE_SPACING, ui::BRIGHT);
    Vector2 m = canvas.mouse();
    enter_ |= ui::button(ENTER, "ENTER THE MALL", m);
    obituary_ |= ui::button(OBITUARY, "OBITUARY", m);
}
