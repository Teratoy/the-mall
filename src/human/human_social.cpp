// Others: trust, greeting, following, and what is told at a greeting:
// news, lies, names, gossip and the way out.
#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <queue>

float Human::trust(int id) const {
    auto it = social_.trust.find(id);
    float t = it == social_.trust.end() ? 0.0f : it->second;
    if (auto w = social_.wronged.find(id); w != social_.wronged.end()) t = std::max(-1.0f, t - w->second.guilt);
    return t;
}

// Nothing done back to one who hit it, nor to a foe, weighs on it.
void Human::wrong(const Human& victim, bool known) {
    if (victim.id == id || fight_.attacker == victim.id || isFoe(victim.id)) return;
    auto& w = social_.wronged[victim.id];
    w.guilt = std::min(1.0f, w.guilt - TRUST_WRONGED);
    w.shuns = w.shuns || known || !cunning();
    if (isFoe(victim.id)) {
        note("@ IS A FOE NOW", victim.id);
        remember(Episode::What::Foe, EPISODE_FOE, id, victim.id);
    }
    if (!w.shuns || (social_.companion != victim.id && social_.leader != victim.id)) return;
    if (social_.companion == victim.id) social_.companion = -1;
    if (social_.leader == victim.id) social_.leader = -1;
    note("PARTED FROM @ AFTER WHAT I DID", victim.id);
    sayLater(talk::Moment::PartWronged, -1, victim.id);
}

bool Human::shuns(int id) const {
    auto it = social_.wronged.find(id);
    return it != social_.wronged.end() && it->second.shuns;
}

void Human::regard(int id, float delta) {
    if (id == this->id) return;
    float& t = social_.trust[id];
    float was = t;
    t = std::clamp(t + delta, -1.0f, 1.0f);
    if (was < TRUST_FRIEND && t >= TRUST_FRIEND) {
        note("@ IS A FRIEND NOW", id);
        remember(Episode::What::Friend, EPISODE_FRIEND, this->id, id);
        lift(HOPE_FRIEND);
    }
    if (was > TRUST_FOE && t <= TRUST_FOE) {
        note("@ IS A FOE NOW", id);
        remember(Episode::What::Foe, EPISODE_FOE, this->id, id);
    }
}

bool Human::isFriend(int id) const { return trust(id) >= TRUST_FRIEND; }

bool Human::isFoe(int id) const { return trust(id) <= TRUST_FOE; }

float Human::grudgeFor(int id) const { return GRUDGE_TIME * (1.0f + std::max(0.0f, -trust(id))); }

bool Human::startMeet(World& world) {
    Human& o = world.human(social_.partner);
    if (!wouldGreet(o)) {
        social_.partner = -1;
        return false;
    }
    bool seen = sight.sees(o.cell);
    Vec2i at = seen ? o.cell : social_.lastSeen[o.id];
    if (seen && cheb(cell, at) <= GREET_RADIUS) {
        tryGreet(world, o);
        return true;
    }
    if (!seen && cheb(cell, at) <= 1) {
        // Got where they were; they're gone.
        social_.partner = -1;
        return false;
    }
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= 1; });
    if (!p) {
        social_.partner = -1;
        return false;
    }
    startWalking(Goal::Meet, *p);
    return true;
}

// Both wave; someone busy waving at another, or eating, is waited for.
void Human::tryGreet(World& world, Human& other) {
    if (other.dead()) {
        social_.partner = -1;
        return startIdle(0.0f, 0.0f);
    }
    // Busy with something that presses: left be a while, not waited on.
    if (!other.approachable()) {
        social_.busy[other.id] = clock_;
        social_.partner = -1;
        return startIdle(FOLLOW_WAIT_MIN, FOLLOW_WAIT_MAX);
    }
    // It takes two: turned away, it leaves them be a while.
    if (!other.welcomes(*this)) {
        social_.snubbed[other.id] = clock_;
        social_.partner = -1;
        note("@ WOULDN'T GREET ME", other.id);
        other.note("WOULDN'T GREET @", id);
        other.sayLater(talk::Moment::Refuse, id);
        // It stands there a moment, to hear it and maybe answer back.
        return startIdle(SNUBBED_WAIT_MIN, SNUBBED_WAIT_MAX);
    }
    greet(other);
    other.greet(*this);
    // Each tells the other something, or lies to them, before names, so the
    // news is said while the greeting lasts.
    exchange(world, other);
    other.exchange(world, *this);
    // Each decides whether to say who it is; being told a name invites one back.
    bool mine = wouldIntroduce(other, false);
    bool theirs = other.wouldIntroduce(*this, mine);
    if (mine) introduce(other);
    if (theirs) other.introduce(*this);
}

