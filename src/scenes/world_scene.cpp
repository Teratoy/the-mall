#include "scenes/world_scene.h"
#include "audio/audio.h"
#include "audio/music.h"
#include "core/files.h"
#include "human/human.h"
#include "human/obituary.h"
#include "human/talk.h"
#include "render/bottle_art.h"
#include "render/canvas.h"
#include "render/light_view.h"
#include "render/map_view.h"
#include "render/speech_view.h"
#include "render/sprites.h"
#include "render/world_view.h"
#include "scenes/menu_scene.h"
#include "world/gen_tuning.h"
#include "ui/pixel_ui.h"
#include "rlgl.h"
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
/// How fast the camera closes on the human it follows: each second it
/// closes all but e^-CAMERA_CATCH of the gap. Walking, it trails by about
/// the speed over this.
constexpr float CAMERA_CATCH = 8.0f;
/// The furthest it trails, in world pixels, so at high speeds the human
/// stays near the middle.
constexpr float CAMERA_LAG_MAX = 12.0f;

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
/// The pause menu: its title and three buttons down the middle, over the
/// whole screen darkened.
constexpr int PAUSE_W = 100, PAUSE_H = 24, PAUSE_GAP = 10;
constexpr int PAUSE_TITLE_Y = Canvas::H / 2 - 50;
constexpr Rectangle pauseButton(int i) {
    return {(Canvas::W - PAUSE_W) / 2.0f, Canvas::H / 2.0f - 20 + i * (PAUSE_H + PAUSE_GAP), PAUSE_W, PAUSE_H};
}
constexpr Rectangle RESUME_BUTTON = pauseButton(0);
constexpr Rectangle CONFIG_BUTTON = pauseButton(1);
constexpr Rectangle MENU_BUTTON = pauseButton(2);
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
/// The phone's world fills the canvas, under the strips.
constexpr Rectangle PHONE_VIEW{0, 0, Canvas::W, Canvas::H};

// The PC layout, on the wide canvas: stats and log side by side on the
// left, the world in a square, then the rest of the HUD over the map on the
// right. Who is followed, what they carry and their bars head the stats.
// Every column is as wide as the phone's log, so stats and log read the
// same in both. Nothing is opened with a button; all of it is always there.
constexpr int COLUMN_W = int(LOG_AREA.width);
static_assert(3 * COLUMN_W + Canvas::H == Canvas::WIDE_W, "the PC columns and square fill the wide canvas");
/// The stats column, top down: the name and items, the four bars a row
/// each, then the stats.
constexpr int WIDE_NAME_Y = MARGIN;
constexpr int WIDE_BARS_Y = WIDE_NAME_Y + ui::ROW + MARGIN;
constexpr int WIDE_BARS = 4;
constexpr int WIDE_STATS_Y = WIDE_BARS_Y + WIDE_BARS * ui::ROW + MARGIN;
constexpr Rectangle WIDE_STATS{0, float(WIDE_STATS_Y), float(COLUMN_W), float(Canvas::H - WIDE_STATS_Y)};
constexpr int STATS_LEFT = MARGIN, STATS_RIGHT = COLUMN_W - MARGIN;
constexpr Rectangle WIDE_LOG{float(COLUMN_W), 0, float(COLUMN_W), Canvas::H};
constexpr Rectangle WIDE_VIEW{float(2 * COLUMN_W), 0, Canvas::H, Canvas::H};
constexpr int HUD_X = 2 * COLUMN_W + Canvas::H;
constexpr int HUD_LEFT = HUD_X + MARGIN, HUD_RIGHT = Canvas::WIDE_W - MARGIN;
/// The HUD column, top down: the world's size, then the buttons in a row;
/// the map takes the rest.
constexpr int WIDE_BUTTONS_Y = MARGIN + ui::ROW + MARGIN;
constexpr Rectangle wideButtonAt(int i) {
    return {float(HUD_LEFT + i * (BUTTON_W + BUTTON_GAP)), float(WIDE_BUTTONS_Y), float(BUTTON_W), float(BUTTON_H)};
}
static_assert(HUD_LEFT + 6 * BUTTON_W + 5 * BUTTON_GAP <= HUD_RIGHT, "the PC buttons fit their row");
constexpr int WIDE_MAP_Y = WIDE_BUTTONS_Y + BUTTON_H + MARGIN;
constexpr Rectangle WIDE_MAP{float(HUD_X), float(WIDE_MAP_Y), float(COLUMN_W), float(Canvas::H - WIDE_MAP_Y)};
/// What they are doing goes in a strip along the bottom of the square, as
/// tall as its rows; once the followed human is dead, NEXT stands over it.
constexpr int WIDE_DOING_W = Canvas::H - 2 * MARGIN;
constexpr float WIDE_NEXT_W = 48, WIDE_NEXT_H = 16;
/// The lines between the columns.
constexpr Color COLUMN_EDGE = ui::GROUND;

