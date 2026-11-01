#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>

// Going: what a human drinks fills its bladder and what it eats its bowels,
// not at once but over the next minutes, and both a little by the hour. Needing to go, it looks for a spot near by
// that feels out of the way, a dead end, a corner, behind something, where
// nobody it sees could see it, never walking far for it, and goes there:
// standing at a wall to piss, squatting for feces. Shame keeps it from
// going where it's watched, until it can hold on no longer; the erratic
// have none. What it leaves stays where it lies (World::mess()).
//
// Fresh feces stepped in dirty the feet, which leave brown prints for some
// steps after and stay dirty until washed. Everyone but the erratic finds
// it disgusting: walks round what it knows of, is put off by the sight of
// it, and by humans with it on their feet.
//
// Those who walk together give each other privacy: they don't count as
// watching, and stand back looking away while one of them goes. Unable to
// hold on, or frightened out of it, a human goes in its clothes; dead, what
// it held lets go. Being seen going shames the modest, and watchers think
// less of whoever goes in the open, far less of whoever goes in the water,
// and the cruel make fun of an accident.

namespace {

/// Filling: per gulp of water, per cake (flesh as a cake), and by the
/// second whatever it takes in.
constexpr float BLADDER_PER_GULP = 0.18f;
constexpr float BOWELS_PER_CAKE = 0.2f;
constexpr float BLADDER_CREEP = 1.0f / 1800.0f;
constexpr float BOWELS_CREEP = 1.0f / 3600.0f;
/// What goes in reaches the bladder or bowels by e every this many seconds:
/// most of a drink within about four minutes, of a meal within ten.
constexpr float WATER_PASSES = 90.0f;
constexpr float FOOD_PASSES = 240.0f;
/// Wants to go from RELIEF_NEED, up to RELIEF_HOLD more for the most
/// modest; at 1 it goes where it stands, watched or not.
constexpr float RELIEF_NEED = 0.6f;
constexpr float RELIEF_HOLD = 0.15f;
/// Walks at most this far for a spot, down to RELIEF_RANGE_URGENT when
/// about to burst.
constexpr int RELIEF_RANGE = 24, RELIEF_RANGE_URGENT = 8;
/// What each open cell round a spot costs it, in tenths of a cell walked,
/// at middling modesty; bursting cuts it down to RELIEF_URGENT_SHY of that.
constexpr float RELIEF_SHY = 30.0f;
constexpr float RELIEF_URGENT_SHY = 0.3f;
/// A spot in full light shows it as much as this many open cells round it
/// (AI/light_plan.md step 5).
constexpr float RELIEF_LIT_OPEN = 4.0f;
/// This modest at least, going in the dark away from a toilet, it is glad of it.
constexpr float RELIEF_DARK_MODEST = 0.55f;
/// Others' mess on a spot puts it off by this much per full fresh pile.
constexpr float RELIEF_MESS_COST = 300.0f;
/// A settler keeps its going this far from home.
constexpr int RELIEF_HOME_GAP = 6;
/// How long each takes.
constexpr float PISS_TIME = 3.5f, SHIT_TIME = 6.0f;
/// A puddle lands this many pixels before the feet, inside its own cell,
/// or this many into water it faces; a pile at them.
constexpr float PISS_REACH_PX = 2.5f, PISS_INTO_WATER_PX = 4.5f;

/// Feces this fresh (times their size, up to 1) are wet enough to stick to
/// the feet, which then print as many as FOOTPRINT_STEPS steps after, each
/// taking FOOTPRINT_WIPE of the wet off, as with blood.
constexpr float FECES_STICKY = 0.4f;
constexpr int FOOTPRINT_STEPS = 8;
constexpr float FOOTPRINT_WIPE = 0.15f;
/// Stepping in it puts this many pixels of this much ink on the feet.
constexpr int STEP_PIXELS = 4, STEP_INK = 200;
/// Feces on the feet dry through in this long.
constexpr float DIRT_DRY_TIME = 600.0f;
/// Walking over a full fresh pile it knows of costs this much more, in
/// tenths of a cell walked; less as it dries.
constexpr float MESS_STEP_COST = 200.0f;
/// Filthy from this much on the feet: thought this much less of, and not
/// greeted by anyone more modest than REPEL_MODEST.
constexpr float FILTHY = 0.15f;
constexpr float TRUST_FILTHY = -0.05f;
constexpr float REPEL_MODEST = 0.45f;
/// Stepping in it frightens the modest this much.
constexpr float STEPPED_DREAD = 0.1f;
/// Washes its feet once this filthy, the modest sooner: from WASH_FILTH at
/// middling modesty, half that at the most modest.
constexpr float WASH_FILTH = 0.12f;
/// Each wash takes this much ink off, at most WASH_PIXEL_INK a pixel, and
/// fouls the water FOUL_PER_PIXEL for each pixel's worth washed off.
constexpr int WASH_INK = 600, WASH_PIXEL_INK = 60;
constexpr float FOUL_PER_PIXEL = 0.2f;
/// A toilet it knows of is worth walking up to TOILET_REACH times as far
/// for; it costs only its dirt, TOILET_DIRT_COST tenths of a cell walked a
/// unit at middling modesty, less to the modest (a filthy toilet still beats
/// the open to them), more to the bold.
constexpr float TOILET_REACH = 2.0f;
constexpr float TOILET_DIRT_COST = 60.0f;
/// Knowing a toilet in reach, anywhere else costs this much more, at
/// middling modesty and in no hurry: about a 50-cell walk.
constexpr float TOILET_PREFER = 500.0f;
/// Finding the nearest toilet it knows busy, it waits its turn when its
/// modesty times how much it can still hold on reaches TOILET_WAIT, else
/// goes elsewhere. It waits TOILET_WAIT_GAP_MIN to MAX cells off, a moment
/// at a time.
constexpr float TOILET_WAIT = 0.25f;
constexpr int TOILET_WAIT_GAP_MIN = 3, TOILET_WAIT_GAP_MAX = 5;
constexpr float TOILET_WAIT_TIME = 1.5f;
/// What each use leaves in it, for a full one.
constexpr float TOILET_FECES = 0.3f, TOILET_PISS = 0.1f;
/// Chance to miss, and more for the urgent, the erratic and the frightened,
/// leaving it on the floor in front instead.
constexpr float TOILET_MISS = 0.05f, TOILET_MISS_URGENT = 0.25f, TOILET_MISS_ERRATIC = 0.3f,
                TOILET_MISS_DREAD = 0.2f;
/// Going straight into water fouls it this much for a full one.
constexpr float FECES_FOUL = 1.0f, PISS_FOUL = 0.25f;

/// A fright big enough (FRIGHT_WET in human_tuning.h) wets the timidest this
/// likely, the bravest never, with the bladder this full at least.
constexpr float FRIGHT_WET_CHANCE = 0.5f;
constexpr float FRIGHT_WET_BLADDER = 0.4f;
/// An accident puts this many pixels of piss on the legs and feet for a
/// full bladder, and of feces on the legs; shames the most modest this much
/// (dread) and costs them this much hope.
constexpr int ACCIDENT_PISS_PIXELS = 14, ACCIDENT_FECES_PIXELS = 8, ACCIDENT_INK = 170;
constexpr float ACCIDENT_SHAME = 0.15f, ACCIDENT_HOPE = 0.05f;
/// Seen going, the most modest is shamed this much.
constexpr float SEEN_SHAME = 0.1f;
/// Watchers think this much less of whoever they see going in the open (8
/// open cells round, by their own modesty), into water they might drink
/// (less if they walk together), and, the cruel, of an accident.
constexpr float TRUST_SEEN_GOING = -0.08f;
constexpr float TRUST_INTO_WATER = -0.3f, TRUST_INTO_WATER_GROUP = -0.1f;
constexpr float TRUST_MOCKED = -0.1f;
/// Cruel: this greedy and no more sociable than this.
constexpr float CRUEL_GREEDY = 0.6f, CRUEL_SOCIABLE = 0.4f;
/// Looking away for someone, a moment at a time.
constexpr float PRIVACY_WAIT_MIN = 0.8f, PRIVACY_WAIT_MAX = 1.4f;
/// Dead, it lets go of what is past this.
constexpr float LET_GO = 0.2f;

} // namespace

