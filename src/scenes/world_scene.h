#pragma once
#include "render/floor_tiles.h"
#include "render/human_sprites.h"
#include "render/statue_art.h"
#include "scenes/scene.h"
#include "world/world.h"
#include <memory>
#include <string>
#include <unordered_map>

class Human;

/// The world, with every human left to explore it. The camera follows one
/// of them, starting with the one you made, and shows what they know,
/// between a strip about them at the top and their bars and doings at the
/// bottom. The buttons down the right edge: + and - (or the wheel, or the
/// keys) zoom in and out, the arrows either side of the speed step it
/// between X1 and X16, and LOG (or L) opens the followed human's log over
/// the world, newest first, scrolled with the wheel or a drag. Tapping
/// the followed human's name at the top (or S) opens their stats in the
/// same place instead: traits, people, mind and lessons. The camera
/// stays with the followed human while they live; once they die, NEXT
/// moves it to the nearest living human, or starts over if nobody is
/// left. Over the world, what the humans in view are saying shows in
/// bubbles. The pause button under the speed (or ESC) stops the world
/// and offers RESUME or MAIN MENU. The human you made goes into the
/// obituary when they die, or as left behind if the world is left with
/// them alive. Debug keys: 1-5 set the speed, TAB reveals everything, F5
/// reads data/lines.txt again.
class WorldScene : public Scene {
public:
    /// humanSeed names and shapes the human you made; seed grows the world.
    /// An empty name gives the one grown from humanSeed.
    WorldScene(const HumanLook& look, uint64_t humanSeed, uint64_t seed, const std::string& name = {});
    ~WorldScene() override;

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    /// tools/shot.cpp opens menus and keeps its worlds out of the obituary.
    friend struct ShotProbe;
    uint64_t seed_;
    World world_;
    /// Made on first draw, per human id.
    std::unordered_map<int, std::unique_ptr<HumanSprites>> sprites_;
    FloorTiles floorTiles_;
    StatueArt statueArt_;

    /// The human the camera follows; 0 is the one you made.
    int follow_ = 0;
    /// An index into the zoom levels.
    int zoom_ = 2;
    /// A zoom button's step, reported while drawing.
    int zoomStep_ = 0;
    /// An index into the speeds.
    int speed_ = 0;
    /// A speed arrow's step, reported while drawing.
    int speedStep_ = 0;
    bool reveal_ = false;
    /// The NEXT button was tapped while drawing.
    bool nextHuman_ = false;
    /// The log is open, scrolled this many entries back from the newest,
    /// for this human; a drag started at dragFrom_ with the log scrolled
    /// to dragScroll_, if dragging_.
    bool logOpen_ = false;
    bool logToggle_ = false;
    int logScroll_ = 0;
    int logFor_ = -1;
    bool dragging_ = false;
    float dragFrom_ = 0.0f;
    int dragScroll_ = 0;
    /// The stats are open in the log's place; the two never show at once.
    bool statsOpen_ = false;
    bool statsToggle_ = false;
    /// The world is stopped under the pause menu; its buttons report while
    /// drawing.
    bool paused_ = false;
    bool pauseToggle_ = false;
    bool toMenu_ = false;
    /// The human you made is in the obituary already.
    bool recorded_ = false;

    const HumanSprites& spritesFor(const Human& h);
    /// Puts the human you made into the obituary, dead or left behind.
    void record();
    void drawPause(Vector2 mouse);
    /// The living human nearest the followed one, if anyone is left.
    const Human* nearestLiving() const;
    /// The followed human's log, newest first, in the space between the
    /// strips left of the buttons.
    void drawLog(const Human& viewer);
    /// The followed human's stats, in the same space as the log.
    void drawStats(const Human& viewer);
};
