#pragma once
#include <raylib.h>

/// The low-res screen everything draws on, scaled up to the window in
/// whole steps where it fits. Pocket Rave's 72x156 at three times the pixels.
/// It comes in two shapes: the phone's, tall, and the PC's, wide, which a
/// scene that can draw wide gets while the window is wider than tall.
class Canvas {
public:
    static constexpr int W = 216;
    static constexpr int H = 468;
    /// The PC canvas: a square of world with columns either side, as tall
    /// as the phone one.
    static constexpr int WIDE_W = 1014;

    Canvas();
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    /// Picks the shape for this frame: wide if the scene can draw wide and
    /// the window is wider than tall.
    void fit(bool sceneCanBeWide);
    bool wide() const { return wide_; }
    int width() const { return wide_ ? WIDE_W : W; }
    int height() const { return H; }

    void begin() const;
    void end() const;
    /// Draws the canvas into the window, centred.
    void present() const;
    /// The mouse or touch point in canvas pixels.
    Vector2 mouse() const;

private:
    RenderTexture2D tall_, wideTarget_;
    bool wide_ = false;

    const RenderTexture2D& target() const { return wide_ ? wideTarget_ : tall_; }
    float scale() const;
    Vector2 offset() const;
};
