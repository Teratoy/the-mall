#include "human/human.h"
#include "human/human_tuning.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <queue>

Human::Human(int id, const HumanLook& look, Vec2i cell, uint64_t seed, std::string chosenName)
    : id(id), name(chosenName.empty() ? randomName(seed) : std::move(chosenName)), look(look), traits(Traits::random(seed)), cell(cell), prev(cell), stains(look), dirt(look), soaked(look),
      rng_(seed), target_(cell) {
    gore_.trod = cell;
    hunger = rng_.range(0.0f, START_HUNGER_MAX);
    thirst = rng_.range(0.0f, START_THIRST_MAX);
    tiredness = rng_.range(0.0f, START_TIRED_MAX);
    timer = rng_.range(WAKE_LIE_MIN, WAKE_LIE_MAX);
    escape_.hunch = rng_.range(0.0f, TAU);
    // Rolled from the seed, as traits are, leaving its own dice as they were.
    run_.lungs = Rng(seed ^ LUNGS_SALT).range(LUNGS_MIN, LUNGS_MAX);
}

void Human::update(World& world, float dt) {
    animTime += dt;
    clock_ += dt;
    spill(world, dt);
    tread(world);
    if (dead()) {
        float lethal;
        bleedOut(dt, lethal);
        // The killing blow shows for a moment, not for good.
        hurtTime = std::max(0.0f, hurtTime - dt);
        return;
    }
    live(world, dt);
    if (dead()) return;
    // Asleep it says nothing and does nothing, until a blow wakes it, and
    // then it answers the blow like anyone.
    if (state == State::Sleeping) {
        // Nodding off it may still say something; lying down, nothing more.
        if (animTime >= LIE_DOWN_TIME) voice_.sayings.clear();
        speak(world, dt);
        if (hurtTime <= 0.0f) return sleep(world, dt);
        wakeUp(world, true, -1, "WOKE UP HURT");
    }
    speak(world, dt);
    chatUrges(world, dt);
    callForHelp(world, dt);
    recallCries();
    keepTrackOfDry(world);
    timer -= dt;
    // A hit is answered at once, or as soon as the step under way is done.
    if (fight_.provoked && sleep_.stunned <= 0.0f && (!moving || timer <= 0.0f)) {
        react(world);
        if (state == State::Walking) step(world);
        return;
    }
    if (state == State::Waking) {
        // The body before the mind: a mutter with its eyes still shut.
        if (!escape_.stirred && animTime >= WAKE_STIR_AT) {
            escape_.stirred = true;
            sayLater(talk::Moment::WakeStir);
        }
        if (timer <= 0.0f) sitUp();
        return;
    }
    if (state == State::Sitting) {
        glance_ -= dt;
        if (glance_ <= 0.0f) {
            glance_ = SIT_GLANCE;
            if (observe(world)) rethink_ = true;
            // On watch, anything that needs it up and about.
            if (goal == Goal::Watch && watchCalls(world)) rethink_ = true;
        }
        if ((rethink_ && !escape_.comingTo) || timer <= 0.0f) startRising();
        return;
    }
    if (state == State::Typing) return typing(world, dt);
    if (timer > 0.0f) return;
    switch (state) {
    case State::Greeting: {
        // Not walking off mid-sentence, while the other still stands there.
        const Human& p = world.human(social_.partner);
        bool with = !p.dead() && p.state == State::Greeting && p.social_.partner == id;
        if (with && (talking() || p.talking()) && timer > GREET_TIME - GREET_LONGEST) return;
        endGreet(world);
        return;
    }
    case State::Eating:
        endEating();
        return;
    case State::Drinking:
        return drink(world);
    case State::Sipping:
        return sip(world);
    case State::Filling:
        return fill(world);
    case State::Pouring:
        return pourOut(world);
    case State::Stashing:
        return finishStash(world);
    case State::Vending:
        return takeVended(world);
    case State::Typing:
        return;
    case State::Relieving:
        endRelieving();
        return choose(world);
    case State::Giving:
        startIdle(0.0f, 0.0f);
        return;
    case State::Washing:
        return wash(world);
    case State::Looting:
        return loot(world);
    case State::Feeding:
        return feed(world);
    case State::Punching:
        if (!punchLanded) return landPunch(world);
        choose(world);
        if (state != State::Walking) return;
        break;
    case State::Idle:
        // A threat or plea is waited out until said, and answered.
        if (awaiting_) return;
        [[fallthrough]];
    case State::Rising:
        moving = false;
        choose(world);
        if (state != State::Walking) return;
        break;
    case State::Walking:
        break;
    case State::Waking:
    case State::Sitting:
    case State::Sleeping:
    case State::Dead:
        return;
    }
    step(world);
}

Human::Action Human::action() const {
    switch (state) {
    case State::Idle:
    case State::Rising:
    case State::Dead:
        return Action::Rest;
    case State::Waking:
        return Action::Sit;
    case State::Sitting:
        return goal == Goal::Watch ? Action::Watch : Action::Sit;
    case State::Walking:
        if (!moving) return Action::Rest;
        return running() ? Action::Run : Action::Walk;
    case State::Greeting:
        return Action::Greet;
    case State::Eating:
        return Action::Eat;
    case State::Drinking:
    case State::Sipping:
        return Action::Drink;
    case State::Filling:
        return Action::Fill;
    case State::Pouring:
        return Action::Pour;
    case State::Stashing:
        return Action::Stash;
    case State::Vending:
        return Action::Vend;
    case State::Typing:
        return Action::Type;
    case State::Relieving:
        return Action::Relieve;
    case State::Punching:
        return Action::Punch;
    case State::Giving:
        return Action::Give;
    case State::Washing:
        return Action::Wash;
    case State::Looting:
        return Action::Loot;
    case State::Feeding:
        return Action::Feed;
    case State::Sleeping:
        return Action::Sleep;
    }
    return Action::Rest;
}