// Only what the teller itself knows: it may vouch for or warn against
// someone already dead, never having seen the body.
std::optional<Human::Tell> Human::gossipFor(const Human& o) const {
    if (trust(o.id) < TRUST_CONFIDE || o.trust(id) < TRUST_CONFIDE) return std::nullopt;
    Tell t{Tell::Kind::People, cell};
    auto nameFor = [&](int who) {
        const std::string* n = knownName(who);
        return n && !o.knownName(who) ? *n : std::string();
    };
    for (const auto& [who, real] : social_.trust) {
        if (who == o.id || who == id) continue;
        // The cunning make anyone they are wary of out to be dangerous.
        float opinion = cunning() && real < 0.0f ? std::min(real, SLANDER) : real;
        if (std::abs(opinion) < GOSSIP_WORTH) continue;
        auto h = o.memory.heard.find(Memory::gossipKey(id, who));
        float told = h == o.memory.heard.end() ? 0.0f : h->second;
        if (std::abs(opinion - told) < GOSSIP_NEWS) continue;
        t.people.push_back({who, nameFor(who), opinion, opinion - real <= -SLANDER_LIE});
    }
    for (const auto& [who, n] : memory.names) {
        if (who == o.id || o.knownName(who)) continue;
        bool said = std::any_of(t.people.begin(), t.people.end(), [&](const Tell::Word& w) { return w.about == who; });
        if (!said) t.people.push_back({who, n, std::nullopt});
    }
    if (t.people.empty()) return std::nullopt;
    return t;
}

int Human::friendCount() const {
    int n = 0;
    for (const auto& [who, t] : social_.trust) n += t >= TRUST_FRIEND;
    return n;
}

int Human::foeCount() const {
    int n = 0;
    for (const auto& [who, t] : social_.trust) n += t <= TRUST_FOE;
    return n;
}

void Human::exchange(World& world, Human& o) {
    if (auto word = gossipFor(o)) {
        tell(o, *word);
        gossiping = o.gossiping = true;
    }
    // One asks; the other answers, then may ask back.
    bool asked = std::any_of(o.voice_.sayings.begin(), o.voice_.sayings.end(), [&](const Saying& s) {
        return s.moment == talk::Moment::AskWay && s.to == id;
    });
    if (!asked && wouldAskWay(o)) {
        escape_.askedWay.insert(o.id);
        if (rng_.chance(ASK_WAY_CHANCE)) sayLater(talk::Moment::AskWay, o.id);
    }
    // A lie with bait in it first; a false tip only once water is seen to.
    auto lie = lieFor(world, o);
    if (lie && !lie->nothing) return tell(o, *lie);
    bool forPay = false;
    // Water ahead of anything else to the thirsty who'd sooner drink than
    // eat; else after it (human_thirst.cpp).
    bool waterFirst = o.thirsty() && o.thirst >= o.hunger;
    if (waterFirst)
        if (auto w = waterFor(world, o, forPay)) {
            w->sold = forPay;
            return tell(o, *w);
        }
    if (lie) return tell(o, *lie);
    if (auto g = gainLieFor(world, o, forPay)) {
        g->sold = forPay;
        return tell(o, *g);
    }
    auto news = newsFor(o, forPay);
    if (!news && !waterFirst) news = waterFor(world, o, forPay);
    if (!news) news = stashNewsFor(o, forPay);
    askAboutStash(o);
    if (!news) return;
    if (news->kind == Tell::Kind::ShutIn) news->route = wayToShutIn(world, *news);
    // A machine comes with the way to its front, as water does.
    if (const Machine* m = news->kind == Tell::Kind::Machine ? world.machine(news->cell) : nullptr)
        if (auto p = search(world, [&](Vec2i c) { return m->usedFrom(c); })) news->route = *p;
    news->sold = forPay;
    tell(o, *news);
}