// Choosing the human, before the world's first moment: a strip at the top
// with BACK and the name to tap and type over, and one at the bottom with
// the traits and the two buttons. On the PC both stand in the left two
// columns, on paper, one over the other in the middle.
/// Mixed into the world's seed for the dice the humans are rolled with.
constexpr uint64_t ROLL_SALT = 0x5EED0F7A11ULL;
/// While choosing, the camera is as close as play lets it get.
constexpr float CHOOSE_ZOOM = ZOOMS[ZOOM_COUNT - 1];
/// After WAKE UP, the seconds the camera takes to ease out to the play zoom.
constexpr float ZOOM_EASE = 2.0f;
/// After WAKE UP, the choosing strips slide off the edges for this long, then
/// the HUD's slide in for as long. On the PC the column slides off over both.
constexpr float SLIDE_TIME = 0.35f;
constexpr Rectangle CHOOSE_BACK{MARGIN, MARGIN, 32, 14};
/// The name is a field to type in: a box round it to tap, a hint under it,
/// and at most NAME_LONGEST characters.
constexpr int CHOOSE_NAME_Y = MARGIN + 3, CHOOSE_NAME_W = 136, CHOOSE_NAME_H = 17;
constexpr int CHOOSE_HINT_Y = CHOOSE_NAME_Y + CHOOSE_NAME_H + 4;
constexpr int CHOOSE_TOP_H = CHOOSE_HINT_Y + ui::ROW + 2;
constexpr size_t NAME_LONGEST = 14;
/// The caret blinks on and off this often.
constexpr float CARET_BLINK = 0.5f;
/// The traits, each a label and a bar of TRAIT_PIPS, one under the other,
/// the bars from the middle TRAIT_BAR_W right and the labels as far left.
constexpr int CHOOSE_PAD = 8;
constexpr int TRAITS = 4, TRAIT_ROW = 13, TRAIT_BAR_W = 68, TRAIT_PIPS = 9, TRAIT_BAR_H = 6;
constexpr int CHOOSE_BUTTON_H = 24;
constexpr int CHOOSE_BOTTOM_H = CHOOSE_PAD + TRAITS * TRAIT_ROW + CHOOSE_PAD + CHOOSE_BUTTON_H + CHOOSE_PAD;
/// On the PC, the column the choosing stands in.
constexpr int CHOOSE_WIDE_W = 2 * COLUMN_W;

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
    case Human::State::Drinking: return h.atTap() ? "DRINKING AT THE COOLER" : "DRINKING";
    case Human::State::Sipping: return "DRINKING FROM THE BOTTLE";
    case Human::State::Filling: return h.atTap() ? "FILLING UP AT THE COOLER" : "FILLING THE BOTTLE";
    case Human::State::Pouring: return h.refilling() ? "POURING INTO THE COOLER" : "POURING THE BOTTLE OUT";
    case Human::State::Stashing: return h.putting() ? "STASHING THINGS" : "PICKING THINGS UP";
    case Human::State::Vending: return "BUYING FROM A MACHINE";
    case Human::State::Typing: return "TYPING AT A COMPUTER";
    case Human::State::Relieving: return h.shitting() ? "SHITTING" : "PISSING";
    case Human::State::Punching: return (h.hunting() ? "ATTACKING " : "FIGHTING ") + who;
    case Human::State::Giving: return (h.givingWater() ? "SHARING WATER WITH " : "SHARING A CAKE WITH ") + who;
    case Human::State::Washing: return "WASHING";
    case Human::State::Looting: return h.robbing() ? "ROBBING " + who : "GOING THROUGH " + who + "'S BODY";
    case Human::State::Feeding: return "EATING " + who;
    case Human::State::Dead: return "DEAD";
    case Human::State::Waking: return "COMING TO";
    case Human::State::Sleeping: return "SLEEPING";
    case Human::State::Sitting:
        if (h.dazed() >= 0.5f) return "COMING TO";
        if (h.goal == Human::Goal::Watch) return "KEEPING WATCH OVER " + who;
        if (h.lyingDown()) return "LYING DOWN";
        [[fallthrough]];
    case Human::State::Rising: return h.rousing() ? "WAKING UP" : "RESTING";
    case Human::State::Idle:
    case Human::State::Walking: break;
    }
    if (h.winded()) return "CATCHING BREATH";
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
    case Human::Goal::Fill: return "GOING TO FILL THE BOTTLE";
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
    case Human::Goal::Ask: return "GOING TO ASK " + who + " FOR WATER";
    case Human::Goal::Pick: return "GOING TO PICK THINGS UP";
    case Human::Goal::Stash: return "TAKING THINGS TO A STASH";
    case Human::Goal::Vend: return "GOING TO A MACHINE";
    case Human::Goal::Voice: return "GOING TOWARD A VOICE";
    case Human::Goal::Inspect: return "GOING TO LOOK";
    case Human::Goal::Feed: return "GOING TO " + who + "'S BODY";
    case Human::Goal::Rob: return "SNEAKING UP ON " + who;
    case Human::Goal::Watch: return "GOING TO KEEP WATCH OVER " + who;
    case Human::Goal::Rescue: return "GOING TO HELP " + who;
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

