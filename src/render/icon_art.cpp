#include "render/icon_art.h"
#include "render/sprites.h"
#include "ui/pixel_ui.h"

namespace {

/// Paper all round the spiral, in pixels.
constexpr int MARGIN_PX = 1;
/// Each pixel is drawn this many times over, so the window manager need not blur it up.
constexpr int ICON_SCALE = 4;

const Sprite& spiral() { return sprites::get("icon", ICON_PX, ICON_PX); }

} // namespace

bool iconWall(int x, int y) { return spiral().pixel(x, y).solid(); }

Image makeIcon() {
    int side = ICON_PX + 2 * MARGIN_PX;
    Image icon = GenImageColor(side * ICON_SCALE, side * ICON_SCALE, ui::PAPER);
    for (int y = 0; y < ICON_PX; ++y)
        for (int x = 0; x < ICON_PX; ++x)
            if (Rgba c = spiral().pixel(x, y); c.solid())
                ImageDrawRectangle(&icon, (x + MARGIN_PX) * ICON_SCALE, (y + MARGIN_PX) * ICON_SCALE, ICON_SCALE,
                                   ICON_SCALE, {c.r, c.g, c.b, c.a});
    return icon;
}