bool Human::hungry() const { return hunger >= HUNGRY; }

bool Human::starving() const { return hunger >= STARVING; }

bool Human::thirsty() const { return thirst >= THIRSTY; }

bool Human::parched() const { return thirst >= PARCHED; }

float Human::fleeHealth() const { return FLEE_HEALTH * sway(1.0f - traits.brave) * (1.0f + DREAD_FLEE * dread_); }

void Human::scare(float how) {
    dread_ = std::min(1.0f, dread_ + how * sway(1.0f - traits.brave));
    // A big enough fright may make it wet itself (human_relief.cpp).
    if (how >= FRIGHT_WET) relief_.frightWet = true;
}

float Human::fearTime() const { return FEAR_TIME * sway(1.0f - traits.brave); }

int Human::carryMax() const {
    return CARRY_MAX + int(std::lround((traits.greedy - 0.5f) * 2.0f + LESSON_CARRY * lessons.hoard));
}

float Human::preyHealth() const { return PREY_HEALTH + PREY_HEALTH_GREEDY * traits.greedy; }

// The generous share from two cakes, the grasping from three.
int Human::spareFrom() const { return traits.greedy < 0.67f ? 1 : 2; }

void Human::live(World& world, float dt) {
    bool wasHungry = hungry(), wasStarving = starving();
    bool wasThirsty = thirsty(), wasParched = parched();
    hunger = std::min(1.0f, hunger + HUNGER_PER_SECOND[size_t(action())] * dt);
    thirst = std::min(1.0f, thirst + THIRST_PER_SECOND[size_t(action())] * dt);
    tire(dt);
    breathe(dt);
    press(dt);
    soil(dt);
    sniff(world, dt);
    if (hunger >= 1.0f) health -= STARVE_RATE * dt;
    if (thirst >= 1.0f) {
        health -= PARCH_RATE * dt;
        water_.thirstHurt = true;
    }
    if (!hungry() && !thirsty() && wounds.empty() && mind_.sick <= 0.0f)
        health = std::min(1.0f, health + HEAL_RATE * (state == State::Sleeping ? SLEEP_HEAL : 1.0f) * dt);
    // Darts' bleeding stops short of killing, as darts do.
    float lethal, bled = bleedOut(dt, lethal);
    health -= lethal;
    if (health > DART_SURVIVE_HEALTH) health = std::max(DART_SURVIVE_HEALTH, health - (bled - lethal));
    fight_.beating = std::max(0.0f, fight_.beating - BEATING_DECAY * dt);
    hurtTime = std::max(0.0f, hurtTime - dt);
    fight_.grudge -= dt;
    sleep_.stunned = std::max(0.0f, sleep_.stunned - dt);
    fight_.fear -= dt;
    escape_.daze = std::max(0.0f, escape_.daze - dt / DAZE_TIME);
    escape_.out = std::max(OUT_REST, escape_.out - (1.0f - OUT_REST) / OUT_SETTLE * dt);
    dread_ = std::max(0.0f, dread_ - DREAD_FADE * sway(traits.brave) * (state == State::Sitting ? DREAD_SIT_FADE : 1.0f) * dt);
    float fade = TRUST_FADE * sway(traits.sociable) * dt;
    for (auto& [who, t] : social_.trust) t = t > 0.0f ? std::max(0.0f, t - fade) : std::min(0.0f, t + fade);
    std::erase_if(social_.wronged, [&](auto& w) { return (w.second.guilt -= fade * WRONGED_FADE) <= 0.0f; });
    urgeWait_ -= dt;
    // Without hope, what going without taught it sticks.
    lessons.hoard = std::max(0.0f, lessons.hoard - LESSON_FADE * (1.0f - LESSON_GLOOM * gloom()) * dt);
    lessons.keysFirst = std::max(0.0f, lessons.keysFirst - LESSON_FADE * dt);
    lessons.nearWater = std::max(0.0f, lessons.nearWater - LESSON_FADE * dt);
    lessons.safeSleep = std::max(0.0f, lessons.safeSleep - LESSON_FADE * dt);
    wary_.darts = std::max(0.0f, wary_.darts - WARY_DARTS_FADE * dt);
    wary_.slam = std::max(0.0f, wary_.slam - LESSON_FADE * dt);
    mind(dt);
    brood(world);
    if (keys > 0) deadEnd_ = false;
    if (health <= 0.0f) {
        note(lethal > 0.0f ? "BLED TO DEATH" : thirst >= 1.0f && hunger < 1.0f ? "DIED OF THIRST" : "STARVED TO DEATH");
        // The one whose wound bleeds worst killed it, as surely as a last blow.
        if (lethal > 0.0f) {
            const Wound* worst = nullptr;
            for (const Wound& w : wounds)
                if (w.lethal && (!worst || w.bleed > worst->bleed)) worst = &w;
            if (worst && worst->by >= 0 && worst->by != id) creditKill(world, world.human(worst->by), false);
        }
        return die(world);
    }
    if ((hungry() && !wasHungry) || (thirsty() && !wasThirsty)) rethink_ = true;
    if (thirsty() && !wasThirsty) gotThirsty();
    if (parched() && !wasParched) {
        note("PARCHED");
        sayLater(talk::Moment::Parched);
        remember(Episode::What::Parched, EPISODE_NEED, id);
        lift(HOPE_PARCHED);
        gotParched(world);
    }
    if (starving() && !wasStarving) {
        note("STARVING");
        remember(Episode::What::Starving, EPISODE_NEED, id);
        lift(HOPE_STARVING);
        if (cakes == 0) learn(lessons.hoard, true);
    }
}

