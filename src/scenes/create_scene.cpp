#include "scenes/create_scene.h"
#include "human/name.h"
#include "render/canvas.h"
#include "scenes/menu_scene.h"
#include "scenes/world_scene.h"
#include "ui/pixel_ui.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace {

constexpr int PREVIEW_SCALE = 5;
/// Seconds the preview stands, walks, waves, sits, gets up, falls asleep
/// and wakes.
constexpr float PHASE_TIME[] = {2.0f, 3.0f, 1.5f, 3.5f, 0.5f, 9.0f, ROUSE_TIME + STAND_UP_TIME + 0.3f};
constexpr Pose PHASE_POSE[] = {Pose::Idle, Pose::Walk, Pose::Wave, Pose::Sit, Pose::Rise, Pose::Sleep, Pose::WakeUp};
constexpr int PHASES = int(std::size(PHASE_POSE));

constexpr int MARGIN = 8;
constexpr int TITLE_Y = 48;
/// The stage the preview stands on, and where it stands at world size.
constexpr Rectangle PANEL{36, 72, 144, 150};
constexpr int NAME_Y = 232;
/// The name is a field to type in: a box round it to tap, a hint under it,
/// and at most NAME_LONGEST characters.
constexpr Rectangle NAME_FIELD{36, NAME_Y - 4, 144, 17};
constexpr int HINT_Y = 248;
constexpr size_t NAME_LONGEST = 14;
/// The caret blinks on and off this often.
constexpr float CARET_BLINK = 0.5f;
constexpr Rectangle BACK{MARGIN, MARGIN, 32, 14};
constexpr int SMALL_Y = 280;
/// The traits, each a label and a bar of TRAIT_PIPS, one under the other.
constexpr int TRAITS_Y = 308, TRAIT_ROW = 16;
constexpr int TRAIT_LABEL_X = 40, TRAIT_BAR_X = 108, TRAIT_BAR_RIGHT = Canvas::W - TRAIT_LABEL_X;
constexpr int TRAIT_PIPS = 9, TRAIT_BAR_H = 6;
/// Two buttons across the bottom, with the same margin everywhere.
constexpr int BUTTON_H = 24;
constexpr int BUTTON_W = (Canvas::W - 3 * MARGIN) / 2;
constexpr Rectangle REFRESH{MARGIN, Canvas::H - MARGIN - BUTTON_H, BUTTON_W, BUTTON_H};
constexpr Rectangle CONTINUE{Canvas::W - MARGIN - BUTTON_W, Canvas::H - MARGIN - BUTTON_H, BUTTON_W, BUTTON_H};

} // namespace

CreateScene::CreateScene(uint64_t seed) : rng_(seed) { roll(); }

void CreateScene::roll() {
    look_ = HumanLook::random(rng_);
    humanSeed_ = rng_.next();
    name_ = randomName(humanSeed_);
    traits_ = Traits::random(humanSeed_);
    sprites_ = std::make_unique<HumanSprites>(look_);
    showTime_ = 0.0f;
    phase_ = 0;
}

Pose CreateScene::pose() const { return PHASE_POSE[phase_]; }

void CreateScene::update(float dt, const Canvas&) {
    showTime_ += dt;
    if (showTime_ >= PHASE_TIME[phase_]) {
        showTime_ = 0.0f;
        phase_ = (phase_ + 1) % PHASES;
        if (phase_ == 0) facingLeft_ = !facingLeft_;
    }
    if (startEdit_) {
        startEdit_ = false;
        editing_ = true;
        caret_ = 0.0f;
    }
    if (editing_) {
        caret_ += dt;
        // Any printable character but @ and #, which logs keep for names.
        for (int ch = GetCharPressed(); ch; ch = GetCharPressed())
            if (ch >= 32 && ch < 127 && ch != '@' && ch != '#' && chosen_.size() < NAME_LONGEST &&
                !(ch == ' ' && chosen_.empty()))
                chosen_ += char(ch);
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !chosen_.empty()) chosen_.pop_back();
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) editing_ = false;
        if (!editing_)
            while (!chosen_.empty() && chosen_.back() == ' ') chosen_.pop_back();
    } else {
        if (IsKeyPressed(KEY_R)) refresh_ = true;
        if (IsKeyPressed(KEY_ENTER)) proceed_ = true;
        if (IsKeyPressed(KEY_ESCAPE)) back_ = true;
    }
    if (back_) {
        next = std::make_unique<MenuScene>();
        return;
    }
    if (refresh_) {
        refresh_ = false;
        roll();
    }
    if (proceed_) next = std::make_unique<WorldScene>(look_, humanSeed_, rng_.next(), chosen_);
}