float Human::modesty() const { return (traits.sociable + (1.0f - traits.brave)) / 2.0f; }

float Human::needBar() const { return RELIEF_NEED + RELIEF_HOLD * modesty(); }

bool Human::needsToGo() const { return std::max(bladder, bowels) >= needBar(); }

bool Human::bursting() const { return std::max(bladder, bowels) >= 1.0f; }

void Human::takeIn(float water, float food) {
    relief_.drunk += BLADDER_PER_GULP * water;
    relief_.eaten += BOWELS_PER_CAKE * food;
}

void Human::press(float dt) {
    for (float& w : dirt.wet) w = std::max(0.0f, w - dt / DIRT_DRY_TIME);
    for (float& w : soaked.wet) w = std::max(0.0f, w - dt / DIRT_DRY_TIME);
    bool needed = needsToGo(), burst = bursting();
    float water = relief_.drunk * (1.0f - std::exp(-dt / WATER_PASSES));
    float food = relief_.eaten * (1.0f - std::exp(-dt / FOOD_PASSES));
    relief_.drunk -= water;
    relief_.eaten -= food;
    bladder = std::min(1.0f, bladder + water + BLADDER_CREEP * dt);
    bowels = std::min(1.0f, bowels + food + BOWELS_CREEP * dt);
    if ((needsToGo() && !needed) || (bursting() && !burst)) rethink_ = true;
    if (needsToGo() && !needed) sayLater(talk::Moment::NeedToGo);
}

