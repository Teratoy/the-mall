#pragma once
#include <raylib.h>

/// CONFIGURATION: the settings kept between runs, a row each with - and +
/// either side of a bar of steps, then the view's effects each ON or OFF
/// (render/view_effects.h), and BACK under them. The sound: MASTER, SOUNDS,
/// MUSIC and VOICES; the view: SNOW, LIGHT DITHER and SIGHT DITHER. A change
/// is saved at once and played as a pop, so the player hears what they set
/// (and the music, on the main menu, which plays under it). The main menu
/// shows it as its own screen (ConfigScene), and the pause menu over the world.
class ConfigPanel {
public:
    /// Draws it down the middle of a canvas width wide. True on the frame
    /// BACK is let go of.
    bool draw(Vector2 mouse, int width);
};
