#pragma once

class World;
class Human;

/// Shades the cells the viewer knows, from x0, y0 to x1, y1, by how lit
/// they are: in sight, deep blue-black in the dark, the source's own colour
/// in full light, and a little round the viewer whose eyes get used to the
/// dark; remembered, greyed by the light they were seen in. The edge of
/// sight fades into memory. Cells meet in an ordered dither, so light falls
/// off in pixel steps, never a smear. Drawn in world pixels, with left, top
/// the world pixel at the view's corner, inside the view's camera.
void drawLight(const World& world, const Human& viewer, bool reveal, int x0, int y0, int x1, int y1, int left,
               int top);
