#include "human/human.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Episodes: the notable things a human went through or saw, kept for it to
// talk about (AI/chitchat_plan.md). Each fades with time, the strong ones
// slower; only the most striking few are kept. The same thing again soon
// after (a punch in the same fight) adds to the one remembered.

namespace {

/// Episodes kept; past this the one that matters least is forgotten.
constexpr size_t EPISODES_MAX = 32;
/// An episode matters half as much after this long, times 0.5 plus its
/// strength: the strongest last three times as long as the faintest.
constexpr float EPISODE_HALF_LIFE = 900.0f;
/// The same thing, with the same people, this soon after the last time is
/// the same episode again: it strikes this much harder each time.
constexpr float EPISODE_MERGE_TIME = 30.0f;
constexpr float EPISODE_AGAIN = 0.15f;
/// Two humans remember the same thing when they remember it this close in
/// time and place.
constexpr float SAME_TIME = 5.0f;
constexpr int SAME_DIST = 3;
/// Most others it notes as having been there.
constexpr size_t WITH_MAX = 8;

// Things that stay where they are, as against things that happen: two who
// saw the same body or pool remember the same thing whenever they saw it.
bool lasting(Episode::What w) {
    using W = Episode::What;
    return w == W::Body || w == W::Gnawed || w == W::Pool || w == W::FoulWater || w == W::Spotted ||
           w == W::NewPlace;
}

// Who did what to whom matches either way round for a fight.
bool samePeople(const Episode& a, int who, int whom) {
    if (a.who == who && a.whom == whom) return true;
    return a.what == Episode::What::Fight && a.who == whom && a.whom == who;
}

} // namespace

void Human::remember(Episode::What what, float strength, int who, int whom, int detail) {
    rememberAt(cell, what, strength, who, whom, detail);
}

void Human::rememberAt(Vec2i at, Episode::What what, float strength, int who, int whom, int detail) {
    float now = born + clock_;
    for (Episode& e : episodes_)
        if (e.what == what && e.detail == detail && samePeople(e, who, whom) && now - e.when < EPISODE_MERGE_TIME &&
            cheb(e.cell, at) <= SAME_DIST * 2) {
            e.strength = std::min(1.0f, std::max(e.strength, strength) + EPISODE_AGAIN * strength);
            e.when = now;
            for (int o : inSight_)
                if (e.with.size() < WITH_MAX && o != who && o != whom &&
                    std::find(e.with.begin(), e.with.end(), o) == e.with.end())
                    e.with.push_back(o);
            return;
        }
    Episode e{what, now, now, at, who, whom, detail, std::clamp(strength, 0.0f, 1.0f), {}, {}};
    for (int o : inSight_)
        if (e.with.size() < WITH_MAX && o != who && o != whom) e.with.push_back(o);
    episodes_.push_back(std::move(e));
    if (episodes_.size() > EPISODES_MAX) {
        auto least = std::min_element(episodes_.begin(), episodes_.end(), [&](const Episode& a, const Episode& b) {
            return salience(a) < salience(b);
        });
        episodes_.erase(least);
    }
}

float Human::salience(const Episode& e) const {
    float age = born + clock_ - e.when;
    return e.strength * std::exp2(-age / (EPISODE_HALF_LIFE * (0.5f + e.strength)));
}

const Episode* Human::shares(const Episode& e) const {
    for (const Episode& m : episodes_)
        if (m.what == e.what && m.detail == e.detail && samePeople(m, e.who, e.whom) &&
            (lasting(e.what) ? e.what == Episode::What::NewPlace || cheb(m.cell, e.cell) <= SAME_DIST
                             : m.since <= e.when + SAME_TIME && e.since <= m.when + SAME_TIME &&
                                   cheb(m.cell, e.cell) <= SAME_DIST))
            return &m;
    return nullptr;
}
