// The dark (AI/light_plan.md step 5): standing where no light falls, a
// human grows uneasy, the timid most. The first time it finds itself in the
// dark it marks it; going back in after a while in light, and coming out of
// a long spell in it, it may say how that feels. Exploring, the timid, the
// parched and the hopeless would sooner make for light they have seen than
// feel their way on into the dark.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"

#include <algorithm>

namespace {

/// Dread per second in the dark, before bravery sways it; the dark alone
/// doesn't take it past DARK_DREAD_MOST.
constexpr float DARK_DREAD = 0.012f;
constexpr float DARK_DREAD_MOST = 0.45f;
/// Seconds in light before going back into the dark is worth a word, and
/// in the dark before coming out of it is.
constexpr float DARK_AGAIN_AFTER = 60.0f;
constexpr float DARK_LONG = 20.0f;
/// What the dark puts the most fearful off a frontier it remembers dark,
/// in tenths of a cell.
constexpr int DARK_FRONTIER_COST = 200;
/// The parched fear the dark this much more, and the hopeless up to this much.
constexpr float DARK_FEAR_PARCHED = 0.4f, DARK_FEAR_HOPELESS = 0.3f;
/// Afraid of the dark this much at least, it says it makes for the light.
constexpr float TOWARD_LIGHT_FEAR = 0.5f;

} // namespace

// Its own cell it sees, lit or not. Asleep or still waking it minds nothing.
void Human::darkness(const World& world, float dt) {
    if (state == State::Sleeping || state == State::Waking) return;
    if (world.light(cell) > 0) {
        if (dark_.in >= DARK_LONG) {
            note("CAME OUT OF THE DARK");
            sayLater(talk::Moment::OutOfDark);
        }
        dark_.in = 0.0f;
        dark_.lit += dt;
        dark_.toward = false;
        return;
    }
    if (dark_.in == 0.0f) {
        if (!dark_.met) {
            dark_.met = true;
            note("STOOD IN THE DARK");
            sayLater(talk::Moment::FirstDark);
        } else if (dark_.lit >= DARK_AGAIN_AFTER) {
            note("INTO THE DARK AGAIN");
            sayLater(talk::Moment::IntoDark);
        }
    }
    dark_.in += dt;
    dark_.lit = 0.0f;
    if (dread_ < DARK_DREAD_MOST) {
        scare(DARK_DREAD * dt);
        dread_ = std::min(dread_, DARK_DREAD_MOST);
    }
}

float Human::darkFear() const {
    float fear = 1.0f - traits.brave;
    if (parched()) fear += DARK_FEAR_PARCHED;
    fear += DARK_FEAR_HOPELESS * gloom();
    return std::min(1.0f, fear);
}

// Only from the dark: in light, the way on is the way on, lit or not.
int Human::darkCost(Vec2i c) const {
    if (dark_.in <= 0.0f) return 0;
    auto seen = memory.lightSeen(c);
    if (!seen || seen->level > 0) return 0;
    return int(float(DARK_FRONTIER_COST) * darkFear());
}

void Human::makeForLight(const World& world, const std::vector<Vec2i>& path) {
    if (dark_.toward || path.empty() || world.light(cell) > 0 || darkFear() < TOWARD_LIGHT_FEAR) return;
    auto seen = memory.lightSeen(path.back());
    if (!seen || seen->level == 0) return;
    dark_.toward = true;
    note("MADE FOR THE LIGHT");
    sayLater(talk::Moment::TowardLight);
}
