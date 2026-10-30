#include "world/mess.h"
#include <algorithm>
#include <cmath>

namespace {

/// Seconds to dry right through: feces slowly, piss a little faster.
constexpr float FECES_DRY_TIME = 7200.0f;
constexpr float PISS_DRY_TIME = 1800.0f;
/// How disgusting each is fresh, and the share of that left once dry.
constexpr float FECES_DISGUST = 1.0f, FECES_DRY_DISGUST = 0.3f;
constexpr float PISS_DISGUST = 0.3f, PISS_DRY_DISGUST = 0.1f;
/// Foulness fades by e every FOUL_LIFE seconds (half gone in about two
/// hours), however far it has spread; below FOUL_GONE it's gone (low
/// enough that the thin edge of a spread loses next to nothing).
constexpr float FOUL_LIFE = 10800.0f;
constexpr float FOUL_GONE = 0.0001f;
/// Each second, this share of the difference between two water cells side
/// by side seeps from the fouler to the cleaner: about a quarter of an hour
/// to even out through a basin, longer through a pool. Worked out every
/// FOUL_SEEP_STEP seconds.
constexpr float FOUL_SEEP = 0.003f;
constexpr float FOUL_SEEP_STEP = 1.0f;
/// A footprint, as feces go, a little of it.
constexpr float PRINT_DISGUST = 0.15f;

} // namespace

void Mess::drop(MessKind kind, Vec2i cell, float x, float y, float size, float born, int who, uint32_t seed) {
    cells_[cell].push_back(int(all_.size()));
    all_.push_back({kind, cell, x, y, size, born, who, seed});
}

float Mess::freshness(const Dropping& d, float now) {
    float dry = d.kind == MessKind::Piss ? PISS_DRY_TIME : FECES_DRY_TIME;
    return std::clamp(1.0f - (now - d.born) / dry, 0.0f, 1.0f);
}

float Mess::disgust(const Dropping& d, float now) {
    float f = freshness(d, now);
    bool piss = d.kind == MessKind::Piss;
    float dry = piss ? PISS_DRY_DISGUST : FECES_DRY_DISGUST;
    float most = piss ? PISS_DISGUST : d.kind == MessKind::Feces ? FECES_DISGUST : PRINT_DISGUST;
    return d.size * most * (dry + (1.0f - dry) * f);
}

float Mess::disgust(Vec2i cell, float now, int but) const {
    const std::vector<int>* here = at(cell);
    if (!here) return 0.0f;
    float sum = 0.0f;
    for (int i : *here)
        if (all_[size_t(i)].who != but || but < 0) sum += disgust(all_[size_t(i)], now);
    return sum;
}

float Mess::freshFeces(Vec2i cell, float now) const {
    const std::vector<int>* here = at(cell);
    if (!here) return 0.0f;
    float most = 0.0f;
    for (int i : *here)
        if (const Dropping& d = all_[size_t(i)]; d.kind == MessKind::Feces)
            most = std::max(most, freshness(d, now) * std::min(1.0f, d.size));
    return most;
}

// What leaves a cell goes into the one beside it, so spreading never makes
// foulness fade faster.
void Mess::update(float dt, const std::function<bool(Vec2i)>& water) {
    if ((seep_ += dt) < FOUL_SEEP_STEP) return;
    float step = seep_;
    seep_ = 0.0f;
    constexpr Vec2i SIDES[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    std::unordered_map<Vec2i, float, Vec2iHash> flow;
    for (const auto& [c, v] : foul_)
        for (Vec2i d : SIDES) {
            Vec2i n = c + d;
            if (!water(n)) continue;
            auto it = foul_.find(n);
            float there = it == foul_.end() ? 0.0f : it->second;
            if (there >= v) continue;
            float moved = (v - there) * std::min(0.25f, FOUL_SEEP * step);
            flow[c] -= moved;
            flow[n] += moved;
        }
    for (const auto& [c, f] : flow) foul_[c] += f;
    // A cell still filling from the next is never gone yet.
    float keep = std::exp(-step / FOUL_LIFE);
    std::erase_if(foul_, [&](auto& kv) {
        auto in = flow.find(kv.first);
        return (kv.second *= keep) < FOUL_GONE && (in == flow.end() || in->second <= 0.0f);
    });
}
