#pragma once
#include "core/color.h"

/// A CRT terminal stands on its own pedestal in one cell, a little shorter
/// than a human: a putty screen with its keyboard on a tray, on a stalk.
inline constexpr int TERMINAL_W = 6;
inline constexpr int TERMINAL_H = 13;

/// A console desk lies six cells wide and three deep against a wall, set up
/// for one human: six screens along the back, the outer two turned in
/// towards the seat, a keyboard between two switch panels, a mug, a stack
/// of printout, and a swivel chair in front.
inline constexpr int CONSOLE_CELLS_W = 6, CONSOLE_CELLS_D = 3;
inline constexpr int CONSOLE_W = 36;
/// The screens stand this far above the desk's footprint, so the picture is this tall in all.
inline constexpr int CONSOLE_RISE = 6;
inline constexpr int CONSOLE_H = CONSOLE_RISE + 18;

/// The terminal's pixel at gx, gy seen from the front at the given tick of
/// its clock, its screen scrolling text; clear where nothing is drawn.
Rgba terminalPixel(int gx, int gy, int tick);

/// Draws the terminal with the bottom-left corner of its foot at canvas
/// pixel x, y, dimmed unless lit, ticking with the seconds gone.
/// Remembered, it holds still.
void drawTerminal(int x, int y, bool lit, float seconds);

/// The console desk's pixel at gx, gy, gy 0 being the top of the screens
/// and CONSOLE_RISE the back edge of its footprint, at the given tick: each
/// screen scrolls its own text and the panels' lamps blink.
Rgba consoleDeskPixel(int gx, int gy, int tick);

/// Draws the console desk with its footprint's top-left corner at canvas
/// pixel x, y, the screens rising CONSOLE_RISE above it, dimmed unless lit.
/// Remembered, it holds still.
void drawConsoleDesk(int x, int y, bool lit, float seconds);
