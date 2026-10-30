#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// What a human brings up in a chat: every topic it has something real for
// is scored by how much it matters to it right now, and the highest wins.
// Nothing is picked by chance. What it raised with whom is kept, so it
// doesn't bring the same thing up with them again until something changes.

namespace {

/// How much each kind of topic weighs, times how strongly it is felt.
/// Something both remember weighs more than a story of its own.
constexpr float SHARED_WEIGHT = 1.3f;
constexpr float STORY_WEIGHT = 1.0f;
/// Episodes that matter less than this are not worth bringing up.
constexpr float EPISODE_WORTH = 0.1f;
/// Things in sight this close are worth a remark: a body (a friend's
/// more), blood, a statue, water, mess. The curious remark on things more,
/// up to HERE_CURIOUS either way.
constexpr int HERE_RANGE = 10;
constexpr float HERE_BODY = 0.7f, HERE_FRIEND_BODY = 0.9f, HERE_BLOOD = 0.5f, HERE_STATUE = 0.45f;
constexpr float HERE_WATER = 0.3f, HERE_FOUL = 0.5f, HERE_MESS = 0.35f;
constexpr float HERE_CURIOUS = 0.25f;
/// Mess this disgusting is worth a remark; water this foul counts fouled.
constexpr float HERE_MESS_DISGUST = 0.3f;
constexpr float HERE_WATER_FOUL = 0.3f;
constexpr int HERE_FOUL_NEAR = 3;
/// A third human it thinks this firmly of, either way, is worth talking
/// about, weighing SOMEONE_WEIGHT times how firmly; a dead friend
/// SOMEONE_DEAD. Not anyone within earshot.
constexpr float SOMEONE_FIRM = 0.15f;
constexpr float SOMEONE_WEIGHT = 0.8f;
constexpr float SOMEONE_DEAD = 0.8f;
constexpr int SOMEONE_EARSHOT = 6;
/// Raised again only once its opinion moved this far.
constexpr float SOMEONE_MOVED = 0.2f;
/// Feelings: getting hungry or thirsty from PECKISH (HUNGRY and THIRSTY in
/// human.cpp are 0.5, where it goes looking), footsore after FOOTSORE_TIME
/// without sitting, uneasy from UNEASY dread, hopeful from HOPEFUL hope;
/// each weighs its own amount at its fullest.
constexpr float PECKISH = 0.35f, NEEDS_SOON = 0.5f;
constexpr float FOOTSORE_TIME = 600.0f, FOOTSORE = 0.35f;
constexpr float UNEASY = 0.25f, UNEASY_WEIGHT = 0.7f;
constexpr float GLOOMY_WEIGHT = 0.6f;
constexpr float HOPEFUL = 0.85f, HOPEFUL_WEIGHT = 0.3f;
/// The way out, while it still wants out this much and is this sure of
/// its hunch.
constexpr float WAY_WANT = 0.3f, WAY_SURE_ENOUGH = 0.15f, WAY_WEIGHT = 0.4f;
/// Its group: after going into GROUP_PLACES places with the other.
constexpr int GROUP_PLACES = 2;
constexpr float GROUP_WEIGHT = 0.3f, GROUP_PER_PLACE = 0.05f;
/// Before: much on its mind while newly woken, less after BEFORE_FRESH s.
constexpr float BEFORE_FRESH = 600.0f, BEFORE_WEIGHT = 0.45f, BEFORE_LATER = 0.05f;
/// A dream of before, lately, puts it back on its mind this much.
constexpr float BEFORE_DREAMT = 0.3f;
/// The same thing in sight, feeling, way or group is raised with the same
/// human again only after this long.
constexpr float HERE_AGAIN = 600.0f, FEEL_AGAIN = 180.0f, WAY_AGAIN = 300.0f, GROUP_AGAIN = 300.0f;

uint64_t keyOf(Topic::Kind k, int sub, int a, int b = 0) {
    return uint64_t(k) << 56 | uint64_t(uint8_t(sub)) << 48 | uint64_t(uint32_t(a) & 0xffffff) << 24 |
           uint64_t(uint32_t(b) & 0xffffff);
}

bool sameEpisode(const Episode& a, const Episode& b) {
    return a.what == b.what && a.since == b.since && a.who == b.who && a.whom == b.whom && a.cell == b.cell;
}

} // namespace

