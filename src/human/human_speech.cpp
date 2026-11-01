// Speaking: queuing lines, saying them in turn, hearing them, and the
// pleas and threats that wait on an answer; and the life log.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <queue>

void Human::note(std::string what, int who, int whom) { log.add(born + clock_, std::move(what), who, whom); }

bool Human::sayLater(talk::Moment m, int to, int about) { return sayLater(Saying{m, to, about}); }

bool Human::sayLater(Saying s) {
    // An emergency (a blow, a cry, the shout of who killed) goes first, and
    // the newest wins: the ordinary lines still to be said are dropped, and
    // so is an older blow not yet said. Other urgent ones keep their place.
    if (talk::isUrgent(s.moment)) {
        std::erase_if(voice_.sayings, [&](const Saying& q) {
            bool drop = !talk::isUrgent(q.moment) || (talk::isCry(q.moment) && talk::isCry(s.moment));
            if (drop && q.awaited) awaiting_ = false;
            return drop;
        });
        voice_.sayings.insert(voice_.sayings.begin(), std::move(s));
        return true;
    }
    // A call into the mall, or a cry for help from a slam room, cuts in ahead
    // of whatever was still to be said, behind any emergency.
    if (s.moment == talk::Moment::CallOut || s.moment == talk::Moment::CallAnswer || s.moment == talk::Moment::Help) {
        auto at = std::find_if(voice_.sayings.begin(), voice_.sayings.end(),
                               [](const Saying& q) { return !talk::isUrgent(q.moment); });
        voice_.sayings.insert(at, std::move(s));
        return true;
    }
    // Word to pass on (a tip, a lie, a name) goes ahead of small talk still
    // to be said, after the hello, so it is said while the greeting lasts.
    bool word = s.tell || s.intro;
    if (!word && voice_.sayings.size() > SPEECH_QUEUE) return false;
    auto at = voice_.sayings.end();
    if (word)
        at = std::find_if(voice_.sayings.begin(), voice_.sayings.end(), [](const Saying& q) {
            return !talk::isUrgent(q.moment) && !q.tell && !q.intro && q.moment != talk::Moment::Meet &&
                   q.moment != talk::Moment::CatchUp;
        });
    voice_.sayings.insert(at, std::move(s));
    return true;
}

bool Human::hasWordFor(int to) const {
    return std::any_of(voice_.sayings.begin(), voice_.sayings.end(),
                       [&](const Saying& s) { return s.to == to && (s.tell || s.intro); });
}

void Human::awaitLine() {
    voice_.sayings.back().awaited = true;
    awaiting_ = true;
    voice_.answerWait = timer;
}

void Human::speak(World& world, float dt) {
    if (!voice_.speech.empty() && (voice_.speech.front().shown += dt) >= voice_.speech.front().time) {
        voice_.speech.pop_front();
        voice_.quietSince = world.time();
    }
    while (!voice_.sayings.empty()) {
        Saying& s = voice_.sayings.front();
        // A line to someone gone out of earshot goes unsaid.
        bool urgent = talk::isUrgent(s.moment);
        const Human* to = s.to >= 0 ? &world.human(s.to) : nullptr;
        bool gone = to && (to->dead() || cheb(cell, to->cell) > HEAR_RADIUS || !sight.sees(to->cell));
        if (gone || s.waited > SPEECH_STUCK) {
            if (s.awaited) awaiting_ = false;
            voice_.sayings.erase(voice_.sayings.begin());
            continue;
        }
        // The line showing is read to its end, but an emergency cuts in once
        // it has shown long enough to read.
        if (!voice_.speech.empty()) {
            if (!urgent || voice_.speech.front().shown < SPEECH_MIN) return;
            Saying due = std::move(s);
            voice_.sayings.erase(voice_.sayings.begin());
            size_t before = voice_.speech.size();
            say(world, due);
            // Worded, it takes the old line's place.
            if (voice_.speech.size() > before) voice_.speech.pop_front();
            return;
        }
        if (!urgent) {
            float now = world.time();
            if (now - voice_.quietSince < SPEECH_PAUSE) return;
            // Its turn: not over the one it talks to, nor over anyone talking
            // to it; and having just spoken, it lets them answer first.
            for (const auto& h : world.humans()) {
                if (h.get() == this || h->dead()) continue;
                const Speech* sp = h->speaking();
                bool party = h->id == s.to || (sp && sp->to == id);
                bool answer = h->id == s.to && !h->voice_.sayings.empty() && h->voice_.sayings.front().to == id &&
                              voice_.quietSince > h->voice_.quietSince;
                if (answer || (party && (sp || now - h->voice_.quietSince < SPEECH_PAUSE))) {
                    s.waited += dt;
                    return;
                }
            }
        }
        Saying due = std::move(s);
        voice_.sayings.erase(voice_.sayings.begin());
        return say(world, due);
    }
}

