#include "ui/pixel_ui.h"
#include <cstdio>

namespace ui {

namespace {
constexpr float FONT_SIZE = 10.0f;
constexpr float SPACING = 1.0f;
} // namespace

void text(const char* s, int x, int y, Color c) {
    DrawTextEx(GetFontDefault(), s, {float(x), float(y)}, FONT_SIZE, SPACING, c);
}

void textCentred(const char* s, int cx, int y, Color c) { text(s, cx - textWidth(s) / 2, y, c); }

void textRight(const char* s, int right, int y, Color c) { text(s, right - textWidth(s), y, c); }

int textWidth(const char* s) { return int(MeasureTextEx(GetFontDefault(), s, FONT_SIZE, SPACING).x); }

std::vector<std::string> wrap(const std::string& s, int width) {
    std::vector<std::string> lines;
    std::string line;
    size_t at = 0;
    while (at < s.size()) {
        size_t end = s.find(' ', at);
        if (end == std::string::npos) end = s.size();
        std::string word = s.substr(at, end - at);
        std::string tried = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(tried.c_str()) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = tried;
        }
        at = end + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void panel(Rectangle r) { DrawRectangleRec(r, SHADE); }

std::string clockText(float seconds) {
    int t = int(seconds), h = t / 3600, m = t / 60 % 60, sec = t % 60;
    char s[16];
    if (h > 0)
        std::snprintf(s, sizeof s, "%d:%02d:%02d", h, m, sec);
    else
        std::snprintf(s, sizeof s, "%d:%02d", m, sec);
    return s;
}

namespace {
/// A button's frame. Gives the colour to draw its face in and whether the
/// press sinks it a pixel.
Color frame(Rectangle r, Vector2 mouse, bool& held) {
    bool over = CheckCollisionPointRec(mouse, r);
    held = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color edge = over ? WHITE : INK;
    DrawRectangleRec(r, edge);
    DrawRectangle(int(r.x) + 1, int(r.y) + 1, int(r.width) - 2, int(r.height) - 2, held ? edge : PAPER);
    return held ? PAPER : edge;
}

bool released(Rectangle r, Vector2 mouse) {
    return CheckCollisionPointRec(mouse, r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}
} // namespace

bool button(Rectangle r, const char* label, Vector2 mouse) {
    bool held;
    Color face = frame(r, mouse, held);
    int ty = int(r.y + (r.height - FONT_SIZE) / 2) + (held ? 1 : 0);
    textCentred(label, int(r.x + r.width / 2), ty, face);
    return released(r, mouse);
}

bool arrowButton(Rectangle r, bool up, Vector2 mouse) {
    constexpr int ARROW_H = 4;
    bool held;
    Color face = frame(r, mouse, held);
    int cx = int(r.x + r.width / 2), top = int(r.y + (r.height - ARROW_H) / 2) + (held ? 1 : 0);
    // Rows 1, 3, 5 and 7 pixels wide, tip first for up.
    for (int i = 0; i < ARROW_H; i++) {
        int half = up ? i : ARROW_H - 1 - i;
        DrawRectangle(cx - half, top + i, 2 * half + 1, 1, face);
    }
    return released(r, mouse);
}

bool backPressed() { return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK); }

} // namespace ui