// A hard lesson it still holds is learnt again, or for good when this
// fills it (from short of all but full); only one all but forgotten is
// learnt anew, with its own line.
void Human::learn(float& lesson, bool hard) {
    float was = lesson;
    lesson = std::min(1.0f, lesson + (hard ? LESSON_HARD : LESSON_PAID));
    if (!hard) return;
    const char* what = &lesson == &lessons.safeSleep ? "FIND A SAFE PLACE TO SLEEP IN TIME"
                       : &lesson == &lessons.nearWater ? "KEEP NEAR WATER"
                       : &lesson == &lessons.hoard     ? "KEEP MORE CAKES"
                                                       : "TAKE KEYS FIRST";
    if (was >= LESSON_HELD) {
        bool full = lesson >= 1.0f && was < 1.0f - LESSON_HELD;
        note(std::string(full ? "LEARNT FOR GOOD TO " : "LEARNT AGAIN TO ") + what);
        sayLater(full ? talk::Moment::LearnMax : talk::Moment::LearnAgain);
        return;
    }
    note(std::string("LEARNT TO ") + what);
    if (&lesson == &lessons.nearWater) sayLater(talk::Moment::LearnWater);
    else if (&lesson == &lessons.hoard) sayLater(talk::Moment::LearnCakes);
    else if (&lesson == &lessons.keysFirst) sayLater(talk::Moment::LearnKeys);
}

float Human::bleeding() const {
    float b = 0.0f;
    for (const Wound& w : wounds) b += w.bleed;
    return b;
}

// Bleeding fades in step with healing, to nothing once healed.
float Human::bleedOut(float dt, float& lethal) {
    float rate = !dead() && (starving() || parched()) ? STARVE_HEAL_MUL : state == State::Sleeping ? SLEEP_HEAL : 1.0f;
    float bled = 0.0f;
    lethal = 0.0f;
    for (Wound& w : wounds) {
        float t = std::min(dt, w.healLeft / rate);
        float after = w.bleed * (1.0f - rate * t / w.healLeft);
        float lost = (w.bleed + after) / 2.0f * t;
        bled += lost;
        if (w.lethal) lethal += lost;
        w.bleed = after;
        w.healLeft -= rate * t;
    }
    std::erase_if(wounds, [](const Wound& w) { return w.healed(); });
    return bled;
}

Wound& Human::wound(WoundKind kind, BodyPart part, float damage, bool lethal, int by) {
    auto it = std::find_if(wounds.begin(), wounds.end(), [&](const Wound& w) {
        return w.kind == kind && w.part == part && w.lethal == lethal;
    });
    if (it == wounds.end()) {
        wounds.push_back({kind, part, 0.0f, 0.0f, 0.0f, lethal});
        it = wounds.end() - 1;
    }
    it->damage += damage;
    it->bleed += BLEED_PER_DAMAGE[size_t(kind)] * damage;
    it->healLeft += HEAL_PER_DAMAGE * damage;
    if (by >= 0) it->by = by;
    return *it;
}

float Human::stepProgress() const {
    if (!moving || stepTime <= 0.0f) return 1.0f;
    float t = 1.0f - timer / stepTime;
    return t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
}