bool Human::watchedAt(const World& world, Vec2i c) const {
    if (isErratic()) return false;
    // Those it walks with look away, unless erratic.
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && sight.sees(o->cell) && o->sight.sees(c) &&
            !(inGroupWith(*o) && !o->isErratic()))
            return true;
    return false;
}

// The fewer open cells round a spot, the better it feels, as far as it is
// worth walking; others' mess puts it off, its own does not.
bool Human::seekRelief(World& world) {
    float bar = needBar();
    float urgent = std::clamp((std::max(bladder, bowels) - bar) / (1.0f - bar), 0.0f, 1.0f);
    int range = int(std::lround(RELIEF_RANGE - (RELIEF_RANGE - RELIEF_RANGE_URGENT) * urgent));
    int toiletRange = int(std::lround(range * TOILET_REACH));
    float shy = isErratic() ? 0.0f : RELIEF_SHY * (0.5f + modesty()) * (1.0f - (1.0f - RELIEF_URGENT_SHY) * urgent);
    float now = world.time();
    // A toilet it knows of within reach makes anywhere else worse.
    bool toiletKnown = false;
    for (const auto& [at, t] : world.toilets())
        toiletKnown |= memory.tile(at) == Tile::Toilet && cheb(t.front, cell) <= toiletRange;
    int elsewhere = toiletKnown && !isErratic() ? int(TOILET_PREFER * (0.5f + modesty()) * (1.0f - urgent)) : 0;
    if (toiletKnown && waitForToilet(world, toiletRange, urgent)) return true;
    auto done = [&](Vec2i c) {
        bool toilet = toiletBefore(world, c).has_value();
        if (cheb(c, cell) > (toilet ? toiletRange : range) || world.mess().freshFeces(c, now) >= FECES_STICKY)
            return false;
        if (Human* o = world.occupant(c); o && o != this) return false;
        if (!isErratic() && !toilet)
            for (Vec2i d : DIRS8)
                if (pooled(memory.tile(c + d))) return false;
        if (escape_.settled && cheb(c, escape_.home) <= RELIEF_HOME_GAP) return false;
        return !watchedAt(world, c);
    };
    // Unknown ground may as well be open: only walls and the like hide it.
    auto cost = [&](Vec2i c) {
        if (auto t = toiletBefore(world, c); t && !isErratic())
            return 1 + int(seenToiletDirt(*t) * TOILET_DIRT_COST * (1.5f - modesty()));
        int open = 0;
        for (Vec2i d : DIRS8) {
            Tile t = memory.tile(c + d);
            open += t == Tile::Void || passable(t) || pooled(t);
        }
        return 1 + elsewhere + int((float(open) + RELIEF_LIT_OPEN * litSeen(c)) * shy) +
               int(world.mess().disgust(c, now, id) * RELIEF_MESS_COST);
    };
    // Its own mess doesn't put it off the way there.
    relief_.ignoreMess = true;
    auto p = search(world, done, true, cost);
    relief_.ignoreMess = false;
    if (!p) return false;
    if (p->empty()) {
        startRelieving(world);
        return true;
    }
    startWalking(Goal::Relieve, *p);
    return true;
}