std::optional<Human::Tell> Human::newsFor(const Human& o, bool& forPay) const {
    forPay = false;
    if (isFoe(o.id)) return std::nullopt;
    // Someone shut in, before anything: it may be their life.
    if (caresForShutIn())
        for (const auto& [who, c] : rescue_.cries)
            if (who != o.id && !o.rescue_.cries.count(who) && !o.rescue_.out.count(who)) {
                Tell t{Tell::Kind::ShutIn, c.at};
                t.who = who;
                t.gate = c.gate;
                return t;
            }
    bool friendly = isFriend(o.id);
    // The one of the cells nearest to them that passes.
    auto nearest = [&](const std::vector<Vec2i>& cells, auto keep) {
        std::optional<Vec2i> best;
        for (Vec2i c : cells) {
            if (!keep(c)) continue;
            if (!best || cheb(c, o.cell) < cheb(*best, o.cell)) best = c;
        }
        return best;
    };
    if (o.hungry() && o.cakes == 0 && (friendly || traits.greedy < STINGY)) {
        std::vector<Vec2i> cakes(memory.cakes.begin(), memory.cakes.end());
        if (auto c = nearest(cakes, [&](Vec2i c) { return !o.memory.cakes.count(c) && !memory.traps.count(c); }))
            return Tell{Tell::Kind::Cake, *c};
    }
    bool selling = traits.greedy >= CUNNING_GREEDY && !friendly;
    // A machine to buy from, for one with the keys and the need: free to
    // friends, sold by the grasping to those with a cake to spare, and kept
    // from rivals by the cunning.
    if (o.canBuy() && (!selling || o.cakes > o.spareFrom()) && !(cunning() && !friendly)) {
        std::vector<Vec2i> machines;
        for (const auto& [at, seen] : memory.machines)
            if (seen.stock > 0 && (seen.kind == Vend::Cake ? o.hungry() && o.cakes == 0 : o.needsBottle()))
                machines.push_back(at);
        if (auto c = nearest(machines, [&](Vec2i c) { return !o.memory.machines.count(c); })) {
            forPay = selling;
            return Tell{Tell::Kind::Machine, *c};
        }
    }
    if (!selling || o.cakes > o.spareFrom()) {
        std::vector<Vec2i> traps;
        for (const auto& [c, kind] : memory.traps) traps.push_back(c);
        if (auto c = nearest(traps, [&](Vec2i c) { return !o.memory.traps.count(c); })) {
            forPay = selling;
            return Tell{Tell::Kind::Trap, *c, memory.traps.at(*c)};
        }
    }
    if (o.keys == 0) {
        std::vector<Vec2i> keys(memory.keys.begin(), memory.keys.end());
        if (auto c = nearest(keys, [&](Vec2i c) { return !o.memory.keys.count(c); })) return Tell{Tell::Kind::Key, *c};
    }
    return std::nullopt;
}

bool Human::wouldAskWay(const Human& o) const {
    return outDrive() >= ASK_WAY_FROM && !isFoe(o.id) && !escape_.askedWay.count(o.id);
}

// Lies and gloom are told however unsure; an honest hunch only when it
// has one worth the name.
void Human::answerWay(Human& asker) {
    auto t = wayFor(asker);
    if (t && (t->lie || t->gloomy || escape_.hunchSure >= WAY_UNSURE))
        tell(asker, *t);
    else
        sayLater(talk::Moment::NoIdea, asker.id);
    if (wouldAskWay(asker)) {
        escape_.askedWay.insert(asker.id);
        if (rng_.chance(ASK_BACK_CHANCE)) sayLater(talk::Moment::AskWay, asker.id);
    }
}

// Settlers have seen the most, and say so.
std::optional<Human::Tell> Human::wayFor(const Human& o) const {
    bool foe = isFoe(o.id);
    if (foe && !cunning()) return std::nullopt;
    Tell t{Tell::Kind::Way, cell};
    if (foe) {
        // The other way from where it thinks out is, or at a trap.
        t.lie = true;
        t.sure = WAY_LIE_SURE;
        t.way = escape_.hunch + TAU / 2.0f;
        int best = 0;
        for (const auto& [plate, kind] : memory.traps)
            if (bait(plate) && !o.memory.traps.count(plate) && plate != o.cell &&
                (best == 0 || cheb(plate, o.cell) < best)) {
                best = cheb(plate, o.cell);
                t.cell = plate;
                t.way = std::atan2(float(plate.y - o.cell.y), float(plate.x - o.cell.x));
            }
    } else if (escape_.settled || gloom() >= WAY_GLOOMY) {
        t.gloomy = true;
        t.sure = std::clamp(float(escape_.chunksIn.size()) / WAY_SEEN_ENOUGH, escape_.hunchSure, 1.0f);
    } else {
        t.way = escape_.hunch;
        t.sure = escape_.hunchSure;
    }
    if (auto it = escape_.wayTold.find(o.id); it != escape_.wayTold.end()) {
        if (t.gloomy) {
            if (it->second < -TAU) return std::nullopt;
        } else if (it->second >= -TAU && std::cos(t.way - it->second) > std::cos(WAY_RETELL)) {
            return std::nullopt;
        }
    }
    return t;
}

// The route is told from here: to beside the plate, over it, and on to a
// door the foe has never seen, so that going for the door takes them
// across it, if they walk the way they were told.
std::optional<Human::Tell> Human::lieFor(const World& world, const Human& foe) const {
    if (!cunning() || !isFoe(foe.id)) return std::nullopt;
    // Hungry with nothing to eat: a cake, and the darts under it.
    if (foe.hungry() && foe.cakes == 0)
        if (auto c = riggedCakeFor(foe)) {
            Tell t{Tell::Kind::Cake, *c, TrapKind::Darts};
            t.lie = true;
            return t;
        }
    if (foe.keys > 0)
        if (auto t = routeLie(world, foe)) return t;
    // A stash to set it on, or one that isn't there (human_stash.cpp).
    if (auto t = stashLieFor(world, foe); t && !t->nothing) return t;
    // Nothing to bait it with: a cake or key that isn't there.
    bool forKey = !(foe.hungry() && foe.cakes == 0);
    if (forKey && foe.keys > 0) return std::nullopt;
    auto c = bareTableFor(world, foe, forKey);
    if (!c && !forKey) return stashLieFor(world, foe);
    if (!c) return std::nullopt;
    Tell t{forKey ? Tell::Kind::Key : Tell::Kind::Cake, *c};
    t.lie = t.nothing = true;
    return t;
}

