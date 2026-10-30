#include "scenes/obituary_scene.h"
#include "render/canvas.h"
#include "scenes/menu_scene.h"
#include "ui/pixel_ui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {

constexpr int MARGIN = 4;
/// BACK, small in the corner, and the title across the top.
constexpr Rectangle BACK{MARGIN, MARGIN, 32, 14};
/// CREMATE, in the other corner of a record page.
constexpr Rectangle CREMATE{Canvas::W - MARGIN - 52, MARGIN, 52, 14};
constexpr int TITLE_Y = MARGIN + 2;
/// What scrolls lies between the top and the bottom margin.
constexpr int TOP = MARGIN + 14 + MARGIN * 2, BOTTOM = Canvas::H - MARGIN;
/// The list: a button per human, a gap apart, inset from the sides.
constexpr int ROW_H = 18, ROW_GAP = 3, ROW_X = 8, ROW_PAD = 4;
/// On a record page: label rows, a gap between parts, and the log's time
/// column as wide as its widest time.
constexpr int PART_GAP = 8, LOG_TIME_GAP = 4;
/// A press that moves this far is a drag, not a tap.
constexpr float DRAG_SLOP = 3.0f;

std::string caps(std::string s) {
    for (char& c : s) c = char(std::toupper(uint8_t(c)));
    return s;
}

/// The wall-clock day a run ended.
std::string dayText(std::time_t when) {
    char s[16];
    std::tm* t = std::localtime(&when);
    if (!t || !std::strftime(s, sizeof s, "%Y-%m-%d", t)) return "";
    return s;
}

} // namespace

ObituaryScene::ObituaryScene() : records_(obituary::load()) {}

void ObituaryScene::update(float, const Canvas& canvas) {
    if (ui::backPressed()) back_ = true;
    if (cremate_) {
        cremate_ = false;
        if (!confirming_) {
            confirming_ = true;
        } else if (open_ >= 0) {
            records_.erase(records_.begin() + open_);
            obituary::save(records_);
            back_ = true;
        }
    }
    if (back_) {
        back_ = false;
        confirming_ = false;
        if (open_ < 0) {
            next = std::make_unique<MenuScene>();
            return;
        }
        open_ = -1;
        scroll_ = 0.0f;
    }
    if (tapped_ >= 0) {
        open_ = tapped_;
        tapped_ = -1;
        scroll_ = 0.0f;
    }
    Vector2 m = canvas.mouse();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.y >= TOP) {
        dragging_ = true;
        dragged_ = false;
        dragFrom_ = m.y;
        dragScroll_ = scroll_;
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragging_ = false;
    if (dragging_) {
        if (std::fabs(m.y - dragFrom_) > DRAG_SLOP) dragged_ = true;
        if (dragged_) scroll_ = dragScroll_ + (dragFrom_ - m.y);
    }
    scroll_ -= GetMouseWheelMove() * ui::ROW * 3;
    scroll_ = std::clamp(scroll_, 0.0f, maxScroll_);
}

// Buttons report clicks while drawing; they act on the next update.
void ObituaryScene::draw(const Canvas& canvas) {
    ClearBackground(BLACK);
    Vector2 m = canvas.mouse();
    back_ |= ui::button(BACK, "BACK", m);
    if (open_ >= 0) {
        cremate_ |= ui::button(CREMATE, confirming_ ? "SURE?" : "CREMATE", m);
        // A tap anywhere else takes the question back.
        if (confirming_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(m, CREMATE))
            confirming_ = false;
        drawRecord(records_[size_t(open_)]);
        return;
    }
    ui::textCentred("OBITUARY", Canvas::W / 2, TITLE_Y, ui::BRIGHT);
    drawList(m);
}

// Only rows wholly in view are drawn, so nothing runs under the title.
void ObituaryScene::drawList(Vector2 mouse) {
    if (records_.empty()) {
        maxScroll_ = 0.0f;
        ui::textCentred("NO ONE HAS DIED YET", Canvas::W / 2, TOP + MARGIN, ui::DIM);
        return;
    }
    int n = int(records_.size());
    maxScroll_ = std::max(0.0f, float(n * (ROW_H + ROW_GAP) - ROW_GAP - (BOTTOM - TOP)));
    // A drag's release must not open whatever it ends on.
    Vector2 tap = dragged_ ? Vector2{-1.0f, -1.0f} : mouse;
    for (int i = 0; i < n; i++) {
        float y = TOP + i * (ROW_H + ROW_GAP) - scroll_;
        if (y < TOP || y + ROW_H > BOTTOM) continue;
        int index = n - 1 - i;
        const obituary::Record& r = records_[size_t(index)];
        Rectangle row{ROW_X, y, Canvas::W - 2 * ROW_X, ROW_H};
        if (ui::button(row, "", tap)) tapped_ = index;
        bool over = CheckCollisionPointRec(tap, row), held = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        Color ink = held ? ui::PAPER : over ? WHITE : ui::INK;
        int ty = int(y + (ROW_H - 10) / 2) + (held ? 1 : 0);
        ui::text(caps(r.name).c_str(), int(row.x) + ROW_PAD, ty, ink);
        ui::textRight(ui::clockText(r.survived).c_str(), int(row.x + row.width) - ROW_PAD, ty, held ? ui::PAPER : ui::DIM);
    }
}

// The name, how long they lasted, what ended them and when, then their
// log newest first, all scrolling together under the BACK button.
void ObituaryScene::drawRecord(const obituary::Record& r) {
    int left = MARGIN * 2, right = Canvas::W - MARGIN * 2;
    int y = TOP - int(scroll_);
    auto visible = [&](int at) { return at >= TOP && at + ui::ROW <= BOTTOM; };
    auto labelRow = [&](const char* label, const std::string& value) {
        if (visible(y)) {
            ui::text(label, left, y, ui::DIM);
            ui::textRight(value.c_str(), right, y, ui::BRIGHT);
        }
        y += ui::ROW;
    };
    std::string name = caps(r.name);
    if (visible(y)) ui::textCentred(name.c_str(), Canvas::W / 2, y, ui::BRIGHT);
    y += ui::ROW + PART_GAP;
    labelRow("SURVIVED", ui::clockText(r.survived));
    labelRow("ENDED", dayText(r.when));
    if (visible(y)) ui::text("CAUSE", left, y, ui::DIM);
    y += ui::ROW;
    for (const std::string& row : ui::wrap(r.cause, right - left)) {
        if (visible(y)) ui::text(row.c_str(), left, y, ui::BRIGHT);
        y += ui::ROW;
    }
    y += PART_GAP;
    if (visible(y)) ui::text("LOG", left, y, ui::DIM);
    y += ui::ROW;
    // The latest time is the longest; measured in zeros, as 1 is narrower.
    std::string widest = r.log.empty() ? "0:00" : ui::clockText(r.log.back().time);
    for (char& c : widest)
        if (std::isdigit(static_cast<unsigned char>(c))) c = '0';
    int column = ui::textWidth(widest.c_str()) + LOG_TIME_GAP;
    for (auto it = r.log.rbegin(); it != r.log.rend(); ++it) {
        std::vector<std::string> rows = ui::wrap(it->what, right - left - column);
        if (visible(y)) ui::text(ui::clockText(it->time).c_str(), left, y, ui::DIM);
        for (const std::string& row : rows) {
            if (visible(y)) ui::text(row.c_str(), left + column, y);
            y += ui::ROW;
        }
    }
    maxScroll_ = std::max(0.0f, float(y + int(scroll_) - BOTTOM));
}
