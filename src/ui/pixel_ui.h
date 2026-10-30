#pragma once
#include <raylib.h>
#include <string>
#include <vector>

/// Minimal pixel UI drawn on the canvas, in raylib's built-in 10px font.
/// Text is laid out from measured widths, so nothing is ever cut off:
/// right-aligned text ends where asked, and rows are ROW apart.
namespace ui {

inline constexpr Color INK = {185, 185, 185, 255};   // Pocket Rave's light grey
inline constexpr Color BRIGHT = {240, 240, 240, 255};
inline constexpr Color DIM = {120, 120, 120, 255};
inline constexpr Color PAPER = {24, 24, 24, 255};
inline constexpr Color GROUND = {68, 68, 68, 255};   // Pocket Rave's ground
/// A see-through backdrop that keeps text readable over the world.
inline constexpr Color SHADE = {0, 0, 0, 150};
/// One row of text: the font's height and a little air under it.
inline constexpr int ROW = 11;

void text(const char* s, int x, int y, Color c = INK);
void textCentred(const char* s, int cx, int y, Color c = INK);
/// Ends at right.
void textRight(const char* s, int right, int y, Color c = INK);
int textWidth(const char* s);
/// s broken at spaces into lines no wider than width, as far as words allow.
std::vector<std::string> wrap(const std::string& s, int width);
/// A strip of SHADE behind a block of text.
void panel(Rectangle r);
/// Seconds as M:SS, or H:MM:SS past the hour.
std::string clockText(float seconds);
/// A flat button. True on the frame a press is released over it.
bool button(Rectangle r, const char* label, Vector2 mouse);
/// A flat button with an arrow pointing up or down instead of a label.
bool arrowButton(Rectangle r, bool up, Vector2 mouse);

} // namespace ui
