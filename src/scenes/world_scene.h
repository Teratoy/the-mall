#pragma once
#include "render/floor_tiles.h"
#include "render/human_sprites.h"
#include "render/statue_art.h"
#include "scenes/scene.h"
#include "world/world.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Human;

/// The world, with every human left to explore it. The camera follows one
/// of them, starting with the one you made, and shows what they know,
/// between a strip about them at the top and their bars and doings at the
/// bottom. The buttons down the right edge: + and - (or the wheel, or the
/// keys) zoom in and out, the arrows either side of the speed step it
/// between X1 and X16, and LOG (or L) opens the followed human's log over
/// the world, newest at the bottom, scrolled with the wheel or a drag. Tapping
/// the followed human's name at the top (or S) opens their stats in the
/// same place instead: traits, people, mind and lessons. MAP beside LOG
/// (or M) opens their map there: all they remember, with symbols for what
/// they know of, dragged to look round and zoomed with + and -. The camera
/// stays with the followed human while they live; once they die, NEXT
/// moves it to the nearest living human, or starts over if nobody is
/// left. Over the world, what the humans in view are saying shows in
/// bubbles. The pause button under the speed (or ESC) stops the world
/// and offers RESUME or MAIN MENU. The human you made goes into the
/// obituary when they die, or as left behind if the world is left with
/// them alive. Debug keys: 1-5 set the speed, TAB reveals everything, F5
/// reads data/lines.txt and data/sprites.txt again.
///
/// In a window wider than tall it takes the PC layout instead: the world in
/// a square, with the stats and log side by side left of it, the name,
/// items and bars over the stats, what they are doing along the bottom of
/// the square, and right of it the world's size and buttons over the map. All three are always open there;
/// the wheel scrolls the log or zooms the map when over them, and zooms the
/// world anywhere else.
class WorldScene : public Scene {
public:
    /// humanSeed names and shapes the human you made; seed grows the world.
    /// An empty name gives the one grown from humanSeed.
    WorldScene(const HumanLook& look, uint64_t humanSeed, uint64_t seed, const std::string& name = {});
    ~WorldScene() override;

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;
    bool canBeWide() const override { return true; }

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
    /// Where the camera looks, in world pixels. It eases after the followed
    /// human's feet instead of being pinned to them, so the stops between
    /// steps and uneven frames glide. focusOf_ is whose feet it last eased
    /// after; for anyone else it jumps straight to them.
    Vector2 focus_{};
    int focusOf_ = -1;
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
    /// The map is open in the log's place, mapZoom_ into MAP_ZOOMS. It keeps
    /// the followed human in the middle until dragged, then stays at
    /// mapCentre_ (in cells) until opened again; a drag started at
    /// mapDragFrom_ with the map at mapDragCentre_.
    bool mapOpen_ = false;
    bool mapToggle_ = false;
    int mapZoom_ = 1;
    bool mapFollow_ = true;
    Vector2 mapCentre_{};
    bool mapDragging_ = false;
    Vector2 mapDragFrom_{}, mapDragCentre_{};
    float mapTime_ = 0.0f;
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
    /// Moves the camera's focus a frame of dt seconds nearer the followed human.
    void easeCamera(float dt);
    /// The six buttons that zoom, set the speed and pause, wherever the
    /// layout puts them; speed is the slot showing it, between its arrows.
    struct Controls {
        Rectangle zoomIn, zoomOut, speedUp, speed, speedDown, pause;
    };

    /// The pause menu over the whole canvas, width wide.
    void drawPause(Vector2 mouse, int width);
    /// The living human nearest the followed one, if anyone is left.
    const Human* nearestLiving() const;
    /// The phone's strips and buttons over the world, and whichever of the
    /// log, stats or map is open.
    void drawPhone(const Human& viewer, Vector2 mouse);
    /// The PC's columns beside the square of world, and what they are doing,
    /// in doingRows, in a strip from stripY down the bottom of the square.
    void drawWide(const Human& viewer, Vector2 mouse, const std::vector<std::string>& doingRows, int stripY);
    void drawControls(const Controls& c, Vector2 mouse);
    /// The followed human's log, newest at the bottom, in area: on the phone the
    /// space between the strips left of the buttons. Its bottom clear pixels
    /// are left free for a button over them.
    void drawLog(const Human& viewer, Rectangle area, int clear = 0);
    /// The followed human's stats, in area: on the phone the log's place.
    void drawStats(const Human& viewer, Rectangle area);
    /// Their map in area, following them or where it was dragged to.
    void drawMapIn(const Human& viewer, Rectangle area);
};