bool Human::observe(const World& world) {
    sight.look(world, cell, int(std::lround(DAZE_SIGHT + (Sight::RADIUS - DAZE_SIGHT) * (1.0f - escape_.daze))));
    bool news = false;
    // The first key, cake, door, water, human, someone else's footprints and
    // toilet it ever sees are remarked on, one a look so they don't all come out
    // at once, and not before it has come to. Free to, it goes and looks at
    // all but the human first, and remarks on it there (human_inspect.cpp).
    bool remarked = state == State::Waking;
    std::vector<Vec2i> checks;
    auto firstSight = [&](talk::Moment m, Vec2i at, int about = -1) {
        unsigned bit = 1u << (int(m) - int(talk::Moment::FirstKey));
        if (firstSights_ & bit) return;
        if (m != talk::Moment::FirstHuman && discover({m, at, 1, about})) {
            firstSights_ |= bit;
            return;
        }
        if (remarked) return;
        remarked = true;
        if (sayLater(m, -1, about)) firstSights_ |= bit;
    };
    // Still dazed, the room it woke in is remarked on too, a thing at a time,
    // to fill a silence while it is alone: anyone about has its attention.
    bool alone = std::none_of(world.humans().begin(), world.humans().end(), [&](const auto& o) {
        return o.get() != this && !o->dead() && sight.sees(o->cell);
    });
    bool foggy = state != State::Waking && escape_.daze >= DAZE_REMARK_MIN &&
                 world.time() - escape_.fogAt >= DAZE_REMARK_GAP && voice_.sayings.empty() && alone;
    auto fogSight = [&](talk::Moment m, std::optional<Topic> topic = std::nullopt) {
        unsigned bit = 1u << (int(m) - int(talk::Moment::FirstStatue));
        if (remarked || !foggy || (escape_.fogSights & bit)) return false;
        Saying s(m);
        s.topic = std::move(topic);
        if (!sayLater(std::move(s))) return false;
        remarked = true;
        escape_.fogSights |= bit;
        escape_.fogAt = world.time();
        return true;
    };
    sight.forEach([&](Vec2i c) {
        Tile t = world.tile(c);
        if (t == Tile::DoorLocked || t == Tile::DoorOpen) firstSight(talk::Moment::FirstDoor, c);
        if (drinkable(t)) {
            firstSight(talk::Moment::FirstWater, c);
            seeWater(world, c);
        }
        if (const Toilet* toilet = world.toilet(c)) {
            firstSight(talk::Moment::FirstToilet, c);
            seeToilet(*toilet, c);
        }
        if (const Statue* st = world.statue(c)) {
            // The statue as a thing in sight, so the line can name it.
            Topic t{Topic::Kind::Here};
            t.cell = st->at;
            t.thing = Topic::Thing::Statue;
            t.detail = int(st->kind) * 16 + int(st->material);
            // The first statue, dazed or not, is worth going up to.
            unsigned bit = 1u << (int(talk::Moment::FirstStatue) - int(talk::Moment::FirstStatue));
            if (!(escape_.fogSights & bit) && discover({talk::Moment::FirstStatue, c, 1, -1, t}))
                escape_.fogSights |= bit;
            else
                fogSight(talk::Moment::FirstStatue, t);
        }
        if (world.table(c)) fogSight(talk::Moment::FirstTable);
        if (const Machine* m = world.machine(c)) news |= seeMachine(*m, c);
        if (const Dispenser* d = world.dispenser(c)) news |= seeDispenser(*d);
        if (const AirlockDoor* a = world.airlock(c)) memory.airlocks[c] = a->in;
        if (t == Tile::Rack) seeServerRoom(world, c);
        if (const ServerPiece* p = world.piece(c); p && p->kind != Rig::Rack) seeComputer(*p);
        if (t != Tile::Void) {
            if (memory.tile(c) == Tile::Void) {
                escape_.lastFound = clock_;
                // Water first seen is news to the thirsty not yet going to some.
                news |= drinkable(t) && thirsty() && goal != Goal::Drink;
                if (drinkable(t) && thirsty()) foundWater();
            }
            memory.see(c, t);
        }
        // Fresh feces of someone else's, first seen, sicken the modest a little.
        if (const std::vector<int>* m = world.mess().at(c))
            for (int i : *m) {
                const Dropping& d = world.mess().all()[size_t(i)];
                if (d.kind == MessKind::Print && d.who != id) firstSight(talk::Moment::FirstPrints, c, d.who);
                if (memory.mess.insert(i).second && !isErratic() && d.who != id)
                    scare(DISGUST_DREAD * modesty() * Mess::disgust(d, world.time()));
            }
        if (const Stain* b = world.blood().at(c)) {
            memory.blood[c] = *b;
            news |= noticeBlood(world, c);
        }
        // What lies put down is in plain sight, bottles see-through. Its own
        // stash it checks once it has looked round (who was beside it).
        if (isMine(c)) checks.push_back(c);
        if (const Pile* p = world.pile(c)) {
            memory.stashTips.erase(c);
            bool fresh = !memory.piles.count(c);
            memory.piles[c] = *p;
            spotBottle(c, *p);
            news |= fresh && !othersStash(c) && ((hungry() && p->cakes > 0) || (thirsty() && p->water() > 0.0f));
            if (othersStash(c)) resent(c, *p, memory.stashes.at(c));
        } else {
            // A stash it was told of that isn't there.
            if (auto it = memory.stashTips.find(c); it != memory.stashTips.end()) {
                note("NOTHING WAS WHERE @ SAID", it->second);
                regard(it->second, TRUST_STALE);
                memory.stashTips.erase(it);
            }
            memory.piles.erase(c);
            memory.stashes.erase(c);
            spotBottle(c, Pile{});
        }
        if (world.hasKey(c)) {
            firstSight(talk::Moment::FirstKey, c);
            news |= memory.keys.insert(c).second;
            checkTold(c, true);
        } else if (memory.keys.erase(c)) {
            checkTold(c, false);
        }
        if (world.hasCake(c)) {
            firstSight(talk::Moment::FirstCake, c);
            news |= memory.cakes.insert(c).second && (hungry() || cakes < carryMax());
            checkTold(c, true);
        } else if (memory.cakes.erase(c)) {
            checkTold(c, false);
        }
        // Raised spikes and pressed plates give themselves away.
        if (const Trap* t = world.trap(c); t && !t->armed()) news |= memory.traps.emplace(c, t->kind).second;
    });
    // Cakes carried are in plain sight, and so is how starved and hurt someone looks.
    inSight_.clear();
    // Who it sees beside its stashes, and the bottle it found, to blame if
    // anything goes missing.
    if (!bottles.empty()) water_.found.reset();
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && sight.sees(o->cell)) {
            for (Vec2i s : stash_.mine)
                if (cheb(o->cell, s) <= 1) stash_.nearBy[s] = o->id;
            if (water_.found && cheb(o->cell, *water_.found) <= 1) water_.foundNear = o->id;
        }
    for (const auto& o : world.humans()) {
        if (o.get() == this || !sight.sees(o->cell)) continue;
        if (!o->dead()) inSight_.push_back(o->id);
        if (!o->dead()) firstSight(talk::Moment::FirstHuman, o->cell, o->id);
        if (!o->dead()) noticeFilth(*o);
        if (o->state == State::Typing) watchTyping(*o);
        // Died asleep, from farther off it still looks asleep.
        if (o->dead() && o->sleep_.diedAsleep && !deaths_.seenDead.count(o->id) && cheb(cell, o->cell) > STILL_SEEN) continue;
        if (o->dead()) {
            social_.seenCakes.erase(o->id);
            social_.seenWater.erase(o->id);
            Body& b = deaths_.bodies[o->id];
            b.cell = o->cell;
            b.cakes = o->cakes;
            b.bottles = int(o->bottles.size());
            b.fullest = o->fullestBottle();
            b.meals = o->meals();
            b.spoiled = o->spoiled();
            seeRemains(*o);
            if (deaths_.seenDead.insert(o->id).second) {
                // Bare bones could be anyone's: not even a friend's are known.
                bool known = !o->bones() || o->deaths_.killer == id;
                if (!known) deaths_.nameless.insert(o->id);
                bool friendDead = known && isFriend(o->id);
                scare(o->bones() ? DREAD_BONES : o->spoiled() ? DREAD_CORPSE : DREAD_CORPSE_FRESH);
                lift(friendDead ? HOPE_FRIEND_DEAD : o->bones() ? HOPE_BONES : HOPE_BODY);
                if (o->deaths_.killer == id && !o->deaths_.killerSaw) {
                    note("FOUND @ DEAD. MY BEATING DID IT", o->id);
                    deaths_.kills++;
                    rememberAt(o->cell, Episode::What::Kill, EPISODE_KILLED, id, o->id);
                } else if (o->deaths_.killer != id) {
                    if (known) {
                        note(o->sleep_.diedAsleep ? "FOUND @ DEAD. I THOUGHT THEY WERE ASLEEP" : "FOUND @ DEAD", o->id);
                        // Taken for asleep, it tries to wake them first.
                        if (o->sleep_.diedAsleep) sayLater(talk::Moment::WakeThem, -1, o->id);
                    } else {
                        note("FOUND SOMEONE'S BONES");
                    }
                    rememberAt(o->cell, Episode::What::Body, friendDead ? EPISODE_FRIEND_BODY : EPISODE_BODY, -1,
                               known ? o->id : -1);
                    // The first body it ever finds it looks at before anything.
                    bool firstBody = !inspect_.body;
                    inspect_.body = true;
                    if (!firstBody || !discover({talk::Moment::FoundBody, o->cell, 1, o->id, std::nullopt, true}))
                        sayLater(talk::Moment::FoundBody, -1, o->id);
                    if (!o->bones() && !o->spoiled()) foundFresh(*o);
                }
                news = true;
            }
            continue;
        }
        social_.lastSeen[o->id] = o->cell;
        bool knewCakes = social_.seenCakes.count(o->id) > 0;
        if (o->cakes > 0)
            social_.seenCakes[o->id] = o->cakes;
        else
            social_.seenCakes.erase(o->id);
        news |= !met.count(o->id);
        news |= hungry() && cakes == 0 && o->cakes > 0 && !knewCakes;
        // Bottles are see-through: the water in them is in plain sight.
        bool knewWater = social_.seenWater.count(o->id) > 0;
        if (o->bottleWater() > 0.0f)
            social_.seenWater[o->id] = o->bottleWater();
        else
            social_.seenWater.erase(o->id);
        news |= thirsty() && sippable() < 0 && o->bottleWater() > 0.0f && !knewWater;
        news |= o->starving() && cakes > 1 && !starving();
        news |= judge(*o);
    }
    for (Vec2i c : checks) checkStash(c, world.pile(c));
    // Up and still where it woke, it reaches back for anything from before.
    if (!escape_.comingTo && escape_.daze <= BLANK_DAZE && escape_.chunksIn.size() <= 1 &&
        fogSight(talk::Moment::Blank))
        note("TRIED TO REMEMBER ANYTHING FROM BEFORE. NOTHING");
    // Looked round long enough to be sure: nobody, anywhere it can see.
    if (social_.lastSeen.empty() && !escape_.comingTo && escape_.daze <= EMPTY_ROOM_DAZE) fogSight(talk::Moment::EmptyRoom);
    hearQuiet(alone);
    callOut(world, alone);
    return news;
}

