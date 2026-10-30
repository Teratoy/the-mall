#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Chit-chat (AI/chitchat_plan.md). A calm human grows an urge to talk to
// each human it knows and has in sight, at its own pace: faster the more
// sociable, between friends and when close, slower when frightened, gloomy
// or not thinking well of the other. No dice: once full, it speaks as soon
// as the two are close, calm and neither is talking; full and farther off,
// it goes over to catch up with them (wouldGreet). A chat starts the
// silence between the two over.



namespace {

/// Together: in sight and this close. The urge grows toward anyone known
/// in sight this close, CHAT_FAR as fast beyond CHAT_RANGE.
constexpr int CHAT_RANGE = 5;
constexpr int CHAT_SEEN = 10;
constexpr float CHAT_FAR = 0.5f;
/// Urge per second for the average human (sociable 0.5) toward a stranger
/// it has met: full in CHAT_EVERY seconds. Sociability sways it (half to
/// one and a half), a friend makes it this much faster, a disliked one this
/// much slower; dread and gloom slow it by up to this much.
constexpr float CHAT_EVERY = 75.0f;
constexpr float CHAT_FRIEND = 1.5f;
constexpr float CHAT_DISLIKED = 0.5f;
constexpr float CHAT_DREAD = 0.6f;
constexpr float CHAT_GLOOM = 0.4f;
/// Apart, the urge toward someone ebbs this much per second.
constexpr float CHAT_FADE = 1.0f / 300.0f;
/// Urges are brought up to date this often.
constexpr float CHAT_TICK = 0.25f;
/// Answering: below this sociability it brushes off all but friends.
/// Thinking of someone this firmly it agrees or disagrees about them; less,
/// it hardly knows them. Peckish, dry, footsore and uneasy as in
/// human_topic.cpp; hopeful from REPLY_HOPEFUL hope. Hunches this close (in
/// radians) agree on the way out; it has no idea below REPLY_UNSURE.
constexpr float REPLY_LONER = 0.3f;
constexpr float REPLY_FIRM = 0.15f;
constexpr float REPLY_PECKISH = 0.35f, REPLY_FOOTSORE = 600.0f, REPLY_UNEASY = 0.25f, REPLY_HOPEFUL = 0.7f;
constexpr float REPLY_SAME_WAY = 1.0f, REPLY_UNSURE = 0.15f;
/// A chat runs to CHAT_TURNS lines (the opener and the answer), one more
/// when the two are this sociable on average, one more again between
/// friends or two who are both this sociable. While it goes on, idlers
/// stand at least CHAT_HOLD seconds.
constexpr int CHAT_TURNS = 2;
constexpr float CHAT_LONGER = 0.5f, CHAT_LONGEST = 0.6f;
constexpr float CHAT_HOLD = 2.5f;
constexpr float TAU = 6.28318531f;

int cheb(Vec2i a, Vec2i b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
float sway(float t) { return 0.5f + t; }

} // namespace

bool Human::easyInMind() const {
    return !dead() && !hungry() && !thirsty() && !needsToGo() && bleeding() <= 0.0f && fear_ <= 0.0f && hurtTime <= 0.0f;
}

bool Human::calmForChat(const Human& with) const {
    if (!easyInMind()) return false;
    switch (state) {
    case State::Idle:
        return !awaiting_ && goal != Goal::Escort && goal != Goal::Escorted;
    case State::Sitting:
        return true;
    case State::Walking:
        // On the move only with its own group, the one after the other, and
        // not on anything urgent.
        if (follows() != with.id && with.follows() != id) return false;
        switch (goal) {
        case Goal::Fight:
        case Goal::Flee:
        case Goal::Share:
        case Goal::GiveWay:
        case Goal::Escort:
        case Goal::Escorted:
        case Goal::Avoid:
        case Goal::Loot:
        case Goal::Feed:
        case Goal::Drink:
        case Goal::Relieve:
        case Goal::Sleep:
        case Goal::Rob:
        case Goal::Wash:
        case Goal::Track:
            return false;
        default:
            return true;
        }
    default:
        return false;
    }
}

bool Human::knowsInSight(const Human& o, int range) const {
    return &o != this && !o.dead() && met.count(o.id) && sight.sees(o.cell) && cheb(cell, o.cell) <= range &&
           !isFoe(o.id) && !suspicious(o.id);
}

bool Human::togetherWith(const Human& o) const { return knowsInSight(o, CHAT_RANGE); }

void Human::chatUrges(World& world, float dt) {
    chatTick_ += dt;
    if (chatTick_ < CHAT_TICK) return;
    float t = chatTick_;
    chatTick_ = 0.0f;
    float pace = sway(traits.sociable) * (1.0f - CHAT_DREAD * dread_) * (1.0f - CHAT_GLOOM * gloom()) / CHAT_EVERY;
    bool easy = easyInMind();
    Human* best = nullptr;
    float bestUrge = 0.0f;
    for (const auto& o : world.humans()) {
        if (o.get() == this) continue;
        auto it = chatUrge_.find(o->id);
        if (!easy || !knowsInSight(*o, CHAT_SEEN)) {
            // Out of sight, or not now: the urge ebbs, and is forgotten once gone.
            if (it != chatUrge_.end() && (it->second -= CHAT_FADE * t) <= 0.0f) chatUrge_.erase(it);
            continue;
        }
        float tr = trust(o->id);
        float u = std::min(1.0f, chatUrge_[o->id] += pace * t * (togetherWith(*o) ? 1.0f : CHAT_FAR) *
                                                     (isFriend(o->id) ? CHAT_FRIEND : tr < 0.0f ? CHAT_DISLIKED : 1.0f));
        // The fullest urge speaks first; ties to the one seen first.
        if (u >= 1.0f && u > bestUrge && togetherWith(*o) && calmForChat(*o) && o->calmForChat(*this) &&
            !o->talking()) {
            best = o.get();
            bestUrge = u;
        }
    }
    if (best && !talking()) startChat(world, *best);
}

void Human::quiet(Human& o) {
    chatUrge_.erase(o.id);
    o.chatUrge_.erase(id);
}

// The urge spoken: it brings up what matters most to it, about whoever
// the topic is about, and the silence starts over.
void Human::startChat(World& world, Human& o) {
    quiet(o);
    lastTopic = pickTopic(world, o);
    if (!lastTopic) return;
    const Topic& t = *lastTopic;
    int about = t.who;
    if (t.kind == Topic::Kind::Shared || t.kind == Topic::Kind::Story) {
        // The one it was about who is neither of them, if any.
        const Episode& e = t.episode;
        about = e.who != id && e.who != o.id ? e.who : e.whom != id && e.whom != o.id ? e.whom : -1;
    }
    if (t.kind == Topic::Kind::Group) about = -1;
    Saying s{talk::chatMoment(t.kind), o.id, about};
    s.topic = t;
    if (sayLater(std::move(s))) chats++;
}

Topic::Reply Human::replyTo(const Human& sp, const Topic& t) const {
    using R = Topic::Reply;
    using F = Topic::Feel;
    if (!easyInMind() || trust(sp.id) < 0.0f || (traits.sociable < REPLY_LONER && !isFriend(sp.id))) return R::Brush;
    switch (t.kind) {
    case Topic::Kind::Shared:
    case Topic::Kind::Group:
    case Topic::Kind::Before:
        return R::Agree;
    case Topic::Kind::Story:
        // Been through the same kind of thing itself.
        return std::any_of(episodes_.begin(), episodes_.end(),
                           [&](const Episode& e) { return e.what == t.episode.what && e.what != Episode::What::Woke; })
                   ? R::Add
                   : R::Unheard;
    case Topic::Kind::Here:
        if (t.thing == Topic::Thing::Body && knowsDead(t.who) && isFriend(t.who)) return R::Add;
        return R::Agree;
    case Topic::Kind::Someone: {
        if (!met.count(t.who) && !knownName(t.who)) return R::Unheard;
        if (sp.knowsDead(t.who) && !knowsDead(t.who)) return R::Unheard;
        float mine = trust(t.who);
        if (std::abs(mine) < REPLY_FIRM) return R::Unheard;
        return (mine > 0.0f) == (t.opinion > 0.0f) ? R::Agree : R::Disagree;
    }
    case Topic::Kind::Feeling:
        // Feels the same, the opposite, or not, and says something kind.
        switch (t.feel) {
        case F::Peckish: return hunger >= REPLY_PECKISH ? R::Agree : R::Add;
        case F::Dry: return thirst >= REPLY_PECKISH ? R::Agree : R::Add;
        case F::Footsore: return clock_ - restedAt_ >= REPLY_FOOTSORE ? R::Agree : R::Add;
        case F::Uneasy: return dread_ >= REPLY_UNEASY ? R::Agree : R::Add;
        case F::Gloomy: return gloom() > 0.0f ? R::Agree : hope_ >= REPLY_HOPEFUL ? R::Disagree : R::Add;
        case F::Hopeful: return gloom() > 0.0f ? R::Disagree : hope_ >= REPLY_HOPEFUL ? R::Agree : R::Add;
        default: return R::Add;
        }
    case Topic::Kind::Way: {
        if (gloom() > 0.0f) return R::Disagree;
        if (hunchSure_ < REPLY_UNSURE) return R::Unheard;
        float d = std::abs(std::remainder(hunch_ - sp.hunch(), TAU));
        return d <= REPLY_SAME_WAY ? R::Agree : R::Disagree;
    }
    default:
        return R::Agree;
    }
}

// After each line of a chat, the next one, said by the other: the answer,
// then the follow-up, then the close, as long as the chat runs.
void Human::chatOn(Human& to, const Saying& s) {
    Topic next = *s.topic;
    if (next.turn >= next.turns) return;
    if (next.turn == 1) {
        next.reply = to.replyTo(*this, next);
        float both = (traits.sociable + to.traits.sociable) * 0.5f;
        bool close = (isFriend(to.id) && to.isFriend(id)) ||
                     std::min(traits.sociable, to.traits.sociable) >= CHAT_LONGEST;
        next.turns = next.reply == Topic::Reply::Brush ? CHAT_TURNS
                                                       : CHAT_TURNS + (both >= CHAT_LONGER) + (both >= CHAT_LONGER && close);
    }
    next.turn++;
    talk::Moment m = next.turn == 2   ? talk::replyMoment(next.reply)
                     : next.turn == 3 ? talk::Moment::ChatFollow
                                      : talk::Moment::ChatClose;
    Saying answer{m, id, s.about};
    answer.topic = next;
    to.sayLater(std::move(answer));
    // Idlers stay for it.
    for (Human* h : {this, &to})
        if (h->state == State::Idle && h->timer < CHAT_HOLD) h->timer = CHAT_HOLD;
}

// Answering, it has talked of it too: its own memory of the same thing is
// told, the same thing in sight or felt kept, its own opinion of whoever it
// was about.
void Human::answered(const Topic& t, int to) {
    Topic mine = t;
    if (t.kind == Topic::Kind::Shared) {
        const Episode* own = shares(t.episode);
        if (!own) return;
        mine.episode = *own;
    } else if (t.kind == Topic::Kind::Story) {
        return;
    } else if (t.kind == Topic::Kind::Someone) {
        mine.opinion = trust(t.who);
    }
    raised(mine, to);
}
