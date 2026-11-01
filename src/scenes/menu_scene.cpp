#include "scenes/menu_scene.h"
#include "human/human.h"
#include "render/canvas.h"
#include "render/world_view.h"
#include "scenes/create_scene.h"
#include "scenes/obituary_scene.h"
#include "ui/pixel_ui.h"
#include <ctime>

namespace {

/// The name, in the default font at three times the UI's size.
constexpr const char* TITLE = "THE MALL";
constexpr float TITLE_SIZE = 30.0f, TITLE_SPACING = 3.0f;
constexpr int TITLE_Y = 150;
/// The name is outlined this thick in black, so it reads over the mall.
constexpr int TITLE_OUTLINE = 2;
constexpr int BUTTON_W = 120, BUTTON_H = 24, BUTTON_GAP = 12;
constexpr int BUTTONS_Y = 260;
constexpr Rectangle ENTER{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y, BUTTON_W, BUTTON_H};
constexpr Rectangle OBITUARY{(Canvas::W - BUTTON_W) / 2.0f, BUTTONS_Y + BUTTON_H + BUTTON_GAP, BUTTON_W, BUTTON_H};

/// The mall behind is shown at the world's own scale, scrolling south this
/// many world pixels a second, and grown this many cells ahead of the view.
constexpr float MALL_ZOOM = 1.0f, MALL_SPEED = 6.0f;
constexpr int MALL_AHEAD = CHUNK;
/// Laid over the mall, so the name and buttons read over it.
constexpr Color MALL_SHADE = {0, 0, 0, 110};

} // namespace

// The mall is grown from its spawn room, whose human stays hidden.
MenuScene::MenuScene() : world_(uint64_t(std::time(nullptr))) {
    Rng looks(uint64_t(std::time(nullptr)));
    const Human& h = world_.makeSpawn(HumanLook::random(looks), looks.next());
    focus_ = {(h.cell.x + 0.5f) * CELL_PX, (h.cell.y + 0.5f) * CELL_PX};
}

void MenuScene::update(float dt, const Canvas&) {
    focus_.y += MALL_SPEED * dt;
    // Down the view's middle and both edges, a chunk ahead of its bottom.
    const int below = int(focus_.y / CELL_PX) + MALL_AHEAD;
    const int half = int(Canvas::W / MALL_ZOOM / CELL_PX / 2) + 1, mid = int(focus_.x / CELL_PX);
    for (int x : {mid - half, mid, mid + half})
        for (int y = int(focus_.y / CELL_PX); y <= below; y += CHUNK / 2) world_.ensureGenerated({x, y});
    if (IsKeyPressed(KEY_ENTER)) enter_ = true;
    if (enter_) next = std::make_unique<CreateScene>(uint64_t(std::time(nullptr)));
    else if (obituary_) next = std::make_unique<ObituaryScene>();
}

// Buttons report clicks while drawing; they act on the next update.
void MenuScene::draw(const Canvas& canvas) {
    ClearBackground(ui::PAPER);
    const Human& hidden = world_.human(0);
    drawWorld(world_, hidden, {}, floorTiles_, statueArt_, true, MALL_ZOOM, {0, 0, Canvas::W, Canvas::H}, focus_,
              false);
    DrawRectangle(0, 0, Canvas::W, Canvas::H, MALL_SHADE);
    Font font = GetFontDefault();
    Vector2 size = MeasureTextEx(font, TITLE, TITLE_SIZE, TITLE_SPACING);
    float titleX = (Canvas::W - size.x) / 2;
    for (int dy = -TITLE_OUTLINE; dy <= TITLE_OUTLINE; dy++)
        for (int dx = -TITLE_OUTLINE; dx <= TITLE_OUTLINE; dx++)
            DrawTextEx(font, TITLE, {titleX + float(dx), float(TITLE_Y + dy)}, TITLE_SIZE, TITLE_SPACING, BLACK);
    DrawTextEx(font, TITLE, {titleX, float(TITLE_Y)}, TITLE_SIZE, TITLE_SPACING, ui::BRIGHT);
    Vector2 m = canvas.mouse();
    enter_ |= ui::button(ENTER, "ENTER THE MALL", m);
    obituary_ |= ui::button(OBITUARY, "OBITUARY", m);
}