// Reflexes come first, in a fixed order: fighting back whoever hit it,
// eating when hungry, a starving human to feed or beat up, the cake it is
// fighting for, keeping away from blood and suspects, and an escort under
// way, begging a cake off someone close when hungry, and a settler driving
// an intruder off its place. Then the wants,
// each scored by WANTING from the traits and hunger,
// tried from the most wanted down until one can be started. With nothing
// left, it rests.
void Human::choose(World& world) {
    rethink_ = false;
    sleep_.robbee = -1;
    if (goal != Goal::OpenDoor) urged_ = false;
    takeInReach(world);
    if (dead()) return;
    if (fight_.foe >= 0 && fight_.grudge > 0.0f && startFight(world)) return;
    // Worn out: drops asleep where it stands, once the fight is over.
    if (tiredness >= 1.0f) return startSleeping(world, true);
    settleFinds();
    // Killed to eat: eats.
    if (mind_.prey >= 0 && fight_.foe != mind_.prey) {
        if (world.human(mind_.prey).dead() && startFeed(world, mind_.prey)) return;
        mind_.prey = -1;
    }
    // Can't hold on: the erratic go where they stand, the rest have an
    // accident and carry on.
    if (bursting()) {
        if (isErratic()) return startRelieving(world);
        accident(world, false);
    }
    // Not by a smell, unless starving: it walks off to eat first.
    if (hungry() && cakes > 0) {
        if (!starving() && stinksHere() && leaveStench(world)) return;
        return startEating();
    }
    if (thirsty())
        if (auto w = waterBeside(cell); w && wouldDrink(world, cell, *w)) return startDrinking(*w);
    // Beside water it would drink with a bottle to fill, it fills it.
    if (!thirsty() && !bottles.empty())
        if (auto w = waterBeside(cell); w && wantsFill(world, *w) && startFilling(world, *w)) return;
    // What lies put down within reach, if it wants any of it.
    if (takeFromPiles(world)) return;
    if (thirsty()) askGroupForWater(world);
    if (askForDrink(world)) return;
    if (helpStarving(world)) return;
    if (fight_.foe >= 0 && startFight(world)) return;
    // Erratic and parched, any bottle it sees is worth a fight (human_bottle.cpp).
    if (bottleRage(world)) return;
    // A body or blood first found it stares at, before backing off from it.
    if (inspectNext(world, true)) return;
    if (keepAway(world)) return;
    if (leadEscort(world) || followEscort(world)) return;
    if (plead(world)) return;
    if (guardHome(world)) return;
    if (givePrivacy(world)) return;
    if (keepWatch(world)) return;
    if (!hungry() && !thirsty() && !needsToGo() && waitForCompany(world)) return;
    // Something seen for the first time it goes and looks at.
    if (inspectNext(world, false)) return;

    // Someone new to greet, unless hungry or thirsty, when nobody is.
    bool needy = hungry() || thirsty() || needsToGo();
    if (needy) {
        social_.partner = -1;
    } else if (Human* o = unmetInSight(world)) {
        if (o->id != social_.partner) social_.partnerSince = clock_;
        social_.partner = o->id;
    } else if (goal != Goal::Meet || (social_.greetedAt.count(social_.partner) && social_.greetedAt[social_.partner] >= social_.partnerSince)) {
        social_.partner = -1;
    }

    struct Wish {
        Want want;
        float score;
    };
    std::vector<Wish> wishes;
    auto want = [&](Want w, float extra = 0.0f) {
        const Wanting& k = WANTING[size_t(w)];
        wishes.push_back({w, k.base + k.brave * traits.brave + k.greedy * traits.greedy + k.curious * traits.curious +
                                 k.sociable * traits.sociable + extra});
    };
    float drive = std::max(0.0f, (hunger - HUNGRY) / (1.0f - HUNGRY));
    if (hungry()) {
        want(Want::Food, drive);
        want(Want::Track, drive);
    }
    if (thirsty()) want(Want::Drink, std::max(0.0f, (thirst - THIRSTY) / (1.0f - THIRSTY)));
    // Tired, it wants to sleep; drowsy far from where it means to, to go
    // back there first.
    if (tired() || sleepsWithGroup(world))
        want(Want::Sleep, tiredDrive());
    else if (humStay())
        want(Want::Sleep);
    else if (float back = headBack(); back > 0.0f)
        want(Want::Sleep, back - WANTING[size_t(Want::Sleep)].base);
    // Up to RELIEF_DRIVE more as it nears bursting.
    if (needsToGo())
        want(Want::Relieve, RELIEF_DRIVE * (std::max(bladder, bowels) - needBar()) / (1.0f - needBar()));
    if (!needy) {
        // Guilt keeps it to itself.
        want(Want::Escort, -GUILT_ALONE * mind_.guilt + parchedToLead(world));
        want(Want::Wash, wantsWashing() ? FILTH_WASH * (0.5f + modesty()) : wantsScrub() ? GRIME_WASH * modesty() : 0.0f);
        if (float fill = std::max(fillDrive(), refillDrive()); fill >= 0.0f) want(Want::Fill, fill);
        // The cunning go out of their way to lie to a foe.
        if (social_.partner >= 0)
            want(Want::Meet, OUT_ASK * outDrive() - GUILT_ALONE * mind_.guilt + (isFoe(social_.partner) ? LIE_URGE : 0.0f));
    }
    if (hunger >= fleshBar()) want(Want::Flesh, drive + mind_.appetite);
    if (hunger >= huntBar()) want(Want::Hunt, drive + mind_.appetite);
    if (rescue_.rescuee >= 0) want(Want::Rescue);
    // A voice from out of sight may be someone who knows the way out: as
    // much a way to look as any, for the curious and the sociable more.
    if (!needy && voiceToGo()) want(Want::Voice, OUT_EXPLORE * outDrive());
    if (keys > 0 && (urged_ || doorUrge())) want(Want::UrgedDoor);
    if (traits.brave >= TIMID &&
        std::any_of(deaths_.bodies.begin(), deaths_.bodies.end(), [&](const auto& b) { return worthLooting(b.first); }))
        want(Want::Loot, -DREAD_LOOT * dread_);
    if (std::any_of(world.humans().begin(), world.humans().end(), [&](const auto& o) { return wouldRob(world, *o); }))
        want(Want::Rob, ROB_TEMPT + ROB_HUNGRY * drive);
    bool fetchCakes = cakes < carryMax() && (!memory.cakes.empty() || hoardBuys() ||
                                             std::any_of(memory.piles.begin(), memory.piles.end(),
                                                         [&](const auto& p) { return p.second.cakes > 0 && !isMine(p.first); }));
    // A hoarder with a stash still to fill fetches the harder for it, and
    // minded to let someone out, it wants a key more.
    float stocking = stashTarget() > 0 && nearStash() ? hoardDrive() : 0.0f;
    if (fetchCakes || !memory.keys.empty())
        want(Want::Fetch, LESSON_FETCH * std::max(fetchCakes ? lessons.hoard : 0.0f,
                                                  memory.keys.empty() ? 0.0f : lessons.keysFirst) +
                              (fetchCakes ? stocking : 0.0f) + (memory.keys.empty() ? 0.0f : keyForCries()));
    if (!needy && wantsToStash()) want(Want::Stash, hoardDrive());
    // Settled, it has stopped looking for a way out, and follows nobody away.
    if (!escape_.settled) {
        if (social_.leader >= 0) want(Want::Follow, -GUILT_ALONE * mind_.guilt);
        // A group goes looking too, so keeping with one shares in wanting
        // out; a friend there pulls harder.
        else if (social_.companion >= 0 || pickCompany(world) >= 0)
            want(Want::Company, COMPANY_OUT * outDrive() - GUILT_ALONE * mind_.guilt +
                                    (social_.companion >= 0 && isFriend(social_.companion) ? COMPANY_FRIEND : 0.0f));
        // Keeping company, it leaves the looking to whoever leads.
        want(Want::Explore, OUT_EXPLORE * outDrive() - DREAD_EXPLORE * dread_ -
                                (social_.companion >= 0 && social_.leader < 0 ? COMPANY_STAY : 0.0f) - waterTether() - sleepTether());
        if (keys > 0) want(Want::OpenDoor, OUT_EXPLORE * outDrive() - sleepTether());
        if (rng_.chance(CHECK_DOOR_CHANCE * sway(traits.curious, CHECK_DOOR_CURIOUS))) want(Want::CheckDoor);
    }
    want(Want::Wander, DREAD_WANDER * dread_ + DESPAIR_WANDER * gloom());
    std::stable_sort(wishes.begin(), wishes.end(), [](const Wish& a, const Wish& b) { return a.score > b.score; });
    for (const Wish& w : wishes)
        if (pursue(world, w.want)) return;
    rest();
}