std::optional<Human::Tell> Human::gainLieFor(const World& world, const Human& o, bool& forPay) const {
    forPay = false;
    if (!cunning() || traits.greedy < GAIN_LIE_GREEDY || isFriend(o.id) || isFoe(o.id) || inGroupWith(o))
        return std::nullopt;
    // A rival for a cake it wants: sent the other way.
    std::optional<Tell> lie;
    if (o.hungry() && o.cakes == 0 && cakes < carryMax()) {
        std::optional<Vec2i> want;
        for (Vec2i c : memory.cakes)
            if (!memory.traps.count(c) && cheb(c, o.cell) <= RIVAL_NEAR && (!want || cheb(c, cell) < cheb(*want, cell)))
                want = c;
        if (want) {
            std::optional<Vec2i> away;
            for (const Table& tb : world.tables()) {
                Vec2i c = tb.at;
                if (memory.tile(c) != Tile::Table || o.memory.tile(c) != Tile::Void || cheb(c, o.cell) < FALSE_TIP_FAR ||
                    memory.cakes.count(c) || memory.traps.count(c))
                    continue;
                if (!away || cheb(c, *want) > cheb(*away, *want)) away = c;
            }
            if (away && cheb(*away, *want) > cheb(o.cell, *want)) {
                lie = Tell{Tell::Kind::Cake, *away};
            }
        }
    }
    // Trap news to sell, made up when it has none.
    if (!lie && o.cakes > o.spareFrom()) {
        bool real = std::any_of(memory.traps.begin(), memory.traps.end(),
                                [&](const auto& t) { return !o.memory.traps.count(t.first); });
        std::optional<Vec2i> spot;
        for (int y = -FAKE_TRAP_FAR; y <= FAKE_TRAP_FAR && !real; y++)
            for (int x = -FAKE_TRAP_FAR; x <= FAKE_TRAP_FAR; x++) {
                Vec2i c = o.cell + Vec2i{x, y};
                // Traps hide in plain floor, so even floor o has seen will do.
                if (cheb(c, o.cell) < FAKE_TRAP_NEAR || memory.tile(c) != Tile::Floor || o.memory.traps.count(c) ||
                    memory.traps.count(c))
                    continue;
                if (!spot || cheb(c, o.cell) < cheb(*spot, o.cell)) spot = c;
            }
        if (spot) {
            lie = Tell{Tell::Kind::Trap, *spot, TrapKind::Spikes};
            forPay = true;
        }
    }
    if (!lie) {
        forPay = false;
        return std::nullopt;
    }
    lie->lie = lie->nothing = lie->gain = true;
    return lie;
}

std::optional<Human::Tell> Human::routeLie(const World& world, const Human& foe) const {
    for (const auto& [plate, kind] : memory.traps) {
        if (!bait(plate) || foe.memory.traps.count(plate)) continue;
        auto there = search(world, [&](Vec2i c) { return c != plate && cheb(c, plate) == 1; });
        if (!there) continue;
        auto on = searchFrom(world, plate, [&](Vec2i c) {
            auto d = lockedDoorBeside(c);
            return d && foe.memory.tile(*d) == Tile::Void;
        }, false);
        if (!on || on->empty()) continue;
        Tell t{Tell::Kind::Route, *lockedDoorBeside(on->back()), kind};
        t.lie = true;
        t.route.push_back(cell);
        t.route.insert(t.route.end(), there->begin(), there->end());
        t.route.push_back(plate);
        t.route.insert(t.route.end(), on->begin(), on->end());
        t.route.push_back(t.cell);
        return t;
    }
    return std::nullopt;
}

bool Human::bait(Vec2i c) const {
    auto it = memory.traps.find(c);
    if (it == memory.traps.end()) return false;
    return it->second == TrapKind::Spikes || (it->second == TrapKind::Darts && memory.tile(c) == Tile::Floor);
}

std::optional<Vec2i> Human::bareTableFor(const World& world, const Human& o, bool forKey) const {
    std::optional<Vec2i> best;
    for (const Table& tb : world.tables()) {
        Vec2i c = tb.at;
        if (memory.tile(c) != Tile::Table || o.memory.tile(c) != Tile::Void || cheb(c, o.cell) < FALSE_TIP_FAR) continue;
        if ((forKey ? memory.keys.count(c) : memory.cakes.count(c)) || memory.traps.count(c)) continue;
        if (!best || cheb(c, o.cell) < cheb(*best, o.cell)) best = c;
    }
    return best;
}

std::optional<Vec2i> Human::riggedCakeFor(const Human& o) const {
    std::optional<Vec2i> best;
    for (const auto& [c, kind] : memory.traps)
        if (kind == TrapKind::Darts && memory.tile(c) == Tile::Table && memory.cakes.count(c) &&
            !o.memory.traps.count(c) && !o.memory.cakes.count(c) && (!best || cheb(c, o.cell) < cheb(*best, o.cell)))
            best = c;
    return best;
}

