#pragma once
#include "world/statue.h"
#include <raylib.h>
#include <unordered_map>

/// Statues as textures, each painted the first time it is drawn.
class StatueArt {
public:
    StatueArt() = default;
    ~StatueArt();
    StatueArt(const StatueArt&) = delete;
    StatueArt& operator=(const StatueArt&) = delete;

    /// Draws the statue with the bottom-left corner of its base at canvas
    /// pixel x, y, dimmed unless lit. id tells statues apart, as an index
    /// into the world's statues.
    void draw(const Statue& s, int id, int x, int y, bool lit) const;

private:
    mutable std::unordered_map<int, Texture2D> textures_;
};