bool Human::pursue(World& world, Want w) {
    switch (w) {
    case Want::Food:
        return seekFood(world);
    case Want::Drink:
        return seekWater(world) || buyBottle(world);
    case Want::Relieve:
        return seekRelief(world);
    case Want::Sleep:
        if (!tired() && humStay()) {
            if (!server_.staying) {
                server_.staying = true;
                note("SLEEPING HERE RATHER THAN PAY TO COME BACK");
                sayLater(talk::Moment::StayHum);
            }
            return seekSleep(world);
        }
        return tired() || sleepsWithGroup(world) ? seekSleep(world) : returnToSleepPlace(world);
    case Want::Flesh:
        return startFeed(world);
    case Want::Hunt:
        return startHunt(world);
    case Want::Track:
        return track(world);
    case Want::Escort:
        return offerEscort(world) || offerDrinkEscort(world);
    case Want::Wash:
        return startWash(world);
    case Want::Fill:
        return seekRefill(world) || seekFill(world) || buyBottle(world);
    case Want::Stash:
        return seekStash(world);
    case Want::Meet:
        return social_.partner >= 0 && startMeet(world);
    case Want::Rescue:
        return startRescue(world);
    case Want::Voice:
        return startVoice(world);
    case Want::UrgedDoor: {
        // Frightened, never a door that lies toward whoever scared it.
        auto it = social_.lastSeen.find(fight_.feared);
        bool afraid = fight_.fear > 0.0f && it != social_.lastSeen.end();
        std::optional<Vec2i> spared;
        auto p = search(world, [&](Vec2i c) {
            auto d = doorWorthKey(c, spared);
            return d && !(afraid && cheb(*d, it->second) < cheb(cell, it->second));
        }, afraid, [&](Vec2i c) { return hunchCost(c, HUNCH_DOOR_COST); });
        spareKeys(spared);
        if (p) {
            if (escape_.outUrged && !escape_.urgedNoted) note("TRYING A DOOR. THERE HAS TO BE A WAY OUT");
            escape_.urgedNoted = escape_.outUrged;
            startWalking(Goal::OpenDoor, *p);
            return true;
        }
        urged_ = false;
        return false;
    }
    case Want::Loot:
        return startLoot(world);
    case Want::Rob:
        return startRob(world);
    case Want::Fetch: {
        // For someone shut in, a key before any cake.
        bool forCries = keyForCries() > 0.0f;
        bool wantCakes = cakes < carryMax() && !memory.cakes.empty();
        Shy shied = Shy::None;
        std::optional<std::vector<Vec2i>> p;
        if (forCries) p = search(world, [&](Vec2i c) { return inReach(c, memory.keys).has_value(); });
        if (!p)
            p = search(world, [&](Vec2i c) {
                return inReach(c, memory.keys) || (wantCakes && cakeInReach(c, &shied));
            });
        noteShy(shied);
        // Cakes lying put down do as well, when nearer, unless it is after a key
        // for someone shut in.
        if (auto q = cakes < carryMax() && !(forCries && p) ? pilePath(world, true, false) : std::nullopt;
            q && (!p || q->size() < p->size())) {
            if (q->empty()) return takeFromPiles(world);
            startWalking(Goal::Pick, *q);
            return true;
        }
        // Nothing to fetch: the greedy buy cakes to hoard.
        if (!p && hoardBuys())
            if (auto v = machinePath(world, Vend::Cake)) return goBuy(world, *v, Vend::Cake);
        if (!p) return false;
        if (!inReach(p->empty() ? cell : p->back(), memory.keys)) {
            startWalking(Goal::FetchCake, *p);
            return true;
        }
        if (forCries && !rescue_.keyNoted) {
            rescue_.keyNoted = true;
            note("GOING FOR A KEY TO LET SOMEONE OUT");
            sayLater(talk::Moment::KeyForShutIn);
        }
        if (social_.leader >= 0) sayLater(talk::Moment::PartOwnKey, social_.leader);
        social_.leader = -1;
        startWalking(Goal::FetchKey, *p);
        return true;
    }
    case Want::Follow:
        return social_.leader >= 0 && startFollow(world);
    case Want::Company:
        return keepCompany(world);
    case Want::Explore:
        if (auto p = search(world, [&](Vec2i c) {
                return (c != cell && isFrontier(c)) || openDoorBeside(c).has_value();
            }, false, [&](Vec2i c) { return hunchCost(c, openDoorBeside(c) ? HUNCH_DOOR_COST : HUNCH_COST); })) {
            startWalking(Goal::Explore, *p);
            return true;
        }
        // Nowhere left to go but through a locked door, and no key for it.
        if (keys == 0 && !deadEnd_ && search(world, [&](Vec2i c) { return lockedDoorBeside(c).has_value(); })) {
            deadEnd_ = true;
            learn(lessons.keysFirst, true);
            lift(HOPE_DEAD_END);
        }
        return false;
    case Want::OpenDoor: {
        if (keys <= 0) return false;
        std::optional<Vec2i> spared;
        auto p = search(world, [&](Vec2i c) { return doorWorthKey(c, spared).has_value(); }, false,
                        [&](Vec2i c) { return hunchCost(c, HUNCH_DOOR_COST); });
        spareKeys(spared);
        if (!p) return false;
        startWalking(Goal::OpenDoor, *p);
        return true;
    }
    case Want::CheckDoor:
        if (auto p = search(world, [&](Vec2i c) { return doorDue(c); })) {
            startWalking(Goal::CheckDoor, *p);
            return true;
        }
        return false;
    case Want::Wander: {
        // Parched with nothing left to try, it waits by a door instead (human_thirst.cpp).
        if (languish(world)) return true;
        int radius = std::max(2, int(WANDER_RADIUS * sway(traits.curious) * (1.0f - DREAD_SHORT * dread_) *
                                     (1.0f - DESPAIR_SHORT * gloom())));
        // Settled, it potters about home, and so comes back to it; taught
        // to keep near water, about that water (human_thirst.cpp).
        Vec2i from = strollFrom();
        for (int i = 0; i < 30; i++) {
            Vec2i t = from + Vec2i{rng_.range(-radius, radius), rng_.range(-radius, radius)};
            if (t == cell || memory.tile(t) != Tile::Floor || avoids(t)) continue;
            if (auto p = search(world, [&](Vec2i c) { return c == t; })) {
                startWalking(Goal::Wander, *p);
                return true;
            }
        }
        return false;
    }
    case Want::Count:
        break;
    }
    return false;
}

