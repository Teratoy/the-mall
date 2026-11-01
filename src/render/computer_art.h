#pragma once
#include "core/color.h"

/// A CRT terminal stands on its own pedestal in one cell, a little shorter
/// than a human: a putty screen with its keyboard on a tray, on a stalk.
inline constexpr int TERMINAL_W = 6;
inline constexpr int TERMINAL_H = 13;

/// A console desk lies six cells wide and four deep against a wall, set up
/// for one human: a wing comes forward each side round the chair, and on
/// it stand screens of all sizes, an instrument box with a round scope,
/// a printer, a phone and the clutter of someone who works there.
inline constexpr int CONSOLE_CELLS_W = 6, CONSOLE_CELLS_D = 4;
inline constexpr int CONSOLE_W = 36;
/// The screens stand this far above the desk's footprint, so the picture is this tall in all.
inline constexpr int CONSOLE_RISE = 9;
inline constexpr int CONSOLE_H = CONSOLE_RISE + 24;

/// The terminal's pixel at gx, gy seen from the front at the given tick of
/// its clock, its screen scrolling text, or typed at, asking for a password;
/// clear where nothing is drawn.
Rgba terminalPixel(int gx, int gy, int tick, bool typed = false);

/// Draws the terminal with the bottom-left corner of its foot at canvas
/// pixel x, y, dimmed unless in sight, ticking with the seconds gone.
/// Remembered, it holds still.
void drawTerminal(int x, int y, bool inSight, float seconds, bool typed = false);

/// The console desk's pixel at gx, gy, gy 0 being the top of the screens
/// and CONSOLE_RISE the back edge of its footprint, at the given tick: each
/// screen scrolls its own text, the panels' lamps blink and the scope sweeps;
/// typed at, the screen before the chair asks for a password.
Rgba consoleDeskPixel(int gx, int gy, int tick, bool typed = false);

/// Draws the console desk with its footprint's top-left corner at canvas
/// pixel x, y, the screens rising CONSOLE_RISE above it, dimmed unless in sight.
/// Remembered, it holds still.
void drawConsoleDesk(int x, int y, bool inSight, float seconds, bool typed = false);