// Piss lands before it, feces at its feet, with what the bladder holds
// beside them.
// Busy: someone else stands at it. The modest, still able to hold on,
// stand back and wait; anyone else goes elsewhere, as seekRelief carries on.
bool Human::waitForToilet(World& world, int reach, float urgent) {
    const Toilet* nearest = nullptr;
    for (const auto& [at, t] : world.toilets())
        if (memory.tile(at) == Tile::Toilet && cheb(t.front, cell) <= reach &&
            (!nearest || cheb(t.front, cell) < cheb(nearest->front, cell)))
            nearest = &t;
    if (!nearest || isErratic()) return false;
    Human* user = world.occupant(nearest->front);
    if (!user || user == this || !sight.sees(nearest->front)) return false;
    if (modesty() * (1.0f - urgent) < TOILET_WAIT) return false;
    Vec2i front = nearest->front;
    auto p = search(world, [&](Vec2i c) {
        int d = cheb(c, front);
        return d >= TOILET_WAIT_GAP_MIN && d <= TOILET_WAIT_GAP_MAX;
    }, true);
    if (!p) return false;
    if (!p->empty()) {
        startWalking(Goal::Relieve, *p);
        return true;
    }
    if (goal != Goal::Relieve || state != State::Idle) {
        note("WAITED FOR THE TOILET");
        sayLater(talk::Moment::ToiletWait, user->id);
    }
    startIdle(TOILET_WAIT_TIME, TOILET_WAIT_TIME);
    goal = Goal::Relieve;
    return true;
}

// Its own doing it knew already (startRelieving keeps it), so only someone
// else's fouling is news; the erratic don't mind.
void Human::seeToilet(const Toilet& toilet, Vec2i at) {
    auto [it, fresh] = relief_.toilets.try_emplace(at);
    bool knewFoul = !fresh && it->second.dirt >= Toilet::FOULED;
    it->second = {toilet.dirt, toilet.piss};
    if (knewFoul || toilet.dirt < Toilet::FOULED || isErratic()) return;
    note("SAW A FOUL TOILET");
    sayLater(talk::Moment::ToiletFoul);
}

std::optional<Vec2i> Human::toiletBefore(const World& world, Vec2i c) const {
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::Toilet)
            if (const Toilet* t = world.toilet(c + d); t && t->front == c) return c + d;
    return std::nullopt;
}