// Fades as fight_.fear runs out.
float Human::fright() const {
    return std::max(dread_, fight_.feared >= 0 ? std::clamp(fight_.fear / fearTime(), 0.0f, 1.0f) : 0.0f);
}

// Past a locked door lies ground nobody has picked over yet, a door
// between it and whatever scared it, and maybe the way out. Weighed again
// every DOOR_URGE_EVERY seconds, likelier the hungrier or thirstier, the more
// frightened, and the more it wants out.
bool Human::doorUrge() {
    if (urgeWait_ > 0.0f) return false;
    // The timid are keener to put a door between.
    float p = DOOR_URGE_FEAR * fright() * sway(1.0f - traits.brave);
    if (hungry()) p = std::max(p, std::clamp((hunger - HUNGRY) / (STARVING - HUNGRY), DOOR_URGE_HUNGRY, 1.0f));
    // Thirsty, the same: with water known, drinking outweighs the door.
    if (thirsty())
        p = std::max(p, std::clamp((thirst - THIRSTY) / (PARCHED - THIRSTY), DOOR_URGE_HUNGRY, 1.0f));
    float out = DOOR_URGE_OUT * sway(traits.curious) * std::max(0.0f, (outDrive() - OUT_URGE_FROM) / (1.0f - OUT_URGE_FROM));
    if (out > p) escape_.outUrged = true, p = out;
    else escape_.outUrged = false;
    if (p <= 0.0f) return false;
    urgeWait_ = DOOR_URGE_EVERY;
    return urged_ = rng_.chance(p);
}

// The body stays where it fell, out of the way, with all it carried on it.
void Human::die(World& world) {
    world.noise(NoiseKind::Death, cell, id);
    for (Wound& w : wounds) w.healLeft = std::min(w.healLeft, CORPSE_BLEED_TIME);
    sleep_.diedAsleep = state == State::Sleeping;
    state = State::Dead;
    moving = false;
    fight_.provoked = false;
    fight_.foe = -1;
    mind_.prey = -1;
    mind_.meal = -1;
    flesh = FLESH_MEALS;
    deaths_.diedAt = clock_;
    path_.clear();
    animTime = 0.0f;
    voice_.speech.clear();
    voice_.sayings.clear();
    letGo(world);
    world.vacate(cell);
}