/// A labelled bar from x to right at y, filled by amount from 0 to 1. The bar
/// starts at barX, or just past the label if none is given.
void bar(const char* label, int x, int right, int y, float amount, Color fill, int barX = -1) {
    ui::text(label, x, y - 2);
    int bx = barX >= 0 ? barX : x + ui::textWidth(label) + 3, bw = right - bx;
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

/// The bottles carried, right to left from right, each drawn as full and
/// as dirty as it is; returns the left edge of the last, or right if none.
int bottleItems(const Human& h, int right, int y) {
    int x = right;
    for (auto it = h.bottles.rbegin(); it != h.bottles.rend(); ++it) {
        x -= BOTTLE_ACROSS;
        drawBottleIcon(x, y + 1, *it);
        x -= ICON_GAP;
    }
    return x + (h.bottles.empty() ? 0 : ICON_GAP);
}

} // namespace

// The world is grown around the human you made, who starts in the spawn room.
WorldScene::WorldScene(const HumanLook& look, uint64_t humanSeed, uint64_t seed, const std::string& name)
    : seed_(seed), world_(std::make_unique<World>(seed)), look_(look), humanSeed_(humanSeed), chosen_(name) {
    world_->makeSpawn(look, humanSeed, name);
}

// Nothing goes in the obituary for one never woken.
WorldScene::WorldScene(uint64_t seed) : seed_(seed), recorded_(true), rolls_(seed ^ ROLL_SALT), wokeFor_(-1.0f) {
    roll();
}

// The spawn room is grown from the world's dice before the human is laid in
// it, and its bottle by dice of its own, so the room stays as it was.
void WorldScene::roll() {
    look_ = HumanLook::random(rolls_);
    humanSeed_ = rolls_.next();
    world_ = std::make_unique<World>(seed_);
    world_->makeSpawn(look_, humanSeed_, chosen_);
    sprites_.clear();
    focusOf_ = -1;
}

float WorldScene::zoomNow() const {
    if (choosing()) return CHOOSE_ZOOM;
    float play = ZOOMS[zoom_];
    if (wokeFor_ >= ZOOM_EASE) return play;
    float t = wokeFor_ / ZOOM_EASE;
    t = t * t * (3.0f - 2.0f * t);
    return CHOOSE_ZOOM + (play - CHOOSE_ZOOM) * t;
}

bool WorldScene::updateChoosing(float dt, const Canvas&) {
    chooseTime_ += dt;
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
        if (IsKeyPressed(KEY_ENTER) || ui::backPressed()) editing_ = false;
        if (!editing_)
            while (!chosen_.empty() && chosen_.back() == ' ') chosen_.pop_back();
    } else {
        if (IsKeyPressed(KEY_R)) refresh_ = true;
        if (IsKeyPressed(KEY_ENTER)) wake_ = true;
        if (ui::backPressed()) back_ = true;
    }
    if (back_) {
        next = std::make_unique<MenuScene>();
        return true;
    }
    if (refresh_) {
        refresh_ = false;
        roll();
    }
    // Grown once more with the name typed, which the human keeps for life;
    // the same world as rolled, now to be played.
    if (wake_) {
        wake_ = false;
        world_ = std::make_unique<World>(seed_);
        world_->makeSpawn(look_, humanSeed_, chosen_);
        sprites_.clear();
        focusOf_ = -1;
        wokeFor_ = 0.0f;
        recorded_ = false;
        return true;
    }
    easeCamera(dt);
    return true;
}

void WorldScene::tryOut(std::optional<ChunkKind> kind, const Footprint& parts) {
    if (kind)
        world_->growPastSpawn(*kind, parts);
    else
        world_->ensureGenerated(world_->spawnEntry());
    recorded_ = true;
}

// Leaving the world any way at all, even closing the window, counts as
// leaving them behind.
WorldScene::~WorldScene() { record(); }

// The cause is the last line of their log, which their death always
// writes; times are counted from when they came in.
void WorldScene::record() {
    if (recorded_) return;
    recorded_ = true;
    const Human& h = world_->human(0);
    obituary::Record r;
    r.name = h.name;
    r.survived = world_->time() - h.born - h.deadFor();
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
    s->setDirty(h.dirty());
    if (h.dead()) s->remains(1.0f - float(h.flesh) / Human::FLESH_MEALS, h.decay(), uint64_t(h.id) * 2654435761ULL + 1, h.diedAsleep());
    return *s;
}

const Human* WorldScene::nearestLiving() const {
    Vec2i from = world_->human(follow_).cell;
    const Human* best = nullptr;
    long bestDist = 0;
    for (const auto& h : world_->humans()) {
        if (h->dead()) continue;
        Vec2i d = h->cell - from;
        long dist = long(d.x) * d.x + long(d.y) * d.y;
        if (!best || dist < bestDist) best = h.get(), bestDist = dist;
    }
    return best;
}

