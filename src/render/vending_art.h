#pragma once
#include "core/color.h"
#include <cstdint>

/// A vending machine stands two cells wide against a wall, as tall as a human.
inline constexpr int VENDING_W = 12;
inline constexpr int VENDING_H = 21;
/// Seen from the side it is this deep.
inline constexpr int VENDING_DEPTH = 9;

/// What a machine sells, each in its own colours: cakes on three shelves of
/// two, empty bottles on two shelves of three, guns on two shelves of one.
enum class Vend : uint8_t { Cake, Bottle, Gun, Count };

/// How many slots it has; bit i of stock is slot i, left to right, top to bottom.
int vendingSlots(Vend kind);
/// Where slot i's goods sit on the front, their top-left: for the sprite editor's preview.
void vendingSlotAt(Vend kind, int slot, int& x, int& y);
/// Stock with every slot filled.
uint8_t vendingFull(Vend kind);

/// The machine's pixel at gx, gy, seen from the front, with what it still
/// holds behind the glass; clear where nothing is drawn. The LEDs round
/// the sign swap colours each blink.
Rgba vendingPixel(Vend kind, int gx, int gy, uint8_t stock, int blink);

/// Draws the machine with the bottom-left corner of its foot at canvas
/// pixel x, y, dimmed unless lit, its LEDs blinking with the seconds gone.
/// Remembered, they hold still.
void drawVendingMachine(Vend kind, int x, int y, uint8_t stock, bool lit, float seconds);

/// The machine's pixel at gx, gy seen from its right side, the mall's
/// spiral and what it sells painted on its flank.
Rgba vendingSidePixel(Vend kind, int gx, int gy, int blink);

/// Draws the machine seen from its right side, as drawVendingMachine draws its front.
void drawVendingMachineSide(Vend kind, int x, int y, bool lit, float seconds);
