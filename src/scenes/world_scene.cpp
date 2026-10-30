#include "scenes/world_scene.h"
#include "human/human.h"
#include "human/obituary.h"
#include "human/talk.h"
#include "render/canvas.h"
#include "render/map_view.h"
#include "render/speech_view.h"
#include "render/world_view.h"
#include "scenes/menu_scene.h"
#include "ui/pixel_ui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <string>
#include <vector>

namespace {

/// Longest slice of time simulated at once, so fast speeds stay correct.
constexpr float MAX_TICK = 1.0f / 30.0f;
constexpr int SPEEDS[] = {1, 2, 4, 8, 16};
constexpr int SPEED_COUNT = sizeof SPEEDS / sizeof SPEEDS[0];
/// Canvas pixels per world pixel. Whole steps in keep the pixels square;
/// zoomed out, some world pixels are skipped.
constexpr float ZOOMS[] = {0.5f, 1.0f, 2.0f, 3.0f};
constexpr int ZOOM_COUNT = sizeof ZOOMS / sizeof ZOOMS[0];

// The HUD is two shaded strips, laid out from measured text so nothing is
// cut off: the top one about the followed human and the world, the bottom
// one their bars and what they are doing. Buttons stand in a column at the
// right edge between them. The human's stats are a menu of their own,
// opened by tapping their name.
constexpr int MARGIN = 4;
constexpr int RIGHT = Canvas::W - MARGIN;
/// The world's size; then the name with the LOG button beside it, and
/// items, centred on a row as tall as the button.
constexpr int NAME_ROW_Y = MARGIN + ui::ROW, NAME_ROW_H = 16;
constexpr int TOP_H = NAME_ROW_Y + NAME_ROW_H + MARGIN / 2;
/// Text on the name row, centred on it.
constexpr int NAME_TEXT_Y = NAME_ROW_Y + 3;
/// Health across the whole strip, the needs on a row under it, then up
/// to two rows of what they are doing.
constexpr int BOTTOM_H = 48 + ui::ROW;
constexpr int BOTTOM_Y = Canvas::H - BOTTOM_H;
constexpr int BAR_Y = BOTTOM_Y + 6, BAR_H = 6;
constexpr int NEEDS_Y = BAR_Y + ui::ROW;
/// Where the rows of what they are doing may go, from the bars down.
constexpr int DOING_TOP = NEEDS_Y + BAR_H + 4, DOING_BOTTOM = Canvas::H - MARGIN;
constexpr Color HEALTH_BAR = {196, 64, 64, 255};
constexpr Color HUNGER_BAR = {214, 150, 62, 255};
constexpr Color THIRST_BAR = {78, 136, 204, 255};
constexpr Color TIRED_BAR = {150, 122, 196, 255};
constexpr Color REVEAL_INK = {232, 197, 71, 255};
/// The mind row, once erratic.
constexpr Color ERRATIC_INK = {214, 90, 90, 255};
/// Items on the top row: a 6px icon, a gap, the count.
constexpr int ICON_PX = 6, ICON_GAP = 2, ITEM_GAP = 8;

constexpr int BUTTON_W = 26, BUTTON_H = 16, BUTTON_GAP = 3;
constexpr Rectangle buttonAt(int i) {
    return {float(RIGHT - BUTTON_W), float(TOP_H + MARGIN + i * (BUTTON_H + BUTTON_GAP)), float(BUTTON_W),
            float(BUTTON_H)};
}
constexpr Rectangle ZOOM_IN_BUTTON = buttonAt(0);
constexpr Rectangle ZOOM_OUT_BUTTON = buttonAt(1);
constexpr Rectangle SPEED_UP_BUTTON = buttonAt(2);
/// The current speed, between its arrows.
constexpr Rectangle SPEED_SLOT = buttonAt(3);
constexpr Rectangle SPEED_DOWN_BUTTON = buttonAt(4);
constexpr Rectangle PAUSE_BUTTON = buttonAt(5);
/// The pause menu: its title and two buttons down the middle, over the
/// whole screen darkened.
constexpr int PAUSE_W = 100, PAUSE_H = 24, PAUSE_GAP = 10;
constexpr int PAUSE_TITLE_Y = Canvas::H / 2 - 50;
constexpr Rectangle RESUME_BUTTON{(Canvas::W - PAUSE_W) / 2.0f, Canvas::H / 2.0f - 20, PAUSE_W, PAUSE_H};
constexpr Rectangle MENU_BUTTON{(Canvas::W - PAUSE_W) / 2.0f, Canvas::H / 2.0f - 20 + PAUSE_H + PAUSE_GAP, PAUSE_W,
                                PAUSE_H};
constexpr Color PAUSE_SHADE = {0, 0, 0, 180};
/// The open log covers the world between the strips, left of the buttons,
/// darker than the strips so it reads over anything.
constexpr Rectangle LOG_AREA{0, float(TOP_H), float(RIGHT - BUTTON_W - MARGIN), float(BOTTOM_Y - TOP_H)};
constexpr Color LOG_SHADE = {0, 0, 0, 210};
/// Room between an entry's time and what happened.
constexpr int LOG_TIME_GAP = 4;
/// Stats are rows of a label and either a count or a bar of STAT_PIPS,
/// in titled sections a gap apart. The bars start STAT_LABEL_GAP past the
/// widest label, STAT_WIDEST.
constexpr int STAT_PIPS = 9, STAT_BAR_H = 6, STAT_LABEL_GAP = 6, SECTION_GAP = 6;
constexpr const char* STAT_WIDEST = "NEAR WATER";
/// Under the name, so it reads as something to tap.
constexpr int NAME_UNDERLINE = 9;
/// The LOG button, this far right of the name.
constexpr int LOG_GAP = 6;
/// The map's canvas pixels per cell, stepped with + and - while it is open.
constexpr int MAP_ZOOMS[] = {1, 2, 3, 4};
constexpr int MAP_ZOOM_COUNT = int(std::size(MAP_ZOOMS));
/// Shown once the followed human is dead, just above the bottom strip.
constexpr Rectangle NEXT_BUTTON{Canvas::W / 2 - 24, BOTTOM_Y - MARGIN - 16, 48, 16};

std::string caps(std::string s) {
    for (char& c : s) c = char(std::toupper(uint8_t(c)));
    return s;
}

/// What h is doing, naming whoever it is dealing with only if h was ever
/// told their name; a nameless one is just SOMEONE.
std::string doing(const Human& h) {
    int o = h.counterpart();
    std::string who = "SOMEONE";
    if (const std::string* n = o >= 0 ? h.knownName(o) : nullptr) who = caps(*n);
    switch (h.state) {
    case Human::State::Greeting: return (h.gossiping ? "GOSSIPING WITH " : "GREETING ") + who;
    case Human::State::Eating: return "EATING";
    case Human::State::Drinking: return "DRINKING";
    case Human::State::Relieving: return h.shitting() ? "SHITTING" : "PISSING";
    case Human::State::Punching: return (h.hunting() ? "ATTACKING " : "FIGHTING ") + who;
    case Human::State::Giving: return "SHARING A CAKE WITH " + who;
    case Human::State::Washing: return "WASHING";
    case Human::State::Looting: return h.robbing() ? "ROBBING " + who : "GOING THROUGH " + who + "'S BODY";
    case Human::State::Feeding: return "EATING " + who;
    case Human::State::Dead: return "DEAD";
    case Human::State::Waking: return "COMING TO";
    case Human::State::Sleeping: return "SLEEPING";
    case Human::State::Sitting:
        if (h.dazed() >= 0.5f) return "COMING TO";
        if (h.goal == Human::Goal::Watch) return "KEEPING WATCH OVER " + who;
        [[fallthrough]];
    case Human::State::Rising: return h.rousing() ? "WAKING UP" : "RESTING";
    case Human::State::Idle:
    case Human::State::Walking: break;
    }
    if (h.waitingForCompany()) return "WAITING FOR THE OTHERS";
    if (h.state == Human::State::Idle && h.goal != Human::Goal::Follow && h.goal != Human::Goal::Company &&
        h.goal != Human::Goal::Escort &&
        h.goal != Human::Goal::Escorted && h.goal != Human::Goal::GiveWay)
        return "LOOKING AROUND";
    switch (h.goal) {
    case Human::Goal::Explore: return "EXPLORING";
    case Human::Goal::FetchKey: return "GETTING THE KEY";
    case Human::Goal::OpenDoor: return "GOING TO THE DOOR";
    case Human::Goal::PassDoor: return "GOING THROUGH";
    case Human::Goal::Wander: return h.settled() ? "KEEPING TO HOME" : "WANDERING";
    case Human::Goal::CheckDoor: return "CHECKING A DOOR";
    case Human::Goal::Meet: return "MEETING " + who;
    case Human::Goal::Follow: return "FOLLOWING " + who;
    case Human::Goal::Company: return "WITH " + who;
    case Human::Goal::FetchCake: return "GETTING A CAKE";
    case Human::Goal::Drink: return "GOING TO DRINK";
    case Human::Goal::Relieve: return "FINDING SOMEWHERE TO GO";
    case Human::Goal::Sleep: return "LOOKING FOR A PLACE TO SLEEP";
    case Human::Goal::Share: return "SHARING A CAKE WITH " + who;
    case Human::Goal::Fight: return (h.hunting() ? "HUNTING " : "FIGHTING ") + who;
    case Human::Goal::Flee: return "RUNNING FROM " + who;
    case Human::Goal::Wash: return "GOING TO WASH";
    case Human::Goal::Avoid: return "KEEPING AWAY";
    case Human::Goal::Escort: return "TAKING " + who + " TO WATER";
    case Human::Goal::Escorted: return "LED TO WATER BY " + who;
    case Human::Goal::Track: return "FOLLOWING BLOOD";
    case Human::Goal::GiveWay: return "MAKING WAY";
    case Human::Goal::Loot: return "GOING TO " + who + "'S BODY";
    case Human::Goal::Feed: return "GOING TO " + who + "'S BODY";
    case Human::Goal::Rob: return "SNEAKING UP ON " + who;
    case Human::Goal::Watch: return "GOING TO KEEP WATCH OVER " + who;
    }
    return "";
}

/// A log entry as h reads it: those in it under the names h knows them by.
std::string logLine(const Human& h, const LifeLog::Entry& e) {
    auto nameOf = [&](int who) {
        const std::string* n = h.knownName(who);
        return n ? caps(*n) : std::string("SOMEONE");
    };
    std::string s;
    for (char c : e.what) {
        if (c == '@')
            s += nameOf(e.who);
        else if (c == '#')
            s += nameOf(e.whom);
        else
            s += c;
    }
    if (e.times > 1) s += " X" + std::to_string(e.times);
    return s;
}

/// A labelled bar from x to right at y, filled by amount from 0 to 1.
void bar(const char* label, int x, int right, int y, float amount, Color fill) {
    ui::text(label, x, y - 2);
    int bx = x + ui::textWidth(label) + 3, bw = right - bx;
    DrawRectangle(bx, y, bw, BAR_H, ui::INK);
    DrawRectangle(bx + 1, y + 1, bw - 2, BAR_H - 2, ui::PAPER);
    DrawRectangle(bx + 1, y + 1, int((bw - 2) * std::clamp(amount, 0.0f, 1.0f)), BAR_H - 2, fill);
}

/// An icon and its count, ending at right. Returns where they start.
int item(int right, int y, int count, void (*icon)(int, int)) {
    char n[8];
    std::snprintf(n, sizeof n, "%d", count);
    int x = right - ui::textWidth(n);
    ui::text(n, x, y);
    x -= ICON_GAP + ICON_PX;
    icon(x, y + 1);
    return x;
}

} // namespace

