#include "render/canvas.h"
#include <algorithm>
#include <cmath>

Canvas::Canvas() : target_(LoadRenderTexture(W, H)) {
    SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
}

Canvas::~Canvas() { UnloadRenderTexture(target_); }

void Canvas::begin() const { BeginTextureMode(target_); }

void Canvas::end() const { EndTextureMode(); }

float Canvas::scale() const {
    float s = std::min(GetScreenWidth() / float(W), GetScreenHeight() / float(H));
    return s >= 1.0f ? std::floor(s) : s;
}

Vector2 Canvas::offset() const {
    float s = scale();
    return {std::floor((GetScreenWidth() - W * s) / 2), std::floor((GetScreenHeight() - H * s) / 2)};
}

void Canvas::present() const {
    float s = scale();
    Vector2 o = offset();
    // Render textures come out upside down.
    DrawTexturePro(target_.texture, {0, 0, float(W), -float(H)}, {o.x, o.y, W * s, H * s},
                   {0, 0}, 0.0f, WHITE);
}

Vector2 Canvas::mouse() const {
    Vector2 m = GetMousePosition(), o = offset();
    float s = scale();
    return {(m.x - o.x) / s, (m.y - o.y) / s};
}