const char* topicName(Topic::Kind k) {
    static const char* NAMES[] = {"here", "shared", "story", "someone", "feeling", "way", "group", "before"};
    static_assert(std::size(NAMES) == size_t(Topic::Kind::Count));
    return NAMES[size_t(k)];
}

// Owning up to killing, eating or robbing the dead is only for the erratic.
bool Human::owns(const Episode& e) const {
    using W = Episode::What;
    bool damning = e.who == id && (e.what == W::Kill || e.what == W::Flesh || e.what == W::Loot);
    return !damning || isErratic();
}

std::optional<Topic> Human::pickTopic(const World& world, const Human& to) const {
    using K = Topic::Kind;
    const auto talkedIt = talked_.find(to.id);
    auto raisedAt = [&](uint64_t key) -> const float* {
        if (talkedIt == talked_.end()) return nullptr;
        auto it = talkedIt->second.find(key);
        return it == talkedIt->second.end() ? nullptr : &it->second;
    };
    auto due = [&](uint64_t key, float again) {
        const float* t = raisedAt(key);
        return !t || clock_ - *t >= again;
    };
    std::optional<Topic> best;
    auto offer = [&](Topic t) {
        if (t.score > 0.0f && (!best || t.score > best->score)) best = std::move(t);
    };

    // Shared and Story: what it remembers, not yet told them.
    for (const Episode& e : episodes_) {
        float s = salience(e);
        // Waking up here is what Before is about.
        if (s < EPISODE_WORTH || !owns(e) || e.what == Episode::What::Woke) continue;
        if (std::find(e.toldTo.begin(), e.toldTo.end(), to.id) != e.toldTo.end()) continue;
        Topic t{to.shares(e) ? K::Shared : K::Story};
        t.score = s * (t.kind == K::Shared ? SHARED_WEIGHT : STORY_WEIGHT);
        t.episode = e;
        t.key = keyOf(t.kind, int(e.what), int(e.since * 10.0f), e.who);
        offer(std::move(t));
    }
    // Here: the most striking thing in sight, close by.
    float curious = 1.0f + HERE_CURIOUS * (traits.curious - 0.5f) * 2.0f;
    for (const auto& [whose, b] : bodies_) {
        if (whose == to.id || cheb(cell, b.cell) > HERE_RANGE || !sight.sees(b.cell)) continue;
        Topic t{K::Here};
        t.thing = Topic::Thing::Body;
        // Bones it never knew for whose are just someone's.
        bool known = knowsWhose(whose);
        t.who = known ? whose : -1;
        t.cell = b.cell;
        t.key = keyOf(K::Here, int(t.thing), whose);
        t.score = (known && isFriend(whose) ? HERE_FRIEND_BODY : HERE_BODY) * curious;
        if (due(t.key, HERE_AGAIN)) offer(std::move(t));
    }
    float now = world.time();
    sight.forEach([&](Vec2i c) {
        if (cheb(cell, c) > HERE_RANGE) return;
        Topic t{K::Here};
        t.cell = c;
        if (const Statue* st = world.statue(c)) {
            // A statue is one thing, however many cells it stands on.
            t.cell = st->at;
            t.thing = Topic::Thing::Statue;
            t.detail = int(st->kind) * 16 + int(st->material);
            t.score = HERE_STATUE;
        } else if (Tile tile = world.tile(c); tile == Tile::Water || tile == Tile::Basin) {
            // Foul water is worth a word only if it knows: it drank from it.
            bool foul = world.foul(c) >= HERE_WATER_FOUL &&
                        std::any_of(episodes_.begin(), episodes_.end(), [&](const Episode& e) {
                            return e.what == Episode::What::FoulWater && cheb(e.cell, c) <= HERE_FOUL_NEAR;
                        });
            t.thing = tile == Tile::Water ? Topic::Thing::Water : Topic::Thing::Basin;
            t.detail = foul ? 1 : 0;
            t.score = foul ? HERE_FOUL : HERE_WATER;
            // A basin is one place; a body of water is remarked on once
            // round here.
            t.cell = tile == Tile::Water ? World::slotOf(c) : c;
        } else if (memory.pools.count(c)) {
            t.thing = Topic::Thing::Blood;
            t.score = HERE_BLOOD;
        } else if (world.mess().disgust(c, now) >= HERE_MESS_DISGUST) {
            t.thing = Topic::Thing::Mess;
            t.detail = world.mess().freshFeces(c, now) > 0.0f ? 1 : 0;
            t.score = HERE_MESS;
        } else {
            return;
        }
        t.score *= curious;
        t.key = keyOf(K::Here, int(t.thing), t.cell.x, t.cell.y);
        if (due(t.key, HERE_AGAIN)) offer(std::move(t));
    });

    // Someone: a third human it thinks firmly of, or a friend it saw dead.
    for (const auto& [other, tr] : trust_) {
        if (other == to.id || other == id) continue;
        const Human& o = world.human(other);
        if (!o.dead() && sight.sees(o.cell) && cheb(cell, o.cell) <= SOMEONE_EARSHOT) continue;
        // Someone they both know, or it can name.
        if (!(to.met.count(other) || knownName(other))) continue;
        bool mourned = seenDead_.count(other) && isFriend(other);
        if (!mourned && std::abs(tr) < SOMEONE_FIRM) continue;
        Topic t{K::Someone};
        t.who = other;
        t.opinion = tr;
        t.score = mourned ? SOMEONE_DEAD : SOMEONE_WEIGHT * std::abs(tr);
        t.key = keyOf(K::Someone, 0, other);
        if (const float* was = raisedAt(t.key); was && std::abs(*was - tr) < SOMEONE_MOVED) continue;
        offer(std::move(t));
    }

    // How it is doing.
    auto feel = [&](Topic::Feel f, float score) {
        if (score <= 0.0f) return;
        Topic t{K::Feeling};
        t.feel = f;
        t.score = score;
        t.key = keyOf(K::Feeling, int(f), 0);
        if (due(t.key, FEEL_AGAIN)) offer(std::move(t));
    };
    if (hunger >= PECKISH && !hungry()) feel(Topic::Feel::Peckish, NEEDS_SOON * (hunger - PECKISH) / (0.5f - PECKISH));
    if (thirst >= PECKISH && !thirsty()) feel(Topic::Feel::Dry, NEEDS_SOON * (thirst - PECKISH) / (0.5f - PECKISH));
    if (clock_ - restedAt_ >= FOOTSORE_TIME) feel(Topic::Feel::Footsore, FOOTSORE);
    if (dread_ >= UNEASY) feel(Topic::Feel::Uneasy, UNEASY_WEIGHT * dread_);
    if (gloom() > 0.0f) feel(Topic::Feel::Gloomy, GLOOMY_WEIGHT * (0.5f + 0.5f * gloom()));
    if (hope_ >= HOPEFUL && !settled_) feel(Topic::Feel::Hopeful, HOPEFUL_WEIGHT);

    // The way out.
    if (outDrive() >= WAY_WANT && hunchSure_ >= WAY_SURE_ENOUGH) {
        Topic t{K::Way};
        t.score = WAY_WEIGHT * outDrive();
        t.key = keyOf(K::Way, 0, 0);
        if (due(t.key, WAY_AGAIN)) offer(std::move(t));
    }

    // The two of them, when one keeps with the other: the places they went
    // into together.
    if (follows() == to.id || to.follows() == id) {
        Topic t{K::Group};
        t.who = follows() == to.id ? to.id : id;
        for (const Episode& e : episodes_)
            if (e.what == Episode::What::NewPlace && std::find(e.with.begin(), e.with.end(), to.id) != e.with.end())
                t.count++;
        t.score = t.count >= GROUP_PLACES ? GROUP_WEIGHT + GROUP_PER_PLACE * float(t.count) : 0.0f;
        t.key = keyOf(K::Group, 0, 0);
        if (due(t.key, GROUP_AGAIN)) offer(std::move(t));
    }

    // Before: never remembered, and asked about once per human.
    {
        Topic t{K::Before};
        float fresh = std::max(0.0f, 1.0f - clock_ / BEFORE_FRESH);
        t.score = BEFORE_LATER + BEFORE_WEIGHT * fresh + (dreamt() ? BEFORE_DREAMT : 0.0f);
        t.key = keyOf(K::Before, 0, 0);
        if (!raisedAt(t.key)) offer(std::move(t));
    }
    return best;
}

void Human::raised(const Topic& t, int to) {
    if (t.kind == Topic::Kind::Shared || t.kind == Topic::Kind::Story) {
        for (Episode& e : episodes_)
            if (sameEpisode(e, t.episode) && std::find(e.toldTo.begin(), e.toldTo.end(), to) == e.toldTo.end())
                e.toldTo.push_back(to);
        return;
    }
    talked_[to][t.key] = t.kind == Topic::Kind::Someone ? t.opinion : clock_;
}