void WorldScene::update(float dt, const Canvas& canvas) {
    // The menu's music plays on while choosing the sleeper, and fades out as
    // it wakes; the world has none of its own yet.
    music::want(choosing() ? music::MENU : "");
    if (choosing()) {
        updateChoosing(dt, canvas);
        return;
    }
    wokeFor_ += dt;
    // Back from CONFIGURATION goes to the pause menu, not the world.
    if (ui::backPressed()) {
        if (configOpen_) configOpen_ = false;
        else pauseToggle_ = true;
    }
    if (pauseToggle_) {
        pauseToggle_ = false;
        paused_ = !paused_;
        configOpen_ = false;
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
    // D tries the light's dither patterns in turn.
    if (IsKeyPressed(KEY_D)) {
        setDither(Dither((int(dither()) + 1) % DITHER_COUNT));
        TraceLog(LOG_INFO, "light dither: %s", ditherName(dither()));
    }
    // F5 reads the data files again: lines, sprites, sounds, and the
    // world's knobs for chunks grown from now on.
    if (IsKeyPressed(KEY_F5)) {
        talk::reload();
        sprites::reload();
        tune::reload();
        audio::reload();
        music::reload();
    }
    int step = zoomStep_;
    zoomStep_ = 0;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) step++;
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) step--;
    // While the log is open the wheel scrolls it, down to older entries.
    float wheel = GetMouseWheelMove();
    if (IsKeyPressed(KEY_L)) logToggle_ = true;
    if (IsKeyPressed(KEY_S)) statsToggle_ = true;
    if (IsKeyPressed(KEY_M)) mapToggle_ = true;
    // On the PC all three are always open, so nothing toggles them, and
    // the wheel goes to whichever the mouse is over.
    bool wide = canvas.wide();
    Vector2 m = canvas.mouse();
    if (wide) logToggle_ = statsToggle_ = mapToggle_ = false;
    Rectangle logArea = wide ? WIDE_LOG : LOG_AREA, mapArea = wide ? WIDE_MAP : LOG_AREA;
    bool logShown = wide || logOpen_, mapShown = wide || mapOpen_;
    bool wheelOnMap = wide ? CheckCollisionPointRec(m, WIDE_MAP) : mapOpen_;
    bool wheelOnLog = wide ? CheckCollisionPointRec(m, WIDE_LOG) : logOpen_;
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
    // While the map is open on the phone, + and - and the wheel zoom it
    // instead; on the PC only the wheel over it does.
    if (wheelOnMap) {
        int mapStep = wide ? 0 : step;
        if (wheel > 0.0f) mapStep++;
        if (wheel < 0.0f) mapStep--;
        mapZoom_ = std::clamp(mapZoom_ + mapStep, 0, MAP_ZOOM_COUNT - 1);
        if (!wide) step = 0;
        wheel = 0.0f;
    }
    if (wheelOnLog) {
        if (wheel > 0.0f) logScroll_++;
        if (wheel < 0.0f) logScroll_--;
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
    // Dragging the log down shows older entries, one per row dragged.
    if (logShown && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, logArea)) {
        dragging_ = true;
        dragFrom_ = m.y;
        dragScroll_ = logScroll_;
    }
    if (!logShown || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragging_ = false;
    if (dragging_) logScroll_ = dragScroll_ + int((m.y - dragFrom_) / ui::ROW);
    // Dragging the map moves it under the finger, and it stops following.
    if (mapShown && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, mapArea)) {
        mapDragging_ = true;
        mapDragFrom_ = m;
        Vec2i at = world_->human(follow_).cell;
        mapDragCentre_ = mapFollow_ ? Vector2{at.x + 0.5f, at.y + 0.5f} : mapCentre_;
    }
    if (!mapShown || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) mapDragging_ = false;
    if (mapDragging_) {
        float px = float(MAP_ZOOMS[mapZoom_]);
        Vector2 moved{(m.x - mapDragFrom_.x) / px, (m.y - mapDragFrom_.y) / px};
        if (!mapFollow_ || std::fabs(moved.x) + std::fabs(moved.y) > 0.0f) {
            mapFollow_ = false;
            mapCentre_ = {mapDragCentre_.x - moved.x, mapDragCentre_.y - moved.y};
        }
    }
    mapTime_ += dt;
    for (float left = dt * SPEEDS[speed_]; left > 0.0f; left -= MAX_TICK) world_->update(std::min(left, MAX_TICK));
    easeCamera(dt);
    hearing_.update(*world_, world_->human(follow_), SPEEDS[speed_], reveal_);
    if (world_->human(0).dead()) record();
}

void WorldScene::easeCamera(float dt) {
    Vector2 feet = feetOf(world_->human(follow_));
    float zoom = zoomNow();
    Vec2i at{int(std::lround(feet.x * zoom)), int(std::lround(feet.y * zoom))};
    if (focusOf_ != follow_) {
        focus_ = feet;
        focusOf_ = follow_;
        lag_ = {0, 0};
        drawnAt_ = at;
        lagZoom_ = zoom_;
        return;
    }
    float k = 1.0f - std::exp(-CAMERA_CATCH * dt);
    focus_.x += (feet.x - focus_.x) * k;
    focus_.y += (feet.y - focus_.y) * k;
    float dx = feet.x - focus_.x, dy = feet.y - focus_.y, d = std::hypot(dx, dy);
    if (d > CAMERA_LAG_MAX) focus_ = {feet.x - dx * CAMERA_LAG_MAX / d, feet.y - dy * CAMERA_LAG_MAX / d};
    // The camera is drawn trailing the viewer by lag_, whole canvas pixels,
    // so the viewer and the world never round apart and take turns, which
    // shakes the walker a pixel back and forth. lag_ follows focus_ a pixel
    // a frame, and grows only on a frame the viewer steps that way too, so
    // the world holds still then instead of stepping back.
    Vec2i want{int(std::lround((feet.x - focus_.x) * zoom)), int(std::lround((feet.y - focus_.y) * zoom))};
    // While the zoom eases it keeps the lag it wants, as on any change of zoom.
    if (lagZoom_ != zoom_ || wokeFor_ < ZOOM_EASE) lag_ = want, lagZoom_ = zoom_;
    auto settle = [](int& lag, int want, int stepped) {
        if (want == lag) return;
        int toward = want > lag ? 1 : -1;
        bool growing = lag == 0 || (lag > 0) == (toward > 0);
        if (!growing || stepped * toward > 0) lag += toward;
    };
    settle(lag_.x, want.x, at.x - drawnAt_.x);
    settle(lag_.y, want.y, at.y - drawnAt_.y);
    drawnAt_ = at;
}

