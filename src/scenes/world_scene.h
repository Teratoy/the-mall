#pragma once
#include "audio/hearing.h"
#include "core/rng.h"
#include "render/floor_tiles.h"
#include "render/human_sprites.h"
#include "render/statue_art.h"
#include "scenes/config_panel.h"
#include "scenes/scene.h"
#include "world/world.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Human;

/// The world, with every human left to explore it. It opens on choosing the
/// human you will be: the spawn room, frozen, with them asleep in it where
/// they will come to, close up, their name and traits in the strips over
/// it. REFRESH (or R) lays another human there in the same room; tapping
/// the name lets you type one of your own (ENTER or a tap elsewhere ends
/// it), which REFRESH keeps; BACK (or ESC) goes to the menu, and WAKE UP
/// (or ENTER) starts the world: the strips slide away for the HUD, the
/// camera eases out to the play zoom, and they come to as everyone does.
///
/// The camera follows one
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
/// and offers RESUME, CONFIGURATION or MAIN MENU. The human you made goes into the
/// obituary when they die, or as left behind if the world is left with
/// them alive. Debug keys: 1-5 set the speed, TAB reveals everything, F5
/// reads data/lines.txt, data/sprites.txt and data/sounds.txt again.
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
    /// An empty name gives the one grown from humanSeed. It starts awake,
    /// with no choosing.
    WorldScene(const HumanLook& look, uint64_t humanSeed, uint64_t seed, const std::string& name = {});
    /// Opens on choosing the human, in a world grown from seed.
    explicit WorldScene(uint64_t seed);
    ~WorldScene() override;

    /// Opened from tools/genview.cpp for a look in play: the chunk it showed,
    /// this kind on this footprint, grown past the spawn before anyone walks
    /// (or, with no kind, the one the game rolls there), and the world kept
    /// out of the obituary.
    void tryOut(std::optional<ChunkKind> kind, const Footprint& parts);
    /// Starts the world at this step of the speeds (0 is X1), for watching
    /// several worlds run fast side by side.
    void startAtSpeed(int step);
    /// Writes every human's life log to this file when the world is left,
    /// for reading after a test run.
    void writeLogsTo(std::string path) { logsPath_ = std::move(path); }

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;
    bool canBeWide() const override { return true; }

private:
    /// tools/shot.cpp opens menus and keeps its worlds out of the obituary.
    friend struct ShotProbe;
    uint64_t seed_;
    /// Where to write every human's life log on leaving, if anywhere.
    std::string logsPath_;
    void writeLogs() const;
    /// Grown afresh from seed_ for each human rolled while choosing, so the
    /// spawn room is the same for all of them.
    std::unique_ptr<World> world_;
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
    /// How far the camera trails the viewer this frame, in whole canvas
    /// pixels at zoom lagZoom_, and where the viewer was drawn last frame,
    /// in canvas pixels. See easeCamera.
    Vec2i lag_{};
    Vec2i drawnAt_{};
    int lagZoom_ = -1;
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
    /// What the followed human hears, played as the world runs.
    Hearing hearing_;
    /// CONFIGURATION is open over the pause menu.
    bool configOpen_ = false;
    ConfigPanel config_;
    /// The human you made is in the obituary already.
    bool recorded_ = false;

    /// Choosing, before the world's first moment: the dice the human is
    /// rolled with, their look and seed, and the name typed in (empty for
    /// the rolled one), being typed while editing_. wokeFor_ is how long
    /// since WAKE UP, below 0 while choosing; chooseTime_ how long the
    /// choosing has gone on, for the sleeper's z's. The buttons report
    /// while drawing.
    Rng rolls_{0};
    HumanLook look_{};
    uint64_t humanSeed_ = 0;
    std::string chosen_;
    bool editing_ = false;
    bool startEdit_ = false;
    float caret_ = 0.0f;
    float wokeFor_ = 0.0f;
    float chooseTime_ = 0.0f;
    bool refresh_ = false;
    bool wake_ = false;
    bool back_ = false;

    bool choosing() const { return wokeFor_ < 0.0f; }
    /// Lays a fresh human asleep in the spawn room, the world grown again.
    void roll();
    /// Choosing: typing the name, the buttons and keys; true once the scene
    /// is done with this update.
    bool updateChoosing(float dt, const Canvas& canvas);
    /// Canvas pixels per world pixel now: close while choosing, easing out
    /// to the play zoom after WAKE UP.
    float zoomNow() const;
    /// The choosing strips over the world: the name at the top, the traits
    /// and buttons at the bottom; in the PC layout one column. out slides
    /// them away, from 0 in place to 1 gone.
    void drawChoosing(Vector2 mouse, bool wide, float out);
    /// The sleeper's z's over it, in the world drawn in view at focus.
    void drawSleeperZs(const Human& sleeper, Rectangle view, Vector2 focus);

    const HumanSprites& spritesFor(const Human& h);
    /// Puts the human you made into the obituary, dead or left behind.
    void record();
    /// Moves the camera's focus a frame of dt seconds nearer the followed human.
    void easeCamera(float dt);
    /// Where the camera looks this frame: the viewer as drawn, less lag_.
    Vector2 cameraFocus(const Human& viewer) const;
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
    /// The mall's time, HH:MM, starting x canvas pixels across, y down.
    void drawClock(int x, int y) const;
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