void Human::startRelieving(World& world) {
    relief_.shitting = bowels >= needBar() || bowels >= 1.0f;
    goal = Goal::Relieve;
    std::optional<Vec2i> toilet = toiletBefore(world, cell);
    float bar = needBar();
    float urgent = std::clamp((std::max(bladder, bowels) - bar) / (1.0f - bar), 0.0f, 1.0f);
    bool missed = toilet && rng_.chance(TOILET_MISS + TOILET_MISS_URGENT * urgent +
                                        (isErratic() ? TOILET_MISS_ERRATIC : 0.0f) + TOILET_MISS_DREAD * dread_);
    // Facing the water beside it, if any (the erratic and the bursting end up
    // there), else the wall beside it, if there is one.
    auto blocked = [&](Vec2i d, bool water) {
        Tile t = world.tile(cell + d);
        return water ? pooled(t) : !passable(t);
    };
    if (toilet && toilet->x != cell.x) facingLeft = toilet->x < cell.x;
    if (!toilet && !isErratic() && modesty() >= RELIEF_DARK_MODEST && world.light(cell) == 0) {
        note("WENT WHERE THE DARK HID ME");
        sayLater(talk::Moment::ReliefDark);
    }
    for (bool water : {true, false}) {
        if (toilet) break;
        if (blocked({-1, 0}, water))
            facingLeft = true;
        else if (blocked({1, 0}, water))
            facingLeft = false;
        else
            continue;
        break;
    }
    state = State::Relieving;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    timer = relief_.shitting ? SHIT_TIME : PISS_TIME;
    float fx = cell.x * CELL_PX + CELL_PX / 2.0f, fy = (cell.y + 1) * CELL_PX - 1.0f;
    float ahead = facingLeft ? -1.0f : 1.0f;
    // Into the toilet, it dirties the toilet; into water, it fouls the water;
    // else it lies where it lands.
    bool intoWater = false;
    auto drop = [&](MessKind kind, float x, float y, float size) {
        if (toilet && !missed)
            return world.soilToilet(*toilet, size * (kind == MessKind::Feces ? TOILET_FECES : TOILET_PISS),
                                    kind == MessKind::Piss);
        Vec2i at{floorDiv(int(std::floor(x)), CELL_PX), floorDiv(int(std::floor(y)), CELL_PX)};
        if (!passable(world.tile(at))) at = cell, x = fx, y = fy;
        if (pooled(world.tile(at))) {
            intoWater = true;
            return foulWater(world, at, size * (kind == MessKind::Feces ? FECES_FOUL : PISS_FOUL));
        }
        world.mess().drop(kind, at, x, y, size, world.time(), id, uint32_t(rng_.next()));
    };
    bool byWater = !toilet && pooled(world.tile(cell + Vec2i{facingLeft ? -1 : 1, 0}));
    if (relief_.shitting) {
        // Squatting with its back to the water, the erratic let it fall in.
        drop(MessKind::Feces, fx + (byWater && isErratic() ? ahead * 4.0f : -ahead), fy,
             std::clamp(bowels / 0.8f, 0.4f, 1.2f));
        if (bladder > 0.25f) drop(MessKind::Piss, fx, fy, bladder);
        note("TOOK A SHIT");
        repeated(Repeat::Going);
    } else {
        // Reaching the water it faces, else short of the wall.
        drop(MessKind::Piss, fx + ahead * (byWater ? PISS_INTO_WATER_PX : PISS_REACH_PX), fy,
             std::clamp(bladder / 0.8f, 0.3f, 1.2f));
        note("PISSED");
        repeated(Repeat::Going);
    }
    if (toilet) note(missed ? "MISSED THE TOILET" : "USED A TOILET");
    if (toilet && !missed) {
        // What it left there it knows of, so seeing it after is no news.
        const Toilet* t = world.toilet(*toilet);
        relief_.toilets[*toilet] = {t->dirt, t->piss};
        if (!relief_.noFlush) {
            relief_.noFlush = true;
            note("FOUND NO FLUSH");
            sayLater(talk::Moment::NoFlush);
        }
    }
    beSeenGoing(world, false, intoWater);
}

void Human::endRelieving() {
    bladder = 0.0f;
    if (relief_.shitting) bowels = 0.0f;
    relief_.shitting = false;
}

float Human::filth() const { return std::min(1.0f, dirt.feet() + 4.0f * dirt.total(false) + 2.0f * soaked.total()); }

void Human::tread(World& world) {
    if (relief_.frightWet) {
        relief_.frightWet = false;
        if (!dead() && bladder >= FRIGHT_WET_BLADDER && rng_.chance(FRIGHT_WET_CHANCE * (1.0f - traits.brave)))
            accident(world, true);
    }
    if (dead() || cell == relief_.messTrod) return;
    relief_.messTrod = cell;
    float fx = cell.x * CELL_PX + CELL_PX / 2.0f, fy = float((cell.y + 1) * CELL_PX);
    if (float fresh = world.mess().freshFeces(cell, world.time()); fresh >= FECES_STICKY) {
        relief_.messSteps = FOOTPRINT_STEPS;
        std::vector<int> feet;
        for (int i = 0; i < SPRITE_PX; i++)
            if (dirt.parts[size_t(i)] == uint8_t(BodyPart::Feet)) feet.push_back(i);
        for (int n = 0; n < STEP_PIXELS && !feet.empty(); n++) {
            size_t i = size_t(feet[size_t(rng_.range(0, int(feet.size()) - 1))]);
            dirt.amount[i] = uint8_t(std::min(255, dirt.amount[i] + STEP_INK));
            dirt.wet[i] = std::max(dirt.wet[i], fresh);
        }
        dirt.version++;
        // In the dark it never saw it coming.
        const bool dark = world.light(cell) == 0;
        if (!isErratic()) {
            scare(STEPPED_DREAD * modesty());
            note(dark ? "STEPPED IN SHIT IN THE DARK" : "STEPPED IN SHIT");
        }
        sayLater(dark ? talk::Moment::SteppedInDark : talk::Moment::SteppedIn);
        return;
    }
    if (relief_.messSteps <= 0) return;
    float x = fx + rng_.range(-1.0f, 1.0f), y = fy - 1.5f;
    world.mess().drop(MessKind::Print, cell, x, y, float(relief_.messSteps) / FOOTPRINT_STEPS, world.time(), id,
                      uint32_t(rng_.next()));
    relief_.messSteps--;
    for (size_t i = 0; i < dirt.amount.size(); i++)
        if (dirt.parts[i] == uint8_t(BodyPart::Feet) && dirt.wet[i] > 0.0f)
            dirt.amount[i] = uint8_t(dirt.amount[i] - int(dirt.amount[i] * FOOTPRINT_WIPE));
    dirt.version++;
}