Vector2 WorldScene::cameraFocus(const Human& viewer) const {
    float zoom = zoomNow();
    Vector2 at = drawnFeet(viewer, zoom);
    if (lagZoom_ != zoom_ || wokeFor_ < ZOOM_EASE) {
        Vector2 feet = feetOf(viewer);
        return {at.x - std::round((feet.x - focus_.x) * zoom) / zoom, at.y - std::round((feet.y - focus_.y) * zoom) / zoom};
    }
    return {at.x - float(lag_.x) / zoom, at.y - float(lag_.y) / zoom};
}

// Each entry is its time, dimmed, then what happened, wrapped under itself.
// The newest sits at the bottom, over the clock, and older ones stack up
// above it; whatever doesn't fit at the top is scrolled to.
void WorldScene::drawLog(const Human& viewer, Rectangle area, int clear) {
    if (logFor_ != viewer.id) logFor_ = viewer.id, logScroll_ = 0, dragging_ = false;
    DrawRectangleRec(area, LOG_SHADE);
    const auto& entries = viewer.log.entries();
    int left = int(area.x) + MARGIN, right = int(area.x + area.width) - MARGIN;
    int top = int(area.y) + MARGIN, y = int(area.y + area.height) - MARGIN - clear;
    // The bottom row is the clock now, live, in the times' column, so the
    // entries read as how long ago.
    std::string now = ui::clockText(world_->time());
    y -= ui::ROW;
    ui::text(now.c_str(), left, y, ui::DIM);
    if (entries.empty()) {
        ui::textCentred("NOTHING YET", (left + right) / 2, y - ui::ROW, ui::DIM);
        return;
    }
    logScroll_ = std::clamp(logScroll_, 0, int(entries.size()) - 1);
    if (logScroll_ > 0) {
        std::string newer = std::to_string(logScroll_) + " NEWER";
        y -= ui::ROW;
        ui::textRight(newer.c_str(), right, y, ui::DIM);
    }
    // The clock is the longest time; measured in zeros, as 1 is narrower.
    std::string widest = now;
    for (char& c : widest)
        if (std::isdigit(static_cast<unsigned char>(c))) c = '0';
    int column = ui::textWidth(widest.c_str()) + LOG_TIME_GAP;
    for (int i = int(entries.size()) - 1 - logScroll_; i >= 0; i--) {
        const LifeLog::Entry& e = entries[size_t(i)];
        std::vector<std::string> rows = ui::wrap(logLine(viewer, e), right - left - column);
        int block = int(rows.size()) * ui::ROW;
        if (y - block < top) break;
        y -= block;
        ui::text(ui::clockText(e.time).c_str(), left, y, ui::DIM);
        for (size_t r = 0; r < rows.size(); r++) ui::text(rows[r].c_str(), left + column, y + int(r) * ui::ROW);
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
void WorldScene::drawStats(const Human& viewer, Rectangle area) {
    DrawRectangleRec(area, LOG_SHADE);
    int x = int(area.x) + MARGIN, right = int(area.x + area.width) - MARGIN;
    int y = int(area.y) + MARGIN;
    const Traits& t = viewer.traits;
    y = section("TRAITS", x, y);
    y = pipRow("BRAVE", t.brave, x, right, y);
    y = pipRow("GREEDY", t.greedy, x, right, y);
    y = pipRow("CURIOUS", t.curious, x, right, y);
    y = pipRow("SOCIAL", t.sociable, x, right, y);
    y = pipRow("MODEST", viewer.modesty(), x, right, y);
    // Whom they have greeted, been told the name of, count friends and foes,
    // seen or heard of, and know they killed.
    y = section("PEOPLE", x, y + SECTION_GAP);
    y = countRow("MET", int(viewer.met.size()), x, right, y);
    y = countRow("NAMES", int(viewer.memory.names.size()), x, right, y);
    y = countRow("FRIENDS", viewer.friendCount(), x, right, y);
    y = countRow("FOES", viewer.foeCount(), x, right, y);
    y = countRow("KILLS", viewer.killCount(), x, right, y);
    // How frightened and erratic they are, and what eating flesh left:
    // guilt, or a taste for it; the mind red once erratic.
    y = section("MIND", x, y + SECTION_GAP);
    y = pipRow("FEAR", viewer.fright(), x, right, y);
    y = pipRow("HOPE", viewer.hope(), x, right, y);
    y = pipRow("ERRATIC", viewer.erratic(), x, right, y, viewer.isErratic() ? ERRATIC_INK : ui::INK);
    y = pipRow("GUILT", viewer.guilt(), x, right, y);
    y = pipRow("CRAVE", viewer.appetite(), x, right, y);
    // Breath left for running, how badly they need to go, and how long
    // they have gone unwashed.
    y = section("BODY", x, y + SECTION_GAP);
    y = pipRow("BREATH", viewer.breath(), x, right, y);
    y = pipRow("BLADDER", viewer.bladder, x, right, y);
    y = pipRow("BOWELS", viewer.bowels, x, right, y);
    y = pipRow("GRIME", viewer.grime(), x, right, y);
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
    const Human& viewer = world_->human(follow_);
    if (focusOf_ != follow_) easeCamera(0.0f);
    const Vector2 focus = cameraFocus(viewer);
    bool wide = canvas.wide();
    Rectangle view = wide ? WIDE_VIEW : PHONE_VIEW;
    const float zoom = zoomNow();
    drawWorld(*world_, viewer, [this](const Human& h) -> const HumanSprites& { return spritesFor(h); },
              floorTiles_, statueArt_, reveal_, zoom, view, focus);
    if (choosing()) {
        drawSleeperZs(viewer, view, focus);
        drawChoosing(canvas.mouse(), wide, 0.0f);
        return;
    }
    // Under the pause menu, and while the strips slide, nothing else takes a tap.
    bool sliding = wokeFor_ < 2.0f * SLIDE_TIME;
    Vector2 mouse = paused_ || sliding ? Vector2{-1.0f, -1.0f} : canvas.mouse();
    if (wide) {
        std::vector<std::string> rows = ui::wrap(doing(viewer), WIDE_DOING_W);
        int stripY = Canvas::H - int(rows.size()) * ui::ROW - 2 * MARGIN;
        drawSpeech(*world_, viewer, reveal_, zoom, view, 0, stripY, focus);
        drawWide(viewer, mouse, rows, stripY);
        if (sliding) drawChoosing(mouse, true, wokeFor_ / (2.0f * SLIDE_TIME));
    } else if (wokeFor_ < SLIDE_TIME) {
        drawChoosing(mouse, false, wokeFor_ / SLIDE_TIME);
    } else if (sliding) {
        // The HUD comes in from the edges, the top half (with the buttons)
        // down from above, the bottom half up from below.
        float in = 1.0f - (wokeFor_ - SLIDE_TIME) / SLIDE_TIME;
        for (int half = 0; half < 2; half++) {
            BeginScissorMode(0, half * Canvas::H / 2, Canvas::W, Canvas::H / 2);
            rlPushMatrix();
            rlTranslatef(0.0f, std::round(half ? in * BOTTOM_H : -in * TOP_H), 0.0f);
            drawPhone(viewer, mouse);
            rlPopMatrix();
            EndScissorMode();
        }
    } else {
        if (!logOpen_ && !statsOpen_ && !mapOpen_)
            drawSpeech(*world_, viewer, reveal_, zoom, view, TOP_H, BOTTOM_Y, focus);
        drawPhone(viewer, mouse);
    }
    if (paused_) drawPause(canvas.mouse(), canvas.width());
}

void WorldScene::drawSleeperZs(const Human& sleeper, Rectangle view, Vector2 focus) {
    float zoom = zoomNow();
    Vector2 cam = cameraFor(focus, zoom, {view.width, view.height}), feet = drawnFeet(sleeper, zoom);
    Vector2 at{view.x + (feet.x - cam.x) * zoom, view.y + (feet.y - cam.y) * zoom};
    spritesFor(sleeper).drawWakingZs(at, sleeper.facingLeft, int(zoom), chooseTime_);
}

// out runs from 0, in place, to 1, slid off: on the phone the strips off the
// top and bottom edges, on the PC the column off the left.
void WorldScene::drawChoosing(Vector2 mouse, bool wide, float out) {
    out = std::clamp(out, 0.0f, 1.0f);
    const int width = wide ? CHOOSE_WIDE_W : Canvas::W, cx = width / 2;
    // On the PC the two strips stand one over the other in the middle.
    const int topY = wide ? Canvas::H / 2 - CHOOSE_TOP_H - CHOOSE_PAD : 0;
    const int bottomY = wide ? Canvas::H / 2 : Canvas::H - CHOOSE_BOTTOM_H;
    auto slide = [](float dx, float dy) {
        rlPushMatrix();
        rlTranslatef(std::round(dx), std::round(dy), 0.0f);
    };
    if (wide) {
        slide(-out * width, 0.0f);
        DrawRectangle(0, 0, width, Canvas::H, ui::PAPER);
        DrawRectangle(width - 1, 0, 1, Canvas::H, COLUMN_EDGE);
    } else {
        slide(0.0f, -out * CHOOSE_TOP_H);
        ui::panel({0, 0, float(width), float(CHOOSE_TOP_H)});
    }

    // The name to tap and type over. A phone has no keyboard to type with,
    // so there the rolled name stays.
    const Rectangle field{float(cx - CHOOSE_NAME_W / 2), float(topY + CHOOSE_NAME_Y - 4), float(CHOOSE_NAME_W),
                          float(CHOOSE_NAME_H)};
    const int nameY = topY + CHOOSE_NAME_Y, hintY = topY + CHOOSE_HINT_Y;
    bool overName = !files::TOUCH_ONLY && CheckCollisionPointRec(mouse, field);
    if (editing_ || overName) DrawRectangleRec(field, ui::GROUND);
    std::string shown = caps(chosen_.empty() && !editing_ ? world_->human(0).name : chosen_);
    ui::textCentred(shown.c_str(), cx, nameY, overName || editing_ ? WHITE : ui::BRIGHT);
    if (editing_) {
        if (std::fmod(caret_, 2 * CARET_BLINK) < CARET_BLINK)
            DrawRectangle(cx + ui::textWidth(shown.c_str()) / 2 + 1, nameY - 1, 1, 10, WHITE);
        ui::textCentred("TYPE A NAME, ENTER WHEN DONE", cx, hintY, ui::DIM);
    } else if (!files::TOUCH_ONLY) {
        ui::textCentred("TAP THE NAME TO CHANGE IT", cx, hintY, ui::DIM);
    }
    // A press on the name starts typing; one anywhere else ends it.
    if (out == 0.0f && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (overName && !editing_) startEdit_ = true;
        if (!overName && editing_) {
            editing_ = false;
            while (!chosen_.empty() && chosen_.back() == ' ') chosen_.pop_back();
        }
    }
    back_ |= ui::button(CHOOSE_BACK, "BACK", mouse);
    if (!wide) {
        rlPopMatrix();
        slide(0.0f, out * CHOOSE_BOTTOM_H);
        ui::panel({0, float(bottomY), float(width), float(CHOOSE_BOTTOM_H)});
    }

    // What they are like, 0 to 9 pips each, as the stats will count them.
    const Traits& t = world_->human(0).traits;
    const char* labels[TRAITS] = {"BRAVE", "GREEDY", "CURIOUS", "SOCIAL"};
    const float values[TRAITS] = {t.brave, t.greedy, t.curious, t.sociable};
    const int step = TRAIT_BAR_W / TRAIT_PIPS;
    for (int i = 0; i < TRAITS; i++) {
        int y = bottomY + CHOOSE_PAD + i * TRAIT_ROW;
        ui::text(labels[i], cx - TRAIT_BAR_W, y);
        int pips = int(values[i] * 9.99f);
        for (int p = 0; p < TRAIT_PIPS; p++)
            DrawRectangle(cx + p * step, y + 2, step - 1, TRAIT_BAR_H, p < pips ? ui::INK : ui::GROUND);
    }
    // Two buttons across the bottom, with the same margin everywhere.
    const float buttonW = (width - 3 * CHOOSE_PAD) / 2.0f;
    const float buttonY = float(bottomY + CHOOSE_BOTTOM_H - CHOOSE_PAD - CHOOSE_BUTTON_H);
    refresh_ |= ui::button({float(CHOOSE_PAD), buttonY, buttonW, float(CHOOSE_BUTTON_H)}, "REFRESH", mouse);
    wake_ |= ui::button({width - CHOOSE_PAD - buttonW, buttonY, buttonW, float(CHOOSE_BUTTON_H)}, "WAKE UP", mouse);
    rlPopMatrix();
}

void WorldScene::drawPhone(const Human& viewer, Vector2 mouse) {
    char line[64];

    // Top strip. Who is followed and what they carry: the one name it is
    // sure of is its own, underlined, and tapping it opens their stats.
    ui::panel({0, 0, Canvas::W, TOP_H});
    // First the world's size, how long it has run, and whether it is all revealed.
    int y = MARGIN;
    std::snprintf(line, sizeof line, "HUMANS %zu  ROOMS %zu  %s", world_->livingCount(), world_->chunkCount(),
                  ui::clockText(world_->time()).c_str());
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
    x = item(x - ITEM_GAP, y, viewer.keys, drawKeyIcon);
    bottleItems(viewer, x - ITEM_GAP, y);

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

    // The dead's NEXT button sits over the bottom of the log: the newest
    // entries stay above it.
    if (logOpen_) drawLog(viewer, LOG_AREA, viewer.dead() ? int(LOG_AREA.y + LOG_AREA.height - NEXT_BUTTON.y) : 0);
    if (statsOpen_) drawStats(viewer, LOG_AREA);
    if (mapOpen_) drawMapIn(viewer, LOG_AREA);

    drawControls({ZOOM_IN_BUTTON, ZOOM_OUT_BUTTON, SPEED_UP_BUTTON, SPEED_SLOT, SPEED_DOWN_BUTTON, PAUSE_BUTTON},
                 mouse);
    if (viewer.dead()) nextHuman_ |= ui::button(NEXT_BUTTON, "NEXT", mouse);
}

// The same as the phone shows, laid out down one column: no strips over the
// world, and stats, log and map always open.
void WorldScene::drawWide(const Human& viewer, Vector2 mouse, const std::vector<std::string>& doingRows,
                          int stripY) {
    char line[64];
    drawStats(viewer, WIDE_STATS);
    drawLog(viewer, WIDE_LOG);
    for (int x : {COLUMN_W, 2 * COLUMN_W - 1, HUD_X}) DrawRectangle(x, 0, 1, Canvas::H, COLUMN_EDGE);

    // Over the stats, who is followed and what they carry.
    int x = item(STATS_RIGHT, WIDE_NAME_Y, viewer.cakes, drawCakeIcon);
    x = item(x - ITEM_GAP, WIDE_NAME_Y, viewer.keys, drawKeyIcon);
    x = bottleItems(viewer, x - ITEM_GAP, WIDE_NAME_Y);
    // A long name is broken off before the items rather than run under them.
    std::vector<std::string> nameRows = ui::wrap(caps(viewer.name), x - ITEM_GAP - STATS_LEFT);
    if (!nameRows.empty()) ui::text(nameRows.front().c_str(), STATS_LEFT, WIDE_NAME_Y, ui::BRIGHT);

    // Health and the needs a row each, their bars lined up past the widest
    // label.
    const char* labels[] = {"HP", "HUNGER", "THIRST", "TIRED"};
    const float amounts[] = {viewer.health, viewer.hunger, viewer.thirst, viewer.tiredness};
    const Color fills[] = {HEALTH_BAR, HUNGER_BAR, THIRST_BAR, TIRED_BAR};
    static_assert(int(std::size(labels)) == WIDE_BARS);
    int barX = 0;
    for (const char* l : labels) barX = std::max(barX, ui::textWidth(l));
    barX += STATS_LEFT + 3;
    for (int i = 0; i < WIDE_BARS; i++)
        bar(labels[i], STATS_LEFT, STATS_RIGHT, WIDE_BARS_Y + 2 + i * ui::ROW, amounts[i], fills[i], barX);

    // Right of the world, its size, how long it has run, and whether it is all revealed.
    std::snprintf(line, sizeof line, "HUMANS %zu  ROOMS %zu  %s", world_->livingCount(), world_->chunkCount(),
                  ui::clockText(world_->time()).c_str());
    ui::text(line, HUD_LEFT, MARGIN, ui::DIM);
    if (reveal_) ui::textRight("REVEAL", HUD_RIGHT, MARGIN, REVEAL_INK);

    // What they are doing, centred along the bottom of the world.
    ui::panel({WIDE_VIEW.x, float(stripY), WIDE_VIEW.width, float(Canvas::H - stripY)});
    int cx = int(WIDE_VIEW.x + WIDE_VIEW.width / 2), y = stripY + MARGIN;
    for (const std::string& row : doingRows) {
        ui::textCentred(row.c_str(), cx, y);
        y += ui::ROW;
    }

    drawControls({wideButtonAt(0), wideButtonAt(1), wideButtonAt(2), wideButtonAt(3), wideButtonAt(4),
                  wideButtonAt(5)},
                 mouse);
    drawMapIn(viewer, WIDE_MAP);
    Rectangle next{cx - WIDE_NEXT_W / 2, stripY - MARGIN - WIDE_NEXT_H, WIDE_NEXT_W, WIDE_NEXT_H};
    if (viewer.dead()) nextHuman_ |= ui::button(next, "NEXT", mouse);
}

void WorldScene::drawMapIn(const Human& viewer, Rectangle area) {
    Vector2 centre = mapFollow_ ? Vector2{viewer.cell.x + 0.5f, viewer.cell.y + 0.5f} : mapCentre_;
    drawMap(*world_, viewer, area, MAP_ZOOMS[mapZoom_], centre, mapTime_);
}

void WorldScene::drawControls(const Controls& c, Vector2 mouse) {
    char line[16];
    if (ui::button(c.zoomIn, "+", mouse)) zoomStep_++;
    if (ui::button(c.zoomOut, "-", mouse)) zoomStep_--;
    if (ui::arrowButton(c.speedUp, true, mouse)) speedStep_++;
    ui::panel(c.speed);
    std::snprintf(line, sizeof line, "X%d", SPEEDS[speed_]);
    ui::textCentred(line, int(c.speed.x + c.speed.width / 2), int(c.speed.y) + 3, ui::BRIGHT);
    if (ui::arrowButton(c.speedDown, false, mouse)) speedStep_--;
    pauseToggle_ |= ui::button(c.pause, "II", mouse);
}

// Leaving with the human you made alive leaves them behind, so it says so.
// It sits in the middle of whichever canvas is up, width wide.
void WorldScene::drawPause(Vector2 mouse, int width) {
    float shift = (width - Canvas::W) / 2.0f;
    auto shifted = [shift](Rectangle r) { return Rectangle{r.x + shift, r.y, r.width, r.height}; };
    Rectangle menu = shifted(MENU_BUTTON);
    DrawRectangle(0, 0, width, Canvas::H, PAUSE_SHADE);
    if (configOpen_) {
        if (config_.draw(mouse, width)) configOpen_ = false;
        return;
    }
    ui::textCentred("PAUSED", width / 2, PAUSE_TITLE_Y, ui::BRIGHT);
    pauseToggle_ |= ui::button(shifted(RESUME_BUTTON), "RESUME", mouse);
    configOpen_ |= ui::button(shifted(CONFIG_BUTTON), "CONFIGURATION", mouse);
    toMenu_ |= ui::button(menu, "MAIN MENU", mouse);
    if (!recorded_) {
        std::string warning = caps(world_->human(0).name) + " WILL BE LEFT BEHIND";
        int y = int(menu.y + menu.height) + PAUSE_GAP;
        for (const std::string& row : ui::wrap(warning, Canvas::W - 2 * MARGIN * 4)) {
            ui::textCentred(row.c_str(), width / 2, y, ui::DIM);
            y += ui::ROW;
        }
    }
}