// Buttons report clicks while drawing; they act on the next update.
void CreateScene::draw(const Canvas& canvas) {
    ClearBackground(ui::PAPER);
    ui::textCentred("NEW HUMAN", Canvas::W / 2, TITLE_Y, ui::BRIGHT);
    DrawRectangleRec(PANEL, ui::GROUND);
    Vector2 feet{PANEL.x + PANEL.width / 2, PANEL.y + PANEL.height - 16};
    sprites_->draw(feet, pose(), showTime_, facingLeft_, PREVIEW_SCALE);
    // The name to tap and type over, then the human again at the size it
    // will walk the world.
    Vector2 m = canvas.mouse();
    bool overName = CheckCollisionPointRec(m, NAME_FIELD);
    if (editing_ || overName) DrawRectangleRec(NAME_FIELD, ui::GROUND);
    std::string shown = chosen_.empty() && !editing_ ? name_ : chosen_;
    for (char& c : shown) c = char(std::toupper(uint8_t(c)));
    ui::textCentred(shown.c_str(), Canvas::W / 2, NAME_Y, overName || editing_ ? WHITE : ui::BRIGHT);
    if (editing_) {
        if (std::fmod(caret_, 2 * CARET_BLINK) < CARET_BLINK)
            DrawRectangle(Canvas::W / 2 + ui::textWidth(shown.c_str()) / 2 + 1, NAME_Y - 1, 1, 10, WHITE);
        ui::textCentred("TYPE A NAME, ENTER WHEN DONE", Canvas::W / 2, HINT_Y, ui::DIM);
    } else {
        ui::textCentred("TAP THE NAME TO CHANGE IT", Canvas::W / 2, HINT_Y, ui::DIM);
    }
    // A press on the name starts typing; one anywhere else ends it.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (overName && !editing_) startEdit_ = true;
        if (!overName && editing_) {
            editing_ = false;
            while (!chosen_.empty() && chosen_.back() == ' ') chosen_.pop_back();
        }
    }
    sprites_->draw({float(Canvas::W / 2), float(SMALL_Y)}, pose(), showTime_, facingLeft_);
    // What they are like, 0 to 9 pips each, as the HUD will count them.
    const char* labels[] = {"BRAVE", "GREEDY", "CURIOUS", "SOCIAL"};
    const float values[] = {traits_.brave, traits_.greedy, traits_.curious, traits_.sociable};
    for (int i = 0; i < 4; i++) {
        int y = TRAITS_Y + i * TRAIT_ROW;
        ui::text(labels[i], TRAIT_LABEL_X, y);
        int pips = int(values[i] * 9.99f), by = y + 2;
        int step = (TRAIT_BAR_RIGHT - TRAIT_BAR_X) / TRAIT_PIPS;
        for (int p = 0; p < TRAIT_PIPS; p++)
            DrawRectangle(TRAIT_BAR_X + p * step, by, step - 1, TRAIT_BAR_H, p < pips ? ui::INK : ui::GROUND);
    }
    back_ |= ui::button(BACK, "BACK", m);
    refresh_ |= ui::button(REFRESH, "REFRESH", m);
    proceed_ |= ui::button(CONTINUE, "CONTINUE", m);
}