void Human::say(World& world, const Saying& s) {
    Human* to = s.to >= 0 ? &world.human(s.to) : nullptr;
    if (s.awaited) {
        awaiting_ = false;
        // Said: now the answer is waited for.
        if (state == State::Idle) timer = voice_.answerWait;
    }
    if (to && to->dead()) return;
    // Asking where water is of one who has shown since that they know of none.
    if (to && s.moment == talk::Moment::AskWater && water_.knewNone.count(to->id)) return;
    const Human* about = s.about >= 0 ? &world.human(s.about) : nullptr;
    // Which time this is holds (again, often) only for the line said of it.
    repeat_.saying = s.repeat;
    auto said = s.quiet ? std::nullopt
                        : talk::pick(s.moment, *this, to, about, rng_, s.topic ? &*s.topic : nullptr, &voice_.chatLines,
                                     world.time());
    repeat_.saying = 0;
    // A topic is raised once it is said; unsaid, it can come up again.
    if (said && to && s.topic) {
        if (s.topic->turn == 1)
            raised(*s.topic, to->id);
        else if (s.topic->turn == 2)
            answered(*s.topic, to->id);
        chatOn(*to, s);
    }
    if (said) {
        voice_.speech.push_back(
            {said->text, said->tone, 0.0f, SPEECH_TIME + SPEECH_PER_LETTER * float(said->text.size()), s.to, std::nullopt});
        // Telling the way, it turns and points it out.
        if (s.tell && s.tell->kind == Tell::Kind::Way && !s.tell->gloomy) {
            voice_.speech.back().points = s.tell->way;
            if (std::abs(std::cos(s.tell->way)) > 0.01f) facingLeft = std::cos(s.tell->way) < 0.0f;
        }
        std::string quote = '"' + said->text + '"';
        if (to)
            note("SAID " + quote + " TO @", to->id);
        else
            note("SAID " + quote);
        for (const auto& h : world.humans())
            if (h.get() != this && !h->dead() && cheb(h->cell, cell) <= HEAR_RADIUS && h->sight.sees(cell))
                h->hear(world, *this, *said, to, about);
    // Told who killed, those in earshot may believe it.
    if (said && about && s.moment == talk::Moment::Blame)
        for (const auto& h : world.humans())
            if (h.get() != this && !h->dead() && h->state != State::Sleeping && cheb(h->cell, cell) <= HEAR_RADIUS &&
                h->sight.sees(cell))
                h->heardBlame(*this, about->id);
    }
    // A call carries through walls to whoever can't see it (human_call.cpp).
    if (said && (s.moment == talk::Moment::CallOut || s.moment == talk::Moment::CallAnswer))
        shout(world, s.moment == talk::Moment::CallAnswer);
    // Turned away out loud, it may answer back.
    if (said && to && s.moment == talk::Moment::Refuse) to->sayLater(talk::Moment::Snubbed, id);
    // Joined out loud, the other answers.
    if (said && to && s.moment == talk::Moment::Join) to->sayLater(talk::Moment::JoinReply, id);
    // Asked the way out, the other answers.
    if (said && to && s.moment == talk::Moment::AskWay) to->answerWay(*this);
    // Asked for a drink, worded or not, the other answers (human_bottle.cpp).
    if (to && s.moment == talk::Moment::AskDrink) to->answerDrink(world, *this);
    // Asked where water is, out loud, the other says what it knows (human_thirst.cpp).
    if (said && to && s.moment == talk::Moment::AskWater) to->answerWater(world, *this);
    // What it tells lands now, worded or not.
    if (to && s.intro) giveName(*to);
    if (to && s.tell) deliver(*to, *s.tell);
    // Its owner, hearing a stash told of, means to move it (human_stash.cpp).
    if (to && s.tell && s.tell->kind == Tell::Kind::Stash && s.tell->owner >= 0) stashToldOf(world, *s.tell, *to);
}