// The world is grown around the human you made, who starts in the spawn room.
WorldScene::WorldScene(const HumanLook& look, uint64_t humanSeed, uint64_t seed, const std::string& name)
    : seed_(seed), world_(seed) {
    world_.makeSpawn(look, humanSeed, name);
}

// Leaving the world any way at all, even closing the window, counts as
// leaving them behind.
WorldScene::~WorldScene() { record(); }

// The cause is the last line of their log, which their death always
// writes; times are counted from when they came in.
void WorldScene::record() {
    if (recorded_) return;
    recorded_ = true;
    const Human& h = world_.human(0);
    obituary::Record r;
    r.name = h.name;
    r.survived = world_.time() - h.born - h.deadFor();
    r.when = std::time(nullptr);
    for (const LifeLog::Entry& e : h.log.entries()) r.log.push_back({e.time - h.born, logLine(h, e)});
    r.cause = h.dead() && !r.log.empty() ? r.log.back().what : "LEFT BEHIND";
    obituary::add(r);
}

const HumanSprites& WorldScene::spritesFor(const Human& h) {
    auto& s = sprites_[h.id];
    if (!s) s = std::make_unique<HumanSprites>(h.look);
    s->bloody(h.stains);
    s->dirty(h.dirt, h.soaked);
    if (h.dead()) s->remains(1.0f - float(h.flesh) / Human::FLESH_MEALS, h.decay(), uint64_t(h.id) * 2654435761ULL + 1, h.diedAsleep());
    return *s;
}