int Human::messCost(const World& world, Vec2i c) const {
    if (isErratic()) return 0;
    const std::vector<int>* here = world.mess().at(c);
    if (!here) return 0;
    float sum = 0.0f;
    for (int i : *here) {
        const Dropping& d = world.mess().all()[size_t(i)];
        // Its own, but for fresh feces it would step in.
        bool mine = relief_.ignoreMess && d.who == id &&
                    !(d.kind == MessKind::Feces && Mess::freshness(d, world.time()) >= FECES_STICKY);
        if (!memory.mess.count(i) || mine) continue;
        sum += Mess::disgust(d, world.time());
    }
    return int(sum * MESS_STEP_COST);
}

void Human::noticeFilth(const Human& o) {
    if (isErratic()) return;
    if (o.filth() < FILTHY) {
        relief_.filthNoted.erase(o.id);
        return;
    }
    if (relief_.filthNoted.insert(o.id).second) regard(o.id, TRUST_FILTHY);
}

bool Human::repelledBy(const Human& o) const {
    return !isErratic() && o.filth() >= FILTHY && modesty() > REPEL_MODEST;
}

bool Human::wantsWashing() const {
    return !isErratic() && filth() >= WASH_FILTH * (1.5f - modesty());
}

// Only where it goes in; it seeps through the rest slowly (Mess::update).
void Human::foulWater(World& world, Vec2i water, float amount) { world.mess().foulWater(water, amount); }

// Feces first, then piss; piss fouls a third as much.
bool Human::washDirt(World& world, Vec2i water) {
    int left = WASH_INK;
    float foul = 0.0f;
    for (BodyBlood* layer : {&dirt, &soaked}) {
        int off = 0;
        for (size_t i = 0; i < layer->amount.size() && left > 0; i++) {
            int k = std::min({int(layer->amount[i]), WASH_PIXEL_INK, left});
            layer->amount[i] = uint8_t(layer->amount[i] - k);
            left -= k;
            off += k;
        }
        if (off > 0) layer->version++;
        foul += off / 255.0f * FOUL_PER_PIXEL * (layer == &dirt ? 1.0f : 1.0f / 3.0f);
    }
    if (foul <= 0.0f) return false;
    foulWater(world, water, foul);
    return dirt.any() || soaked.any();
}

bool Human::inGroupWith(const Human& o) const {
    return o.follows() == id || follows() == o.id || (follows() >= 0 && o.follows() == follows());
}

bool Human::givePrivacy(World& world) {
    int before = relief_.privacyFor;
    relief_.privacyFor = -1;
    if (isErratic() || needsToGo()) return false;
    const Human* going = nullptr;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && sight.sees(o->cell) && inGroupWith(*o) &&
            (o->state == State::Relieving || (o->goal == Goal::Relieve && o->state == State::Walking)) &&
            (!going || cheb(cell, o->cell) < cheb(cell, going->cell)))
            going = o.get();
    if (!going) return false;
    relief_.privacyFor = going->id;
    if (before != going->id) sayLater(talk::Moment::Privacy, going->id);
    if (going->cell.x != cell.x) facingLeft = going->cell.x > cell.x;
    startIdle(PRIVACY_WAIT_MIN, PRIVACY_WAIT_MAX);
    goal = Goal::Company;
    return true;
}

