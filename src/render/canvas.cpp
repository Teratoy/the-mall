#include "render/canvas.h"
#include <algorithm>
#include <cmath>

namespace {
/// From this many times up the canvas scales in whole steps, keeping every
/// pixel the same size; below it, smoothly, so a wide canvas still fills a
/// screen it can't fit twice.
constexpr float WHOLE_FROM = 2.0f;
} // namespace

Canvas::Canvas() : tall_(LoadRenderTexture(W, H)), wideTarget_(LoadRenderTexture(WIDE_W, H)) {
    SetTextureFilter(tall_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(wideTarget_.texture, TEXTURE_FILTER_POINT);
}

Canvas::~Canvas() {
    UnloadRenderTexture(tall_);
    UnloadRenderTexture(wideTarget_);
}

void Canvas::fit(bool sceneCanBeWide) { wide_ = sceneCanBeWide && GetScreenWidth() > GetScreenHeight(); }

void Canvas::begin() const { BeginTextureMode(target()); }

void Canvas::end() const { EndTextureMode(); }

float Canvas::scale() const {
    float s = std::min(GetScreenWidth() / float(width()), GetScreenHeight() / float(height()));
    return s >= WHOLE_FROM ? std::floor(s) : s;
}

Vector2 Canvas::offset() const {
    float s = scale();
    return {std::floor((GetScreenWidth() - width() * s) / 2), std::floor((GetScreenHeight() - height() * s) / 2)};
}

void Canvas::present() const {
    float s = scale();
    Vector2 o = offset();
    float w = float(width()), h = float(height());
    // Render textures come out upside down.
    DrawTexturePro(target().texture, {0, 0, w, -h}, {o.x, o.y, w * s, h * s}, {0, 0}, 0.0f, WHITE);
}

Vector2 Canvas::mouse() const {
    Vector2 m = GetMousePosition(), o = offset();
    float s = scale();
    return {(m.x - o.x) / s, (m.y - o.y) / s};
}