const Human* WorldScene::nearestLiving() const {
    Vec2i from = world_.human(follow_).cell;
    const Human* best = nullptr;
    long bestDist = 0;
    for (const auto& h : world_.humans()) {
        if (h->dead()) continue;
        Vec2i d = h->cell - from;
        long dist = long(d.x) * d.x + long(d.y) * d.y;
        if (!best || dist < bestDist) best = h.get(), bestDist = dist;
    }
    return best;
}

void WorldScene::update(float dt, const Canvas& canvas) {
    if (IsKeyPressed(KEY_ESCAPE)) pauseToggle_ = true;
    if (pauseToggle_) {
        pauseToggle_ = false;
        paused_ = !paused_;
    }
    if (toMenu_) {
        next = std::make_unique<MenuScene>();
        return;
    }
    if (paused_) {
        GetMouseWheelMove();
        return;
    }
    for (int i = 0; i < SPEED_COUNT; i++)
        if (IsKeyPressed(KEY_ONE + i)) speed_ = i;
    speed_ = std::clamp(speed_ + speedStep_, 0, SPEED_COUNT - 1);
    speedStep_ = 0;
    if (IsKeyPressed(KEY_TAB)) reveal_ = !reveal_;
    if (IsKeyPressed(KEY_F5)) talk::reload();
    int step = zoomStep_;
    zoomStep_ = 0;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) step++;
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) step--;
    // While the log is open the wheel scrolls it, down to older entries.
    float wheel = GetMouseWheelMove();
    if (IsKeyPressed(KEY_L)) logToggle_ = true;
    if (IsKeyPressed(KEY_S)) statsToggle_ = true;
    if (IsKeyPressed(KEY_M)) mapToggle_ = true;
    // The log, stats and map share one place, so opening one shuts the others.
    if (logToggle_) {
        logToggle_ = false;
        logOpen_ = !logOpen_;
        logScroll_ = 0;
        if (logOpen_) statsOpen_ = mapOpen_ = false;
    }
    if (statsToggle_) {
        statsToggle_ = false;
        statsOpen_ = !statsOpen_;
        if (statsOpen_) logOpen_ = mapOpen_ = false;
    }
    if (mapToggle_) {
        mapToggle_ = false;
        mapOpen_ = !mapOpen_;
        mapFollow_ = true;
        if (mapOpen_) logOpen_ = statsOpen_ = false;
    }
    // While the map is open, + and - and the wheel zoom it instead.
    if (mapOpen_) {
        if (wheel > 0.0f) step++;
        if (wheel < 0.0f) step--;
        mapZoom_ = std::clamp(mapZoom_ + step, 0, MAP_ZOOM_COUNT - 1);
        step = 0;
        wheel = 0.0f;
    }
    if (logOpen_) {
        if (wheel < 0.0f) logScroll_++;
        if (wheel > 0.0f) logScroll_--;
        wheel = 0.0f;
    }
    if (wheel > 0.0f) step++;
    if (wheel < 0.0f) step--;
    zoom_ = std::clamp(zoom_ + step, 0, ZOOM_COUNT - 1);
    if (nextHuman_) {
        nextHuman_ = false;
        if (const Human* h = nearestLiving()) {
            follow_ = h->id;
            mapFollow_ = true;
        } else {
            next = std::make_unique<MenuScene>();
            return;
        }
    }
    // Dragging the log up shows older entries, one per row dragged.
    Vector2 m = canvas.mouse();
    if (logOpen_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, LOG_AREA)) {
        dragging_ = true;
        dragFrom_ = m.y;
        dragScroll_ = logScroll_;
    }
    if (!logOpen_ || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragging_ = false;
    if (dragging_) logScroll_ = dragScroll_ + int((dragFrom_ - m.y) / ui::ROW);
    // Dragging the map moves it under the finger, and it stops following.
    if (mapOpen_ && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, LOG_AREA)) {
        mapDragging_ = true;
        mapDragFrom_ = m;
        Vec2i at = world_.human(follow_).cell;
        mapDragCentre_ = mapFollow_ ? Vector2{at.x + 0.5f, at.y + 0.5f} : mapCentre_;
    }
    if (!mapOpen_ || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) mapDragging_ = false;
    if (mapDragging_) {
        float px = float(MAP_ZOOMS[mapZoom_]);
        Vector2 moved{(m.x - mapDragFrom_.x) / px, (m.y - mapDragFrom_.y) / px};
        if (!mapFollow_ || std::fabs(moved.x) + std::fabs(moved.y) > 0.0f) {
            mapFollow_ = false;
            mapCentre_ = {mapDragCentre_.x - moved.x, mapDragCentre_.y - moved.y};
        }
    }
    mapTime_ += dt;
    for (float left = dt * SPEEDS[speed_]; left > 0.0f; left -= MAX_TICK) world_.update(std::min(left, MAX_TICK));
    if (world_.human(0).dead()) record();
}