bool Human::cunning() const { return traits.greedy >= CUNNING_GREEDY && traits.brave >= CUNNING_BRAVE; }

bool Human::masks(int id) const { return cunning() && isFoe(id) && fight_.foe != id && !afraidOf(id); }

// Word of people is spoken of by the first with an opinion; names alone
// pass without a line of their own.
void Human::tell(Human& o, const Tell& t) {
    using talk::Moment;
    Saying s{Moment::TellCake, o.id, -1};
    switch (t.kind) {
    case Tell::Kind::Cake: s.moment = Moment::TellCake; break;
    case Tell::Kind::Key: s.moment = Moment::TellKey; break;
    case Tell::Kind::Trap: s.moment = t.sold ? Moment::SellTrap : Moment::TellTrap; break;
    case Tell::Kind::Route: s.moment = t.lie ? Moment::Lie : Moment::TellDoor; break;
    case Tell::Kind::Way: s.moment = t.gloomy ? Moment::NoWayOut : t.lie ? Moment::LieWay : Moment::TellWay; break;
    case Tell::Kind::Water: s.moment = t.lie ? Moment::LieWater : t.sold ? Moment::SellWater : Moment::TellWater; break;
    case Tell::Kind::ShutIn:
        s.moment = Moment::TellShutIn;
        s.about = t.who;
        break;
    case Tell::Kind::Stash:
        s.moment = t.lie ? Moment::LieStash : t.sold ? Moment::SellStash : Moment::TellStash;
        s.about = t.owner;
        break;
    case Tell::Kind::Machine: s.moment = t.sold ? Moment::SellVending : Moment::TellVending; break;
    case Tell::Kind::People: {
        auto w = std::find_if(t.people.begin(), t.people.end(),
                              [&](const Tell::Word& w) { return w.opinion && w.about != o.id; });
        s.quiet = w == t.people.end();
        if (!s.quiet) {
            s.moment = *w->opinion < 0.0f ? Moment::Warn : Moment::Praise;
            s.about = w->about;
        }
        break;
    }
    }
    s.tell = t;
    sayLater(std::move(s));
}