void Human::accident(World& world, bool fright) {
    bool shit = !fright && bowels >= 1.0f;
    float fx = cell.x * CELL_PX + CELL_PX / 2.0f, fy = (cell.y + 1) * CELL_PX - 1.0f;
    auto drop = [&](MessKind kind, float size) {
        world.mess().drop(kind, cell, fx + rng_.range(-1.0f, 1.0f), fy, size, world.time(), id,
                          uint32_t(rng_.next()));
    };
    auto soil = [&](BodyBlood& layer, std::initializer_list<BodyPart> parts, int pixels) {
        std::vector<int> on;
        for (int i = 0; i < SPRITE_PX; i++)
            for (BodyPart p : parts)
                if (layer.parts[size_t(i)] == uint8_t(p)) on.push_back(i);
        for (int n = 0; n < pixels && !on.empty(); n++) {
            size_t i = size_t(on[size_t(rng_.range(0, int(on.size()) - 1))]);
            layer.amount[i] = uint8_t(std::min(255, layer.amount[i] + ACCIDENT_INK));
            layer.wet[i] = 1.0f;
        }
        layer.version++;
    };
    if (bladder > 0.1f) {
        drop(MessKind::Piss, std::max(0.4f, bladder));
        soil(soaked, {BodyPart::Legs, BodyPart::Feet}, int(std::lround(ACCIDENT_PISS_PIXELS * bladder)));
    }
    if (shit) {
        drop(MessKind::Feces, bowels);
        soil(dirt, {BodyPart::Legs}, ACCIDENT_FECES_PIXELS);
        soil(dirt, {BodyPart::Feet}, ACCIDENT_FECES_PIXELS / 2);
        relief_.messSteps = FOOTPRINT_STEPS;
        bowels = 0.0f;
    }
    bladder = 0.0f;
    note(shit ? "SOILED MYSELF" : fright ? "WET MYSELF IN FRIGHT" : "WET MYSELF");
    sayLater(talk::Moment::Accident);
    if (!isErratic()) {
        // Below FRIGHT_WET, so the shame itself wets nothing more.
        scare(ACCIDENT_SHAME * modesty());
        lift(-ACCIDENT_HOPE * modesty());
    }
    beSeenGoing(world, true, false);
}

void Human::letGo(World& world) {
    float fx = cell.x * CELL_PX + CELL_PX / 2.0f, fy = (cell.y + 1) * CELL_PX - 1.0f;
    if (bladder > LET_GO)
        world.mess().drop(MessKind::Piss, cell, fx, fy, bladder, world.time(), id, uint32_t(rng_.next()));
    if (bowels > LET_GO)
        world.mess().drop(MessKind::Feces, cell, fx, fy, bowels, world.time(), id, uint32_t(rng_.next()));
    bladder = bowels = 0.0f;
}

// Those it walks with look away: only an accident or the water is theirs to
// see.
void Human::beSeenGoing(World& world, bool accident, bool intoWater) {
    int open = 0;
    for (Vec2i d : DIRS8) open += passable(world.tile(cell + d));
    const Human* nearest = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->isErratic() || !o->sight.sees(cell)) continue;
        bool group = o->inGroupWith(*this);
        if (intoWater) {
            o->regard(id, group ? TRUST_INTO_WATER_GROUP : TRUST_INTO_WATER);
            o->note("SAW @ GO IN THE WATER", id);
            o->sayLater(talk::Moment::IntoWater, id);
        } else if (accident) {
            bool cruel = o->traits.greedy >= CRUEL_GREEDY && o->traits.sociable <= CRUEL_SOCIABLE;
            if (group && !cruel) continue;
            if (cruel) {
                o->regard(id, TRUST_MOCKED);
                o->sayLater(talk::Moment::Mock, id);
            }
            o->note(cruel ? "LAUGHED AT @ FOR WETTING THEMSELVES" : "SAW @ WET THEMSELVES", id);
        } else {
            if (group) continue;
            o->regard(id, TRUST_SEEN_GOING * o->modesty() * 2.0f * float(open) / 8.0f);
            o->note("SAW @ RELIEVE THEMSELVES", id);
        }
        if (!nearest || cheb(cell, o->cell) < cheb(cell, nearest->cell)) nearest = o.get();
    }
    if (!nearest || isErratic()) return;
    scare(SEEN_SHAME * modesty());
    note(accident ? "SOMEONE SAW MY ACCIDENT" : "SOMEONE SAW ME GO");
    sayLater(talk::Moment::SeenGoing, nearest->id);
}