// Each entry is its time, dimmed, then what happened, wrapped under itself.
// Whatever doesn't fit at the bottom is scrolled to.
void WorldScene::drawLog(const Human& viewer) {
    if (logFor_ != viewer.id) logFor_ = viewer.id, logScroll_ = 0, dragging_ = false;
    DrawRectangleRec(LOG_AREA, LOG_SHADE);
    const auto& entries = viewer.log.entries();
    int left = MARGIN, right = int(LOG_AREA.x + LOG_AREA.width) - MARGIN;
    int y = int(LOG_AREA.y) + MARGIN, bottom = int(LOG_AREA.y + LOG_AREA.height) - MARGIN;
    if (entries.empty()) {
        ui::textCentred("NOTHING YET", (left + right) / 2, y, ui::DIM);
        return;
    }
    logScroll_ = std::clamp(logScroll_, 0, int(entries.size()) - 1);
    if (logScroll_ > 0) {
        std::string newer = std::to_string(logScroll_) + " NEWER";
        ui::textRight(newer.c_str(), right, y, ui::DIM);
        y += ui::ROW;
    }
    // The newest time is the longest; measured in zeros, as 1 is narrower.
    std::string widest = ui::clockText(entries.back().time);
    for (char& c : widest)
        if (std::isdigit(static_cast<unsigned char>(c))) c = '0';
    int column = ui::textWidth(widest.c_str()) + LOG_TIME_GAP;
    for (int i = int(entries.size()) - 1 - logScroll_; i >= 0; i--) {
        const LifeLog::Entry& e = entries[size_t(i)];
        std::vector<std::string> rows = ui::wrap(logLine(viewer, e), right - left - column);
        if (y + int(rows.size()) * ui::ROW > bottom) break;
        ui::text(ui::clockText(e.time).c_str(), left, y, ui::DIM);
        for (const std::string& row : rows) {
            ui::text(row.c_str(), left + column, y);
            y += ui::ROW;
        }
    }
}

