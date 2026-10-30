#pragma once
#include "human/obituary.h"
#include "scenes/scene.h"
#include <raylib.h>
#include <vector>

/// The humans you made whose runs are over, on black: a button per name,
/// newest first, with how long they lasted. Tapping one shows how long
/// they survived, what ended them and their log, with CREMATE in the
/// corner to strike them off for good (tapped twice, the second time as
/// SURE?). Both pages scroll with
/// the wheel or a drag; BACK (or ESC) goes up a page, to the menu last.
class ObituaryScene : public Scene {
public:
    ObituaryScene();

    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    /// Oldest first, as the file keeps them.
    std::vector<obituary::Record> records_;
    /// The record open, or -1 for the list.
    int open_ = -1;
    /// Tapped while drawing: a record, or BACK.
    int tapped_ = -1;
    bool back_ = false;
    /// CREMATE was tapped once and waits for SURE?; tapped again while drawing.
    bool confirming_ = false;
    bool cremate_ = false;
    /// Pixels scrolled down; a drag started at dragFrom_ with dragScroll_,
    /// and once it moves, taps on rows no longer count.
    float scroll_ = 0.0f;
    bool dragging_ = false, dragged_ = false;
    float dragFrom_ = 0.0f, dragScroll_ = 0.0f;
    /// How far the open page can scroll, found while drawing.
    float maxScroll_ = 0.0f;

    void drawList(Vector2 mouse);
    void drawRecord(const obituary::Record& r);
};
