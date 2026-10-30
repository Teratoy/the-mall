#include "render/icon_art.h"
#include "ui/pixel_ui.h"

namespace {

/// The spiral, # its wall, entered at the top left.
constexpr int SPIRAL_PX = 15;
constexpr const char* SPIRAL_GLYPH[SPIRAL_PX] = {
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

Image makeIcon() {
    int side = SPIRAL_PX + 2 * MARGIN_PX;
    Image icon = GenImageColor(side * ICON_SCALE, side * ICON_SCALE, ui::PAPER);
    for (int y = 0; y < SPIRAL_PX; ++y)
        for (int x = 0; x < SPIRAL_PX; ++x)
            if (SPIRAL_GLYPH[y][x] == '#')
                ImageDrawRectangle(&icon, (x + MARGIN_PX) * ICON_SCALE, (y + MARGIN_PX) * ICON_SCALE, ICON_SCALE,
                                   ICON_SCALE, ui::BRIGHT);
    return icon;
}