namespace {

/// A section's title, dimmed; returns the row under it.
int section(const char* title, int x, int y) {
    ui::text(title, x, y, ui::DIM);
    return y + ui::ROW;
}

/// A label and a count.
int countRow(const char* label, int count, int x, int right, int y) {
    ui::text(label, x, y);
    ui::textRight(std::to_string(count).c_str(), right, y, ui::BRIGHT);
    return y + ui::ROW;
}

/// A label and a bar of 0 to 9 pips, as the create screen draws traits.
int pipRow(const char* label, float amount, int x, int right, int y, Color fill = ui::INK) {
    ui::text(label, x, y);
    int bx = x + ui::textWidth(STAT_WIDEST) + STAT_LABEL_GAP, step = (right - bx) / STAT_PIPS;
    int pips = int(std::clamp(amount, 0.0f, 1.0f) * 9.99f);
    for (int p = 0; p < STAT_PIPS; p++)
        DrawRectangle(bx + p * step, y + 2, step - 1, STAT_BAR_H, p < pips ? fill : ui::GROUND);
    return y + ui::ROW;
}

} // namespace

// What they are like, whom they know, how their mind stands and what going
// without has taught them.
void WorldScene::drawStats(const Human& viewer) {
    DrawRectangleRec(LOG_AREA, LOG_SHADE);
    int x = MARGIN, right = int(LOG_AREA.x + LOG_AREA.width) - MARGIN;
    int y = int(LOG_AREA.y) + MARGIN;
    const Traits& t = viewer.traits;
    y = section("TRAITS", x, y);
    y = pipRow("BRAVE", t.brave, x, right, y);
    y = pipRow("GREEDY", t.greedy, x, right, y);
    y = pipRow("CURIOUS", t.curious, x, right, y);
    y = pipRow("SOCIAL", t.sociable, x, right, y);
    y = pipRow("MODEST", viewer.modesty(), x, right, y);
    // Whom they have greeted, been told the name of, and count friends and
    // foes, seen or heard of.
    y = section("PEOPLE", x, y + SECTION_GAP);
    y = countRow("MET", int(viewer.met.size()), x, right, y);
    y = countRow("NAMES", int(viewer.memory.names.size()), x, right, y);
    y = countRow("FRIENDS", viewer.friendCount(), x, right, y);
    y = countRow("FOES", viewer.foeCount(), x, right, y);
    // How frightened and erratic they are, and what eating flesh left:
    // guilt, or a taste for it; the mind red once erratic.
    y = section("MIND", x, y + SECTION_GAP);
    y = pipRow("FEAR", viewer.fright(), x, right, y);
    y = pipRow("HOPE", viewer.hope(), x, right, y);
    y = pipRow("ERRATIC", viewer.erratic(), x, right, y, viewer.isErratic() ? ERRATIC_INK : ui::INK);
    y = pipRow("GUILT", viewer.guilt(), x, right, y);
    y = pipRow("CRAVE", viewer.appetite(), x, right, y);
    // How badly they need to go.
    y = section("BODY", x, y + SECTION_GAP);
    y = pipRow("BLADDER", viewer.bladder, x, right, y);
    y = pipRow("BOWELS", viewer.bowels, x, right, y);
    y = section("LESSONS", x, y + SECTION_GAP);
    y = pipRow("HOARDS", viewer.lessons.hoard, x, right, y);
    y = pipRow("KEYS FIRST", viewer.lessons.keysFirst, x, right, y);
    y = pipRow(STAT_WIDEST, viewer.lessons.nearWater, x, right, y);
    pipRow("SAFE SLEEP", viewer.lessons.safeSleep, x, right, y);
}