void Human::deliver(Human& o, const Tell& t) {
    bool news = false;
    // The cells along a route and all round them, so they can cut the same
    // corners; only what they have never seen, and only tiles.
    // Every cell along it is told as clear, seen before or not, so a trap
    // that goes off on it is laid at the teller's door (learnTrap).
    auto learnRoute = [&] {
        for (Vec2i c : t.route)
            if (!o.memory.traps.count(c)) o.memory.toldBy[c] = id;
        for (Vec2i c : t.route) {
            for (Vec2i d : DIRS8)
                for (Vec2i m : {c, c + d})
                    if (Tile tile = memory.tile(m); tile != Tile::Void && o.memory.tile(m) == Tile::Void) {
                        o.memory.hear(m, tile);
                        news = true;
                    }
        }
    };
    switch (t.kind) {
    case Tell::Kind::Cake:
        news = o.memory.cakes.insert(t.cell).second;
        if (news) {
            o.memory.toldBy[t.cell] = id;
            o.memory.cakeTips[t.cell] = id;
        }
        break;
    case Tell::Kind::Key:
        news = o.memory.keys.insert(t.cell).second;
        if (news) o.memory.toldBy[t.cell] = id;
        break;
    case Tell::Kind::Trap:
        news = o.memory.traps.emplace(t.cell, t.trap).second;
        if (news) o.memory.toldBy[t.cell] = id;
        break;
    case Tell::Kind::Route:
    case Tell::Kind::Water:
        // A way to water ends beside it, so the water comes with it, and how
        // full a dispenser was when the teller saw it.
        learnRoute();
        if (auto it = memory.dispensers.find(t.cell); it != memory.dispensers.end())
            o.memory.dispensers.emplace(t.cell, it->second);
        break;
    case Tell::Kind::People:
        // Taken on as far as the listener trusts the teller, and only the
        // change since this teller last spoke of them.
        for (const Tell::Word& w : t.people) {
            if (w.about == o.id) continue;
            if (!w.name.empty() && o.memory.names.emplace(w.about, w.name).second) {
                o.note("HEARD @'S NAME FROM #", w.about, id);
                news = true;
            }
            if (!w.opinion) continue;
            float& told = o.memory.heard[Memory::gossipKey(id, w.about)];
            float delta = (*w.opinion - told) * GOSSIP_WEIGHT * std::max(0.0f, o.trust(id));
            told = *w.opinion;
            if (delta == 0.0f) continue;
            o.note(*w.opinion < 0.0f ? "# WARNED ME ABOUT @" : "# SPOKE WELL OF @", w.about, id);
            if (w.lie) note("LIED TO @ ABOUT #", o.id, w.about);
            o.regard(w.about, delta);
            news = true;
        }
        break;
    case Tell::Kind::ShutIn:
        // Kept as if heard, on their word, with the way there; weighed
        // below, once noted.
        if (t.who == o.id) break;
        learnRoute();
        news = true;
        o.hearOf(t.who, t.cell, t.gate, id);
        break;
    case Tell::Kind::Stash:
        // Only what it hasn't seen for itself; a stash that isn't there
        // looks like a few cakes from what it is told.
        if (o.memory.stashes.count(t.cell) || (o.memory.piles.count(t.cell) && t.owner < 0)) break;
        news = true;
        o.memory.stashes[t.cell] = t.owner;
        o.memory.stashTips[t.cell] = id;
        if (!o.memory.piles.count(t.cell)) {
            auto it = memory.piles.find(t.cell);
            o.memory.piles[t.cell] = it != memory.piles.end() ? it->second : Pile{FALSE_STASH_CAKES, {}};
        }
        if (o.memory.tile(t.cell) == Tile::Void) o.memory.hear(t.cell, Tile::Floor);
        break;
    case Tell::Kind::Machine: {
        // What it sells and what the teller last saw in it, with the way
        // there and the cells round it.
        auto it = memory.machines.find(t.cell);
        if (it == memory.machines.end() || o.memory.machines.count(t.cell)) break;
        learnRoute();
        news = true;
        o.memory.machines[t.cell] = it->second;
        o.memory.toldBy[t.cell] = id;
        for (Vec2i d : DIRS8)
            for (Vec2i m : {t.cell, t.cell + d})
                if (Tile tile = memory.tile(m); tile != Tile::Void && o.memory.tile(m) == Tile::Void)
                    o.memory.hear(m, tile);
        break;
    }
    case Tell::Kind::Way: {
        // Believed as far as the teller is trusted, and as sure as it sounds.
        float w = std::clamp(o.trust(id) + WAY_STRANGER, 0.0f, 1.0f);
        news = true;
        escape_.wayTold[o.id] = t.gloomy ? -2.0f * TAU : t.way;
        if (t.gloomy) {
            o.note("@ SAYS THERE IS NO WAY OUT", id);
            note("TOLD @ THERE IS NO WAY OUT", o.id);
            o.lift(-WAY_GLOOM * w * t.sure);
            break;
        }
        float pull = w * t.sure / (t.sure + o.escape_.hunchSure);
        float agree = std::cos(t.way - o.escape_.hunch);
        float x = std::cos(o.escape_.hunch) * (1.0f - pull) + std::cos(t.way) * pull;
        float y = std::sin(o.escape_.hunch) * (1.0f - pull) + std::sin(t.way) * pull;
        if (x != 0.0f || y != 0.0f) o.escape_.hunch = std::atan2(y, x);
        o.escape_.hunchSure = std::clamp(o.escape_.hunchSure + WAY_SURE * w * agree, WAY_SURE_MIN, 1.0f);
        o.lift(WAY_HOPE * w);
        // Pointed at spikes: walking into them shows it was a lie (learnTrap).
        if (t.lie && memory.traps.count(t.cell) && !o.memory.traps.count(t.cell)) o.memory.toldBy[t.cell] = id;
        o.note("@ POINTED THE WAY OUT", id);
        note(t.lie ? "POINTED @ THE WRONG WAY" : "POINTED @ THE WAY I THINK IS OUT", o.id);
        break;
    }
    }
    if (!news) return;
    o.rethink_ = true;
    o.regard(id, TRUST_TOLD);
    if (t.kind == Tell::Kind::Way) return;
    // Word of people is in the listener's log a piece at a time, above.
    static const char* const WHAT[] = {"WHERE A CAKE IS", "WHERE A KEY IS", "WHERE A TRAP IS", "THE WAY TO A DOOR",
                                       "ABOUT OTHERS", "THE WAY OUT", "THE WAY TO WATER", "SOMEONE IS SHUT IN",
                                       "WHERE A STASH IS", "WHERE A MACHINE IS"};
    static_assert(std::size(WHAT) == size_t(Tell::Kind::Machine) + 1);
    std::string what = WHAT[size_t(t.kind)];
    if (t.kind != Tell::Kind::People) o.note("@ TOLD ME " + what, id);
    std::string over = std::string(" OVER ") + trapWord(t.trap);
    if (t.kind == Tell::Kind::Stash && t.lie && !t.nothing)
        note("SET @ ON #'S STASH", o.id, t.owner);
    else if (t.kind == Tell::Kind::Stash && t.nothing)
        note("TOLD @ OF A STASH THAT ISN'T THERE", o.id);
    else
    note(!t.lie                          ? "TOLD @ " + what
         : t.gain && t.kind == Tell::Kind::Trap ? std::string("SOLD @ A TRAP THAT ISN'T THERE")
         : t.gain                        ? std::string("SENT @ AWAY FROM A CAKE I WANT")
         : t.nothing                     ? std::string("TOLD @ OF A ") + (t.kind == Tell::Kind::Key ? "KEY" : "CAKE") + " THAT ISN'T THERE"
         : t.kind == Tell::Kind::Cake    ? std::string("TOLD @ OF A RIGGED CAKE")
         : t.kind == Tell::Kind::Water   ? "TOLD @ A WAY TO WATER" + over
                                         : "TOLD @ A WAY" + over,
         o.id);
    // A tip sold is owed for as the greeting ends; said once it is over, it went for nothing.
    if (t.sold && o.state == State::Greeting && o.social_.partner == id) o.social_.owes = id;
    if (t.kind == Tell::Kind::ShutIn) o.weighRescue(t.who);
    if (t.kind != Tell::Kind::People) o.sayLater(talk::Moment::ThankTip, id);
}

