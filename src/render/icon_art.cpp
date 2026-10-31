#include "render/icon_art.h"
#include "ui/pixel_ui.h"

namespace {

/// The spiral, # its wall, entered at the top left.
constexpr const char* SPIRAL_GLYPH[ICON_PX] = {
    "###############",
    "..............#",
    "#############.#",
    "#...........#.#",
    "#.#########.#.#",
    "#.#.......#.#.#",
    "#.#.#####.#.#.#",
    "#.#.#...#.#.#.#",
    "#.#.#.###.#.#.#",
    "#.#.#.....#.#.#",
    "#.#.#######.#.#",
    "#.#.........#.#",
    "#.###########.#",
    "#.............#",
    "###############",
};
/// Paper all round the spiral, in pixels.
constexpr int MARGIN_PX = 1;
/// Each pixel is drawn this many times over, so the window manager need not blur it up.
constexpr int ICON_SCALE = 4;

} // namespace

bool iconWall(int x, int y) { return SPIRAL_GLYPH[y][x] == '#'; }

Image makeIcon() {
    int side = ICON_PX + 2 * MARGIN_PX;
    Image icon = GenImageColor(side * ICON_SCALE, side * ICON_SCALE, ui::PAPER);
    for (int y = 0; y < ICON_PX; ++y)
        for (int x = 0; x < ICON_PX; ++x)
            if (iconWall(x, y))
                ImageDrawRectangle(&icon, (x + MARGIN_PX) * ICON_SCALE, (y + MARGIN_PX) * ICON_SCALE, ICON_SCALE,
                                   ICON_SCALE, ui::BRIGHT);
    return icon;
}
