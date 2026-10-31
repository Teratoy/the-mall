#pragma once
#include <raylib.h>

class World;
class Human;

/// What the humans on the viewer's screen are saying, each line in a
/// bubble over its speaker's head with a tail down to it, edged in the
/// colour of its tone. Drawn in canvas pixels over the world, so the text
/// stays sharp at any zoom; bubbles that would overlap are stacked upward,
/// and all are kept inside view, the part of the canvas drawWorld showed
/// the world in, and between top and bottom, clear of the HUD strips.
/// focus is the camera's, as drawWorld was given it.
void drawSpeech(const World& world, const Human& viewer, bool reveal, float zoom, Rectangle view, int top,
                int bottom, Vector2 focus);
