#pragma once

class World;
class Human;

/// What the humans on the viewer's screen are saying, each line in a
/// bubble over its speaker's head with a tail down to it, edged in the
/// colour of its tone. Drawn in canvas pixels over the world, so the text
/// stays sharp at any zoom; bubbles that would overlap are stacked upward,
/// and all are kept between top and bottom, clear of the HUD strips.
void drawSpeech(const World& world, const Human& viewer, bool reveal, float zoom, int top, int bottom);
