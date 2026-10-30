#pragma once
#include "core/color.h"
#include <cstdint>

/// The cake vending machine stands two cells wide against a wall, as tall as a human.
inline constexpr int VENDING_W = 12;
inline constexpr int VENDING_H = 21;
/// Seen from the side it is this deep.
inline constexpr int VENDING_DEPTH = 9;
/// It shows cakes on three shelves of two: bit i of stock is slot i, left to right, top to bottom.
inline constexpr int VENDING_SLOTS = 6;
inline constexpr uint8_t VENDING_FULL = (1u << VENDING_SLOTS) - 1;

/// The machine's pixel at gx, gy, seen from the front, with the cakes it
/// still holds behind the glass; clear where nothing is drawn. The LEDs
/// round the sign swap colours each blink.
Rgba vendingPixel(int gx, int gy, uint8_t stock, int blink);

/// Draws the machine with the bottom-left corner of its foot at canvas
/// pixel x, y, dimmed unless lit, its LEDs blinking with the seconds gone.
/// Remembered, they hold still.
void drawVendingMachine(int x, int y, uint8_t stock, bool lit, float seconds);

/// The machine's pixel at gx, gy seen from its right side, the mall's
/// spiral and a cake painted on its flank.
Rgba vendingSidePixel(int gx, int gy, int blink);

/// Draws the machine seen from its right side, as drawVendingMachine draws its front.
void drawVendingMachineSide(int x, int y, bool lit, float seconds);
