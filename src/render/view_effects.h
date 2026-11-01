#pragma once

/// The view's effects the player can turn off in CONFIGURATION, all on at
/// first: snow where the viewer has never been, light stepping from cell to
/// cell through a dither, and the edge of sight fading into memory. Kept
/// between runs in the settings file with the volumes (audio::save).
struct ViewEffects {
    /// Cells never seen are snow, like a dead screen; off, plain black.
    bool snow = true;
    /// Light falls off in dithered pixel steps; off, in a smooth blend.
    bool lightDither = true;
    /// The last rings of sight fade into how memory looks, and pixels blend
    /// across the edge; off, sight ends at the edge of a cell.
    bool sightDither = true;
};

inline ViewEffects& viewEffects() {
    static ViewEffects effects;
    return effects;
}
