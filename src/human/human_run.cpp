// Running: in a hurry, a human runs while it has breath. Out of breath, it
// stands bent over, hands on its knees, panting, then walks on until it has
// enough back to run again.
#include "human/human.h"
#include "human/human_tuning.h"

namespace {

/// Breath back per second walking, and standing or sitting still (or winded).
constexpr float BREATH_WALK = 1.0f / 20.0f;
constexpr float BREATH_REST = 1.0f / 6.0f;
/// Out of breath, it can't run again until it has this much back.
constexpr float BREATH_AGAIN = 0.4f;
/// Seconds it stands winded once out of breath.
constexpr float WINDED_MIN = 2.0f, WINDED_MAX = 3.0f;

} // namespace

bool Human::running() const { return run_.on && !run_.spent && !winded() && state == State::Walking; }

void Human::startRunning(Goal g, std::vector<Vec2i> path, RunWhy why) {
    startWalking(g, std::move(path));
    run_.on = true;
    run_.why = why;
}

void Human::breathe(float dt) {
    if (action() == Action::Run) {
        run_.breath -= dt / run_.lungs;
        if (run_.breath > 0.0f) return;
        run_.breath = 0.0f;
        run_.spent = true;
        run_.winded = rng_.range(WINDED_MIN, WINDED_MAX);
        note("OUT OF BREATH");
        sayLater(talk::Moment::Winded);
        return;
    }
    run_.winded = std::max(0.0f, run_.winded - dt);
    bool still = !moving || state != State::Walking;
    run_.breath = std::min(1.0f, run_.breath + (still ? BREATH_REST : BREATH_WALK) * dt);
    if (run_.breath >= BREATH_AGAIN) run_.spent = false;
}