void Human::checkTold(Vec2i c, bool asTold) {
    auto it = memory.toldBy.find(c);
    if (it == memory.toldBy.end()) return;
    if (!asTold) {
        note("NOTHING WAS WHERE @ SAID", it->second);
        remember(Episode::What::Lied, EPISODE_STALE_TIP, it->second, id);
        regard(it->second, TRUST_STALE);
    }
    memory.toldBy.erase(it);
}

const std::string* Human::knownName(int other) const {
    auto it = memory.names.find(other);
    return it == memory.names.end() ? nullptr : &it->second;
}

bool Human::wouldIntroduce(const Human& other, bool theyDid) const {
    if (other.knownName(id)) return false;
    if (isFriend(other.id)) return true;
    float p = INTRODUCE_BASE + INTRODUCE_SOCIABLE * traits.sociable + (theyDid ? INTRODUCE_RETURN : 0.0f);
    return rng_.chance(p);
}

void Human::introduce(Human& other) {
    Saying s{talk::Moment::Introduce, other.id, -1};
    s.intro = true;
    sayLater(std::move(s));
}

// Heard said aloud, the name may be known already.
void Human::giveName(Human& other) {
    if (other.memory.names.emplace(id, name).second) other.note("@ TOLD ME THEIR NAME", id);
    note("TOLD @ MY NAME", other.id);
}

int Human::counterpart() const {
    switch (state) {
    case State::Greeting: return social_.partner;
    case State::Punching: return fight_.foe;
    case State::Giving: return social_.sharee;
    case State::Looting: return sleep_.robbee >= 0 ? sleep_.robbee : deaths_.corpse;
    case State::Feeding: return mind_.meal;
    default: break;
    }
    switch (goal) {
    case Goal::Meet: return social_.partner;
    case Goal::Follow: return social_.leader;
    case Goal::Company: return social_.companion;
    case Goal::Fight: return fight_.foe;
    case Goal::Flee: return fight_.feared;
    case Goal::Rob: return sleep_.robbee;
    case Goal::Watch: return sleep_.watchee;
    case Goal::Share: return social_.sharee;
    case Goal::Escort: return escorting_.escortee;
    case Goal::Escorted: return escorting_.by;
    case Goal::Loot: return deaths_.corpse;
    case Goal::Feed: return mind_.meal;
    case Goal::Rescue: return rescue_.rescuee;
    case Goal::Ask: return water_.askee;
    default: return -1;
    }
}

void Human::greet(Human& other) {
    state = State::Greeting;
    animTime = 0.0f;
    gossiping = false;
    moving = false;
    path_.clear();
    social_.partner = other.id;
    bool first = met.insert(other.id).second;
    note(first ? "MET @" : "CAUGHT UP WITH @", other.id);
    sayLater(first ? talk::Moment::Meet : talk::Moment::CatchUp, other.id);
    social_.greetedAt[other.id] = clock_;
    regard(other.id, TRUST_GREETED);
    if (other.cell.x != cell.x) facingLeft = other.cell.x < cell.x;
    timer = GREET_TIME;
}

// Without a key, the human now follows a new friend who has one.
void Human::endGreet(World& world) {
    if (keys == 0 && world.human(social_.partner).keys > 0 && !isFoe(social_.partner) && !shuns(social_.partner) &&
        social_.leader != social_.partner) {
        social_.leader = social_.partner;
        note("FOLLOWING @, WHO HAS A KEY", social_.leader);
        sayLater(talk::Moment::Follow, social_.leader);
    }
    social_.partner = -1;
    // A tip bought is paid for now, if they are still there to pay.
    int seller = social_.owes;
    social_.owes = -1;
    if (seller >= 0) {
        Human& s = world.human(seller);
        // Greeted from as far as one can call out, it pays from there too.
        if (!s.dead() && cakes > 0 && cheb(cell, s.cell) <= HAIL_RADIUS) return give(s, Gift::Pay);
    }
    startIdle(0.0f, 0.0f);
}

