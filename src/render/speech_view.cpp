#include "render/speech_view.h"
#include "human/human.h"
#include "render/world_view.h"
#include "ui/pixel_ui.h"
#include "world/world.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {

/// Text wraps at this width; the bubble pads it this much on each side,
/// floats this far over the head, and keeps this far in from the edges.
constexpr int TEXT_W = 96;
constexpr int PAD_X = 3, PAD_Y = 2;
constexpr int LIFT = 3;
constexpr int EDGE = 2;
constexpr Color FILL = {14, 14, 14, 225};
/// Edges by tone: plain, warm, rude, plead, fear, threat.
constexpr Color EDGES[] = {
    {150, 150, 150, 255}, {120, 200, 120, 255}, {215, 120, 80, 255},
    {130, 165, 220, 255}, {175, 140, 215, 255}, {225, 70, 60, 255},
};

struct Bubble {
    std::vector<std::string> rows;
    Rectangle box;
    /// The top of the speaker's head, where the tail ends.
    Vector2 head;
    Color edge;
};

bool overlap(const Rectangle& a, const Rectangle& b) {
    return a.x < b.x + b.width + 1 && b.x < a.x + a.width + 1 && a.y < b.y + b.height + 1 && b.y < a.y + a.height + 1;
}

} // namespace

void drawSpeech(const World& world, const Human& viewer, bool reveal, float zoom, Rectangle view, int top,
                int bottom, Vector2 focus) {
    // The head is taken from the same canvas pixel its sprite is drawn on;
    // else the bubble and the body move by different steps, and shake.
    Vector2 cam = cameraFor(focus, zoom, {view.width, view.height});
    float viewRight = view.x + view.width;
    std::vector<Bubble> bubbles;
    for (const auto& h : world.humans()) {
        const Human::Speech* s = h->speaking();
        if (!s || h->dead() || !shown(viewer, *h, reveal)) continue;
        Vector2 feet = drawnFeet(*h, zoom);
        Vector2 head{view.x + std::round((feet.x - cam.x) * zoom),
                     view.y + std::round((feet.y - SPRITE_H - cam.y) * zoom)};
        if (head.x < view.x || head.x >= viewRight || head.y < top || head.y >= bottom) continue;
        Bubble b;
        b.rows = ui::wrap(s->text, TEXT_W);
        int w = 0;
        for (const std::string& r : b.rows) w = std::max(w, ui::textWidth(r.c_str()));
        float bw = float(w + 2 * PAD_X), bh = float(int(b.rows.size()) * ui::ROW + 2 * PAD_Y - 1);
        float x = std::clamp(std::round(head.x - bw / 2), view.x + EDGE, viewRight - EDGE - bw);
        b.box = {x, std::round(head.y) - LIFT - bh, bw, bh};
        b.head = head;
        b.edge = EDGES[size_t(s->tone)];
        bubbles.push_back(std::move(b));
    }
    // The lowest speakers keep their place; anyone above whose bubble would
    // cover one already placed is pushed up over it.
    std::sort(bubbles.begin(), bubbles.end(), [](const Bubble& a, const Bubble& b) { return a.head.y > b.head.y; });
    for (size_t i = 0; i < bubbles.size(); i++) {
        Rectangle& r = bubbles[i].box;
        for (bool moved = true; moved;) {
            moved = false;
            for (size_t j = 0; j < i; j++)
                if (overlap(r, bubbles[j].box)) {
                    r.y = bubbles[j].box.y - r.height - 2;
                    moved = true;
                }
        }
        r.y = std::max(r.y, float(top + EDGE));
    }
    // Drawn top down, so lower bubbles, nearer their speakers, lie on top.
    for (auto it = bubbles.rbegin(); it != bubbles.rend(); ++it) {
        const Bubble& b = *it;
        int tx = int(std::clamp(b.head.x, b.box.x + 1, b.box.x + b.box.width - 2));
        int tailTop = int(b.box.y + b.box.height), tailEnd = int(b.head.y) - 1;
        if (tailEnd > tailTop) DrawLine(tx, tailTop, tx, tailEnd, b.edge);
        DrawRectangleRec(b.box, FILL);
        DrawRectangleLinesEx(b.box, 1.0f, b.edge);
        int y = int(b.box.y) + PAD_Y;
        for (const std::string& row : b.rows) {
            ui::textCentred(row.c_str(), int(b.box.x + b.box.width / 2), y, ui::BRIGHT);
            y += ui::ROW;
        }
    }
}
