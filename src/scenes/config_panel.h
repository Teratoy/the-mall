#pragma once
#include <raylib.h>

/// CONFIGURATION: the settings kept between runs, a row each with - and +
/// either side of a bar of steps, and BACK under them. For now only the
/// sound: MASTER and SOUNDS. A change is saved at once and played as a
/// pop, so the player hears what they set. The main menu shows it as its
/// own screen (ConfigScene), and the pause menu over the world.
class ConfigPanel {
public:
    /// Draws it down the middle of a canvas width wide. True on the frame
    /// BACK is let go of.
    bool draw(Vector2 mouse, int width);
};