void Human::hear(World&, Human& s, const talk::Said& said, Human* to, const Human* about) {
    using talk::Tone;
    if (state == State::Sleeping) return;
    bool mine = to == this;
    std::string quote = '"' + said.text + '"';
    if (mine)
        note("@ SAID " + quote, s.id);
    else if (to)
        note("HEARD @ SAY " + quote + " TO #", s.id, to->id);
    else
        note("HEARD @ SAY " + quote, s.id);
    // A name said aloud is learnt by whoever can see whose it is.
    auto learn = [&](const Human& h) {
        if (&h == this || knownName(h.id) || !sight.sees(h.cell)) return;
        memory.names[h.id] = h.name;
        note("HEARD @'S NAME", h.id);
    };
    if (said.namesMe) learn(s);
    if (said.namesThem && to) learn(*to);
    if (said.namesWho && about) learn(*about);

    switch (said.tone) {
    case Tone::Plain:
        break;
    case Tone::Warm:
        if (mine) regard(s.id, TRUST_WARM);
        break;
    case Tone::Rude:
        if (mine) regard(s.id, TRUST_RUDE);
        break;
    case Tone::Fear:
        if (trust(s.id) >= 0.0f) scare(DREAD_TALK * (1.0f + trust(s.id)));
        break;
    case Tone::Plead:
        // Moved or not, it decides once per plea.
        if (mine && cakes > 0 && !movedBy(s.id) &&
            rng_.chance(PLEA_MOVES * sway(traits.sociable) * sway(1.0f - traits.greedy) +
                        (isFriend(s.id) ? PLEA_FRIEND : 0.0f))) {
            voice_.moved[s.id] = clock_ + MOVED_TIME;
            rethink_ = true;
        }
        break;
    case Tone::Threat: {
        if (!mine) {
            // Overheard, it reflects on the threatener, unless the threatened is a foe.
            if (!to || !isFoe(to->id)) regard(s.id, TRUST_HEARD_THREAT);
            break;
        }
        // Not while chasing or running, nor to whoever it is after.
        bool rushing = state == State::Walking && (goal == Goal::Fight || goal == Goal::Flee);
        bool free = state == State::Idle || state == State::Walking || state == State::Sitting ||
                    state == State::Rising;
        if (free && !rushing && fight_.foe != s.id && !fight_.provoked && cakes > 0 && !starving() && cheb(cell, s.cell) <= THREAT_RANGE &&
            rng_.chance(YIELD_CHANCE * (1.0f - traits.brave) * (1.0f + fright()))) {
            sayLater(talk::Moment::Yield, s.id);
            give(s, Gift::Yield);
        } else {
            regard(s.id, TRUST_THREAT);
            sayLater(talk::Moment::Defy, s.id);
        }
        break;
    }
    }
}

bool Human::movedBy(int id) const {
    auto it = voice_.moved.find(id);
    return it != voice_.moved.end() && it->second > clock_;
}

// Only when it knows of no cake lying about, unless starving; never a foe,
// or whoever it is afraid of.
bool Human::plead(World& world) {
    if (!hungry() || cakes > 0 || (!memory.cakes.empty() && !starving())) return false;
    Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->cakes == 0 || !sight.sees(o->cell) || cheb(cell, o->cell) > HEAR_RADIUS)
            continue;
        if (isFoe(o->id) || (fight_.fear > 0.0f && o->id == fight_.feared)) continue;
        if (auto it = voice_.begged.find(o->id); it != voice_.begged.end() && clock_ - it->second < BEG_AGAIN) continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    if (!best) return false;
    voice_.begged[best->id] = clock_;
    float p = BEG_CHANCE * sway(1.0f - traits.brave) * sway(traits.sociable) * sway(1.0f - traits.greedy) +
              (starving() ? BEG_STARVING : 0.0f);
    if (!rng_.chance(p)) return false;
    if (!sayLater(talk::Moment::Beg, best->id)) return false;
    startIdle(BEG_WAIT_MIN, BEG_WAIT_MAX);
    awaitLine();
    if (best->cell.x != cell.x) facingLeft = best->cell.x < cell.x;
    return true;
}

bool Human::threaten(Human& f) {
    if (auto it = voice_.threatened.find(f.id); it != voice_.threatened.end() && clock_ - it->second < THREAT_AGAIN) return false;
    voice_.threatened[f.id] = clock_;
    if (!rng_.chance(THREAT_BASE + THREAT_BOLD * (traits.brave + traits.greedy) / 2.0f)) return false;
    if (!sayLater(talk::Moment::Threat, f.id)) return false;
    startIdle(THREAT_WAIT_MIN, THREAT_WAIT_MAX);
    awaitLine();
    if (f.cell.x != cell.x) facingLeft = f.cell.x < cell.x;
    return true;
}
