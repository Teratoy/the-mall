#pragma once
#include "core/rng.h"
#include "scenes/scene.h"
#include <vector>

/// The first screen: the game's name, ENTER THE MALL to make a human
/// (or ENTER), and OBITUARY for the humans you made whose runs are over.
/// Behind them a maze carves itself out without end, its old corridors
/// fading back into wall for it to carve again.
class MenuScene : public Scene {
public:
    MenuScene();
    void update(float dt, const Canvas& canvas) override;
    void draw(const Canvas& canvas) override;

private:
    /// Starts carving from a random cell of an empty maze.
    void startMaze();
    /// One step of the carving: on to a neighbour that is wall again, or back.
    void carveStep();
    /// Opens a cell, or keeps it open, from now.
    void dig(int cell);
    bool isOpen(int cell) const;
    void drawMaze() const;

    /// When each cell of the maze was last dug, a row after another;
    /// negative for wall. Dug cells fade back into wall.
    std::vector<float> dugAt_;
    /// The path back from the cell being carved, as cell indices.
    std::vector<int> trail_;
    /// The menu's own clock, and steps owed to the carving.
    float clock_ = 0.0f, carveDebt_ = 0.0f;
    Rng mazeRng_;
    bool enter_ = false;
    bool obituary_ = false;
};
