#pragma once
#include <raylib.h>

/// The low-res screen everything draws on, scaled up to the window in
/// whole steps where it fits. Pocket Rave's 72x156 at three times the pixels.
class Canvas {
public:
    static constexpr int W = 216;
    static constexpr int H = 468;

    Canvas();
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    void begin() const;
    void end() const;
    /// Draws the canvas into the window, centred.
    void present() const;
    /// The mouse or touch point in canvas pixels.
    Vector2 mouse() const;

private:
    RenderTexture2D target_;

    float scale() const;
    Vector2 offset() const;
};
