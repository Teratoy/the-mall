#include "scenes/config_panel.h"
#include "audio/audio.h"
#include "audio/voice.h"
#include "render/canvas.h"
#include "ui/pixel_ui.h"
#include <algorithm>

namespace {

constexpr const char* TITLE = "CONFIGURATION";
constexpr int TITLE_Y = Canvas::H / 2 - 70;
/// The first row, and how far apart rows are.
constexpr int ROWS_Y = TITLE_Y + 24, ROW_STEP = 22;
/// The - and + buttons, the pips between them, and the gaps in a row.
constexpr int STEP_W = 16, STEP_H = 16, PIP_W = 5, PIP_H = 8, PIP_GAP = 2, GAP = 6;
constexpr int BACK_W = 100, BACK_H = 24, BACK_GAP = 14;
/// Said when VOICES changes, to hear how loud.
constexpr const char* SAMPLE_LINE = "HELLO?";
constexpr Color PIP_ON = ui::BRIGHT, PIP_OFF = ui::GROUND;

struct Row {
    const char* label;
    int audio::Settings::*value;
};
constexpr Row ROWS[] = {{"MASTER", &audio::Settings::master}, {"SOUNDS", &audio::Settings::sounds},
                        {"VOICES", &audio::Settings::voices}};

} // namespace

// The rows are laid out from the widest label, the whole block centred.
bool ConfigPanel::draw(Vector2 mouse, int width) {
    ui::textCentred(TITLE, width / 2, TITLE_Y, ui::BRIGHT);
    int labelW = 0;
    for (const Row& r : ROWS) labelW = std::max(labelW, ui::textWidth(r.label));
    int pipsW = audio::VOLUME_STEPS * (PIP_W + PIP_GAP) - PIP_GAP;
    int rowW = labelW + GAP + STEP_W + GAP + pipsW + GAP + STEP_W;
    int x = (width - rowW) / 2;
    audio::Settings& s = audio::settings();
    int y = ROWS_Y;
    for (const Row& r : ROWS) {
        int& v = s.*r.value;
        ui::text(r.label, x, y + (STEP_H - ui::ROW) / 2 + 1);
        int bx = x + labelW + GAP;
        int step = 0;
        if (ui::button({float(bx), float(y), STEP_W, STEP_H}, "-", mouse)) step = -1;
        int px = bx + STEP_W + GAP;
        for (int i = 0; i < audio::VOLUME_STEPS; i++)
            DrawRectangle(px + i * (PIP_W + PIP_GAP), y + (STEP_H - PIP_H) / 2, PIP_W, PIP_H, i < v ? PIP_ON : PIP_OFF);
        if (ui::button({float(px + pipsW + GAP), float(y), STEP_W, STEP_H}, "+", mouse)) step = 1;
        if (step && std::clamp(v + step, 0, audio::VOLUME_STEPS) != v) {
            v += step;
            audio::save();
            // A voice is set by hearing one.
            if (r.value == &audio::Settings::voices) audio::speak(voice::speak(voice::Voice{}, SAMPLE_LINE));
            else audio::play("pop");
        }
        y += ROW_STEP;
    }
    Rectangle back{(width - BACK_W) / 2.0f, float(y + BACK_GAP), BACK_W, BACK_H};
    return ui::button(back, "BACK", mouse);
}