bool Human::startFollow(World& world) {
    Human& l = world.human(social_.leader);
    bool seen = sight.sees(l.cell);
    // Dead out of sight, they are followed on until found or lost.
    if ((seen && (l.dead() || l.keys == 0)) || suspicious(l.id) || isFoe(l.id) || shuns(l.id)) {
        if (suspicious(l.id) || isFoe(l.id))
            sayLater(talk::Moment::PartTurned, l.id);
        else if (!l.dead() && !shuns(l.id))
            sayLater(talk::Moment::PartNoKeys, l.id);
        social_.leader = -1;
        return false;
    }
    Vec2i at = seen ? l.cell : social_.lastSeen[l.id];
    if (cheb(cell, at) <= FOLLOW_GAP) {
        if (!seen) {
            // Lost them.
            note("LOST @", social_.leader);
            sayLater(talk::Moment::PartLost, -1, social_.leader);
            social_.leader = -1;
            return false;
        }
        // The leader asleep, it sits down by them to wait.
        if (l.state == State::Sleeping)
            sitDown();
        else
            startIdle(FOLLOW_WAIT_MIN, FOLLOW_WAIT_MAX);
        goal = Goal::Follow;
        tellLockedDoor(world, l);
        return true;
    }
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= FOLLOW_GAP; });
    if (!p) {
        social_.leader = -1;
        return false;
    }
    startWalking(Goal::Follow, *p);
    return true;
}

// The nearest locked door it knows of and the leader has never seen, and the
// way there from here: the cells along it and all round them, so the leader
// can cut the same corners. Only cells the leader has never seen are told,
// and only tiles: nothing of keys, cakes or traps. A leader free to stop
// stops and they greet; a busy one is told on the move.
void Human::tellLockedDoor(World& world, Human& l) {
    if (hasWordFor(l.id)) return;
    std::optional<Vec2i> door;
    auto p = search(world, [&](Vec2i c) {
        if (memory.tile(c) != Tile::Floor) return false;
        for (Vec2i d : DIRS4)
            if (memory.tile(c + d) == Tile::DoorLocked && l.memory.tile(c + d) == Tile::Void) {
                door = c + d;
                return true;
            }
        return false;
    });
    if (!p) return;
    Tell t{Tell::Kind::Route, *door};
    t.route.push_back(cell);
    t.route.insert(t.route.end(), p->begin(), p->end());
    t.route.push_back(*door);
    if (l.approachable() && l.welcomes(*this)) {
        greet(l);
        l.greet(*this);
    }
    tell(l, t);
    if (wouldIntroduce(l, false)) introduce(l);
    if (auto word = gossipFor(l)) {
        tell(l, *word);
        gossiping = true;
    }
}

Human* Human::unmetInSight(const World& world) const {
    Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || !o->approachable() || !sight.sees(o->cell) || !wouldGreet(*o)) continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    return best;
}

bool Human::wouldGreet(const Human& o) const {
    if (o.dead() || suspicious(o.id)) return false;
    if (auto it = social_.snubbed.find(o.id); it != social_.snubbed.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    if (auto it = social_.busy.find(o.id); it != social_.busy.end() && clock_ - it->second < BUSY_WAIT) return false;
    if (isFoe(o.id)) return lieWorthy(o);
    if (!met.count(o.id)) return true;
    auto it = social_.greetedAt.find(o.id);
    if (it != social_.greetedAt.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    // Wanting a word with them: goes over.
    if (wantsWord(o.id)) return true;
    bool pay;
    return newsFor(o, pay).has_value() || o.newsFor(*this, pay).has_value() || gossipFor(o).has_value() ||
           o.gossipFor(*this).has_value();
}

// A foe with keys may be sent over spikes to a door; any foe not yet
// pointed the wrong way may be pointed at spikes as the way out.
bool Human::lieWorthy(const Human& o) const {
    if (!cunning() || !isFoe(o.id)) return false;
    auto it = social_.greetedAt.find(o.id);
    if (it != social_.greetedAt.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    // A false tip will nearly always do for the hungry or the keyless.
    if ((o.hungry() && o.cakes == 0) || o.keys == 0) return true;
    for (const auto& [plate, kind] : memory.traps)
        if (bait(plate) && !o.memory.traps.count(plate)) return true;
    return false;
}

bool Human::approachable() const {
    if (state == State::Idle) return !awaiting_ && goal != Goal::Escort && goal != Goal::Escorted;
    if (state != State::Walking) return false;
    switch (goal) {
    case Goal::Fight:
    case Goal::Flee:
    case Goal::Share:
    case Goal::GiveWay:
    case Goal::PassDoor:
    case Goal::Escort:
    case Goal::Escorted:
    case Goal::Avoid:
    case Goal::Loot:
    case Goal::Feed:
    case Goal::Drink:
    case Goal::Relieve:
    case Goal::Sleep:
    case Goal::Rob:
        return false;
    case Goal::FetchCake:
        return !hungry();
    default:
        return true;
    }
}

bool Human::welcomes(const Human& o) const {
    // A foe worth lying to is greeted back with a smile.
    return (!isFoe(o.id) || lieWorthy(o)) && !suspicious(o.id) && !afraidOf(o.id) && !repelledBy(o);
}
