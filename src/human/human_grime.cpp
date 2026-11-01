#include "human/human.h"
#include "human/human_tuning.h"
#include <algorithm>

namespace {

/// Seconds unwashed before a human turns dirty.
constexpr float GRIME_TIME = 600.0f;
/// Grime goes this many times as fast while there is blood, feces or piss on it.
constexpr float GRIME_SOILED = 2.0f;
/// Grime stops building here: the grimiest it gets.
constexpr float GRIME_MAX = 3.0f;
/// Below this modesty a human never washes for grime or dried blood; from
/// MODEST_FULL up it washes as soon as it turns dirty.
constexpr float MODEST_WASH = 0.3f, MODEST_FULL = 0.7f;
/// The least modest who still wash for grime put up with this much more
/// grime past turning dirty first.
constexpr float GRIME_PUT_UP = 1.5f;
/// Each wash step takes this much grime off. This little, which builds
/// again while it washes, is clean.
constexpr float GRIME_SCRUB = 0.5f, GRIME_CLEAN = 0.05f;
/// Told it smells or shunned for it, the shame is gone in this many
/// seconds: a few minutes (the user).
constexpr float SHAME_FADE = 180.0f;

} // namespace

void Human::soil(float dt) {
    bool soiled = stains.any() || dirt.any() || soaked.any();
    grime_.level = std::min(GRIME_MAX, grime_.level + dt / GRIME_TIME * (soiled ? GRIME_SOILED : 1.0f));
    grime_.shame = std::max(0.0f, grime_.shame - dt / SHAME_FADE);
    if (grime_.dirty || grime_.level < 1.0f) return;
    grime_.dirty = true;
    note("TURNED DIRTY");
    sayLater(talk::Moment::GotDirty);
    if (wantsScrub()) rethink_ = true;
}

bool Human::washesForLooks() const { return !isErratic() && modesty() >= MODEST_WASH; }

bool Human::wantsScrub() const {
    if (!grime_.dirty || isErratic()) return false;
    // Smelling, the modest put up with it no longer; the shameless only
    // while ashamed of it.
    if (grime_.smelly && (washesForLooks() || grime_.shame > 0.0f)) return true;
    if (!washesForLooks()) return false;
    float shame = std::min(1.0f, (modesty() - MODEST_WASH) / (MODEST_FULL - MODEST_WASH));
    return grime_.level >= 1.0f + GRIME_PUT_UP * (1.0f - shame);
}

float Human::smellWash() const {
    if (!grime_.smelly || isErratic()) return 0.0f;
    float push = washesForLooks() ? 0.5f + 0.5f * grime_.shame : grime_.shame;
    return GRIME_WASH * smellStrength() * push;
}

bool Human::scrub() {
    if (grime_.level > GRIME_CLEAN) {
        grime_.level = std::max(0.0f, grime_.level - GRIME_SCRUB);
        return true;
    }
    grime_.level = 0.0f;
    if (!grime_.dirty) return false;
    grime_.dirty = false;
    grime_.shame = 0.0f;
    note("WASHED CLEAN");
    sayLater(talk::Moment::WashedClean);
    return false;
}