// The buttons report their clicks while drawing; they act on the next
// update.
void WorldScene::draw(const Canvas& canvas) {
    ClearBackground(BLACK);
    const Human& viewer = world_.human(follow_);
    drawWorld(world_, viewer, [this](const Human& h) -> const HumanSprites& { return spritesFor(h); },
              floorTiles_, statueArt_, reveal_, ZOOMS[zoom_]);
    if (!logOpen_ && !statsOpen_ && !mapOpen_) drawSpeech(world_, viewer, reveal_, ZOOMS[zoom_], TOP_H, BOTTOM_Y);
    // Under the pause menu nothing else takes a tap.
    Vector2 mouse = paused_ ? Vector2{-1.0f, -1.0f} : canvas.mouse();
    char line[64];

    // Top strip. Who is followed and what they carry: the one name it is
    // sure of is its own, underlined, and tapping it opens their stats.
    ui::panel({0, 0, Canvas::W, TOP_H});
    // First the world's size, and whether it is all revealed.
    int y = MARGIN;
    std::snprintf(line, sizeof line, "HUMANS %zu  CHUNKS %zu", world_.livingCount(), world_.chunkCount());
    ui::text(line, MARGIN, y, ui::DIM);
    if (reveal_) ui::textRight("REVEAL", RIGHT, y, REVEAL_INK);
    y = NAME_TEXT_Y;
    std::string name = caps(viewer.name);
    int nameW = ui::textWidth(name.c_str());
    Rectangle nameHit{0, float(NAME_ROW_Y), float(MARGIN + nameW + LOG_GAP / 2), float(NAME_ROW_H)};
    bool overName = CheckCollisionPointRec(mouse, nameHit);
    Color nameInk = overName || statsOpen_ ? WHITE : ui::BRIGHT;
    ui::text(name.c_str(), MARGIN, y, nameInk);
    DrawRectangle(MARGIN, y + NAME_UNDERLINE, nameW, 1, statsOpen_ ? WHITE : ui::DIM);
    if (overName && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) statsToggle_ = true;
    Rectangle logButton{float(MARGIN + nameW + LOG_GAP), float(NAME_ROW_Y), float(BUTTON_W), float(NAME_ROW_H)};
    logToggle_ |= ui::button(logButton, "LOG", mouse);
    Rectangle mapButton{logButton.x + BUTTON_W + BUTTON_GAP, float(NAME_ROW_Y), float(BUTTON_W), float(NAME_ROW_H)};
    mapToggle_ |= ui::button(mapButton, "MAP", mouse);
    int x = item(RIGHT, y, viewer.cakes, drawCakeIcon);
    item(x - ITEM_GAP, y, viewer.keys, drawKeyIcon);

    // Bottom strip: health across it, hunger, thirst and tiredness side by
    // side under it, then what they are doing, broken onto a second row
    // rather than running off the edge.
    ui::panel({0, float(BOTTOM_Y), Canvas::W, float(BOTTOM_H)});
    bar("HP", MARGIN, RIGHT, BAR_Y, viewer.health, HEALTH_BAR);
    // The needs themselves equally long, whatever their labels take.
    const char* labels[] = {"HUNGER", "THIRST", "TIRED"};
    const float amounts[] = {viewer.hunger, viewer.thirst, viewer.tiredness};
    const Color fills[] = {HUNGER_BAR, THIRST_BAR, TIRED_BAR};
    constexpr int BARS = int(std::size(labels));
    int barW = RIGHT - MARGIN - (BARS - 1) * MARGIN;
    for (const char* l : labels) barW -= ui::textWidth(l) + 3;
    barW /= BARS;
    for (int i = 0, x = MARGIN; i < BARS; i++) {
        int right = x + ui::textWidth(labels[i]) + 3 + barW;
        bar(labels[i], x, i == BARS - 1 ? RIGHT : right, NEEDS_Y, amounts[i], fills[i]);
        x = right + MARGIN;
    }
    std::vector<std::string> rows = ui::wrap(doing(viewer), Canvas::W - 2 * MARGIN);
    int block = int(rows.size()) * ui::ROW;
    y = DOING_TOP + (DOING_BOTTOM - DOING_TOP - block) / 2;
    for (const std::string& row : rows) {
        ui::textCentred(row.c_str(), Canvas::W / 2, y);
        y += ui::ROW;
    }

    if (logOpen_) drawLog(viewer);
    if (statsOpen_) drawStats(viewer);
    if (mapOpen_) {
        Vector2 centre = mapFollow_ ? Vector2{viewer.cell.x + 0.5f, viewer.cell.y + 0.5f} : mapCentre_;
        drawMap(world_, viewer, LOG_AREA, MAP_ZOOMS[mapZoom_], centre, mapTime_);
    }

    if (ui::button(ZOOM_IN_BUTTON, "+", mouse)) zoomStep_++;
    if (ui::button(ZOOM_OUT_BUTTON, "-", mouse)) zoomStep_--;
    if (ui::arrowButton(SPEED_UP_BUTTON, true, mouse)) speedStep_++;
    ui::panel(SPEED_SLOT);
    std::snprintf(line, sizeof line, "X%d", SPEEDS[speed_]);
    ui::textCentred(line, int(SPEED_SLOT.x + SPEED_SLOT.width / 2), int(SPEED_SLOT.y) + 3, ui::BRIGHT);
    if (ui::arrowButton(SPEED_DOWN_BUTTON, false, mouse)) speedStep_--;
    if (viewer.dead()) nextHuman_ |= ui::button(NEXT_BUTTON, "NEXT", mouse);
    pauseToggle_ |= ui::button(PAUSE_BUTTON, "II", mouse);
    if (paused_) drawPause(canvas.mouse());
}

// Leaving with the human you made alive leaves them behind, so it says so.
void WorldScene::drawPause(Vector2 mouse) {
    DrawRectangle(0, 0, Canvas::W, Canvas::H, PAUSE_SHADE);
    ui::textCentred("PAUSED", Canvas::W / 2, PAUSE_TITLE_Y, ui::BRIGHT);
    pauseToggle_ |= ui::button(RESUME_BUTTON, "RESUME", mouse);
    toMenu_ |= ui::button(MENU_BUTTON, "MAIN MENU", mouse);
    if (!recorded_) {
        std::string warning = caps(world_.human(0).name) + " WILL BE LEFT BEHIND";
        int y = int(MENU_BUTTON.y + MENU_BUTTON.height) + PAUSE_GAP;
        for (const std::string& row : ui::wrap(warning, Canvas::W - 2 * MARGIN * 4)) {
            ui::textCentred(row.c_str(), Canvas::W / 2, y, ui::DIM);
            y += ui::ROW;
        }
    }
}
