#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <cstdlib>

namespace {

/// Drops a second from any bleeding at all, and more per health a second bled.
constexpr float DRIP_RATE = 1.5f;
constexpr float DRIP_RATE_PER_BLEED = 80.0f;
/// A drop's radius in world pixels: a trickle leaves single pixels, a deep
/// wound splotches.
constexpr float DROP_RADIUS = 0.4f;
constexpr float DROP_RADIUS_PER_BLEED = 45.0f;
constexpr float DROP_RADIUS_MAX = 4.0f;
/// Arms hang this far either side of the feet.
constexpr float ARM_OFFSET_PX = 3.0f;
/// The dead lie this long either side of the feet, this tall above them,
/// and their blood pools out from under them, in drops this much wider.
constexpr float BODY_HALF_PX = 9.0f;
constexpr float BODY_HEIGHT_PX = 10.0f;
constexpr float POOL_SPREAD_PX = 2.0f;
constexpr float POOL_DROP_MUL = 1.3f;
/// Drops from a hit, and how much more per damage; they fly up to this far.
constexpr int SPRAY_DROPS = 2;
constexpr float SPRAY_DROPS_PER_DAMAGE = 25.0f;
constexpr float SPRAY_REACH_PX = 2.0f * CELL_PX;
/// Half the angle a spray fans out over, in radians.
constexpr float SPRAY_FAN = 0.6f;
constexpr float SPRAY_RADIUS_MIN = 0.4f, SPRAY_RADIUS_PER_DAMAGE = 3.0f;

/// Pixels of the body a wounding hit bloodies at once, and more per damage.
constexpr int HIT_PIXELS = 2;
constexpr float HIT_PIXELS_PER_DAMAGE = 15.0f;
/// How thick blood lands on a body: from a hit, a drip, a spray on a
/// bystander, and stepping in it.
constexpr int HIT_INK = 140, DRIP_INK = 30, SPLASH_INK = 120, STEP_INK = 100;
/// Only stains this thick wet the feet: a footprint never makes more.
constexpr float FOOT_WET_MIN = 0.04f;
/// Wet feet print this many steps, fainter each time.
constexpr int FOOTPRINT_STEPS = 6;
constexpr float FOOTPRINT_RADIUS = 0.8f;
/// Each footprint takes this share of the wet blood off the feet.
constexpr float FOOTPRINT_WIPE = 0.3f;
/// Seconds for blood on the body to dry, as on the floor.
constexpr float BODY_DRY_TIME = 90.0f;
/// Every this many seconds, fresh blood this thick runs a pixel down the
/// body, taking this share of itself along.
constexpr float RUN_EVERY = 0.4f;
constexpr int RUN_MIN = 90;
constexpr float RUN_WET = 0.5f;
constexpr float RUN_SHARE = 0.35f;
/// Bloodier than this above the feet, or the feet alone this bloody, and it
/// goes to wash; a walk through a trail soon bloodies the feet, so they
/// wait until well soaked.
constexpr float WASH_MIN = 0.004f;
constexpr float WASH_FEET_MIN = 0.6f;
/// Water someone it sees lives by is as far as this many tenths of a cell
/// more walking (human_thirst.cpp).
constexpr int OTHERS_WATER_COST = 400;
/// Each wash takes this long and this much blood off, at most this much off
/// any one pixel, from the head down; the bloodier, the longer it washes.
constexpr float WASH_STEP = 0.5f;
constexpr int WASH_INK = 1200, WASH_PIXEL_INK = 40;
/// How red the water it washes in gets for each pixel's worth of blood
/// washed off, and the water next to that, half as red.
constexpr float WASH_TINT_PER_PIXEL = 0.5f / (CELL_PX * CELL_PX);
/// Blood this thick on a cell beside a trap gives the trap away.
constexpr float TRAP_BLOOD_MIN = 0.02f;
/// A cell this bloody is a pool. Walking on or beside one costs as much as
/// this many more straight steps.
constexpr float POOL_MIN = 0.35f;
constexpr int POOL_STEPS = 4;
/// A fresh pool seen this close scares it this far off, for this long.
constexpr int POOL_FEAR_DIST = 6;
constexpr float POOL_FEAR_TIME = 12.0f;
/// Bloodier than this with no wound to show is suspicious, and stays so
/// this long after last seen that way; suspects are kept this far away.
constexpr float SUSPECT_BLOOD = 0.01f;
constexpr float SUSPECT_TIME = 30.0f;
constexpr int SUSPECT_DIST = 5;
/// Dried blood is old news when there's little of it: under DRY_MIN it
/// counts for nothing, and it weighs more the more there is, as much as
/// fresh blood from DRY_FULL on. The weight scales trust, dread and how
/// long the suspicion lasts; below SUSPECT_WEIGHT it isn't suspicious at
/// all, from ACCUSE_WEIGHT it is thrown in their face, and a weight this
/// much over the last one judged is a new deed.
constexpr float DRY_MIN = 0.03f;
constexpr float DRY_FULL = 0.15f;
constexpr float SUSPECT_WEIGHT = 0.1f;
constexpr float ACCUSE_WEIGHT = 0.5f;
constexpr float HEAVIER = 0.3f;
/// What blood on each part says, by BodyPart: round the mouth or on the
/// hands it is what they did, on the shoes only where they walked.
constexpr float TELLING[] = {3.0f, 1.0f, 2.0f, 2.0f, 1.0f, 0.25f};
/// Only this healthy a human takes a wounded one to water, when it meets
/// them this close; and keeps this close behind its escort.
constexpr float HELPER_HEALTH = 0.7f;
/// Below this much sociability a human never escorts anyone.
constexpr float LONER = 0.3f;
/// How much a pool of blood, its own or another's, and an escort to water
/// strike it, as episodes to talk about.
constexpr float EPISODE_POOL = 0.5f, EPISODE_OWN_POOL = 0.7f, EPISODE_ESCORT = 0.5f, EPISODE_ESCORTED = 0.6f;
/// What seeing someone bloodied with no wound of their own does to trust,
/// and what being led to water does. The rest of trust is in human_social.cpp.
constexpr float TRUST_SUSPECT = -0.2f;
/// What a fresh pool of blood and a bloodied suspect add to dread.
constexpr float DREAD_POOL = 0.3f;
constexpr float DREAD_SUSPECT = 0.2f;
constexpr float TRUST_LED_TO_WATER = 0.3f;
/// Blood at least this much its own it knows for its own: it saw itself
/// bleed there, so it neither fears it nor follows it.
constexpr float OWN_BLOOD = 0.5f;
constexpr int ESCORT_MEET = 3;
constexpr int ESCORT_FOLLOW = 1;
/// Waits by the water a moment at a time while they wash.
constexpr float ESCORT_WAIT_MIN = 0.5f, ESCORT_WAIT_MAX = 1.0f;
/// Following close behind, it waits a moment at a time.
constexpr float ESCORTED_WAIT_MIN = 0.2f, ESCORTED_WAIT_MAX = 0.4f;
/// A trail is followed this far, and steered round by those carrying this
/// many cakes or more, as though it were this many more straight steps.
constexpr int TRACK_RANGE = 20;
constexpr int RICH_CAKES = 2;
/// Blood walked up to on a trail isn't walked up to again for this long.
constexpr float TRACK_FORGET = 60.0f;
constexpr int TRAIL_STEPS = 3;

} // namespace

bool BodyBlood::any() const {
    for (uint8_t a : amount)
        if (a) return true;
    return false;
}

bool BodyBlood::wetAny() const {
    for (size_t i = 0; i < amount.size(); i++)
        if (amount[i] && wet[i] > 0.0f) return true;
    return false;
}

float BodyBlood::total(bool feet) const {
    int sum = 0, body = 0;
    for (size_t i = 0; i < amount.size(); i++) {
        if (!feet && parts[i] == uint8_t(BodyPart::Feet)) continue;
        sum += amount[i];
        body += parts[i] != NO_PART;
    }
    return body ? sum / float(body * 255) : 0.0f;
}

float BodyBlood::feet() const {
    int sum = 0, n = 0;
    for (size_t i = 0; i < amount.size(); i++)
        if (parts[i] == uint8_t(BodyPart::Feet)) {
            sum += amount[i];
            n++;
        }
    return n ? sum / float(n * 255) : 0.0f;
}

float BodyBlood::freshness() const {
    float wetInk = 0.0f;
    int sum = 0;
    for (size_t i = 0; i < amount.size(); i++) {
        wetInk += amount[i] * wet[i];
        sum += amount[i];
    }
    return sum ? wetInk / sum : 0.0f;
}

float BodyBlood::telling() const {
    float sum = 0.0f;
    int body = 0;
    for (size_t i = 0; i < amount.size(); i++) {
        if (parts[i] == NO_PART) continue;
        sum += amount[i] * TELLING[parts[i]];
        body++;
    }
    return body ? sum / (body * 255.0f) : 0.0f;
}

void Human::stain(std::initializer_list<BodyPart> parts, int pixels, int ink, bool low) {
    std::vector<int> on;
    int lowest = -1;
    for (int i = 0; i < SPRITE_PX; i++) {
        bool in = false;
        for (BodyPart p : parts) in |= stains.parts[size_t(i)] == uint8_t(p);
        if (!in) continue;
        if (low && i / SPRITE_W > lowest) {
            lowest = i / SPRITE_W;
            on.clear();
        }
        if (!low || i / SPRITE_W == lowest) on.push_back(i);
    }
    if (on.empty()) return;
    for (int n = 0; n < pixels; n++) {
        size_t i = size_t(on[size_t(rng_.range(0, int(on.size()) - 1))]);
        stains.amount[i] = uint8_t(std::min(255, stains.amount[i] + ink));
        stains.wet[i] = 1.0f;
    }
    stains.version++;
}

// Runs from the bottom up, so blood moves at most a pixel each time.
void Human::dryStains(float dt) {
    bool wet = false;
    for (size_t i = 0; i < stains.wet.size(); i++)
        if (stains.wet[i] > 0.0f) {
            stains.wet[i] = std::max(0.0f, stains.wet[i] - dt / BODY_DRY_TIME);
            wet = true;
        }
    if (!wet || (gore_.run -= dt) > 0.0f) return;
    gore_.run = RUN_EVERY;
    for (int y = SPRITE_H - 2; y >= 0; y--)
        for (int x = 0; x < SPRITE_W; x++) {
            size_t i = size_t(y * SPRITE_W + x), below = i + SPRITE_W;
            if (stains.amount[i] < RUN_MIN || stains.wet[i] < RUN_WET || stains.parts[below] == NO_PART) continue;
            int moved = int(stains.amount[i] * RUN_SHARE);
            stains.amount[i] = uint8_t(stains.amount[i] - moved);
            stains.amount[below] = uint8_t(std::min(255, stains.amount[below] + moved));
            stains.wet[below] = std::max(stains.wet[below], stains.wet[i]);
        }
    // Drying darkens it too, so it is redrawn now and then while wet.
    stains.version++;
}

// Drops fall under the wound they come from, picked by how hard each bleeds,
// within the cell the human stands on, or round the body lying dead.
void Human::spill(World& world, float dt) {
    float t = stepProgress();
    float fx = (prev.x + (cell.x - prev.x) * t) * CELL_PX + CELL_PX / 2.0f;
    float fy = (prev.y + (cell.y - prev.y) * t) * CELL_PX + CELL_PX;

    dryStains(dt);
    for (const Spray& s : gore_.sprays) {
        stain({s.part}, HIT_PIXELS + int(s.damage * HIT_PIXELS_PER_DAMAGE), HIT_INK);
        int n = SPRAY_DROPS + int(s.damage * SPRAY_DROPS_PER_DAMAGE);
        float len = std::hypot(s.dx, s.dy);
        float aim = std::atan2(s.dy, s.dx);
        for (int i = 0; i < n; i++) {
            // All round, it stays close; along a blow, it flies.
            float a = len > 0.0f ? aim + rng_.range(-SPRAY_FAN, SPRAY_FAN) : rng_.range(0.0f, 6.2832f);
            float d = SPRAY_REACH_PX * (len > 0.0f ? rng_.range(0.2f, 1.0f) : rng_.range(0.0f, 0.6f));
            float r = rng_.range(SPRAY_RADIUS_MIN, SPRAY_RADIUS_MIN + s.damage * SPRAY_RADIUS_PER_DAMAGE);
            // It splashes on the first thing in its way: someone else, a
            // wall, a table, a statue, or water.
            float x = fx, y = fy - 2.0f;
            Human* hit = nullptr;
            for (float f = 1.0f;; f += 1.0f) {
                x = fx + std::cos(a) * std::min(f, d);
                y = fy - 2.0f + std::sin(a) * std::min(f, d);
                Vec2i c{floorDiv(int(std::floor(x)), CELL_PX), floorDiv(int(std::floor(y)), CELL_PX)};
                if (Human* o = world.occupant(c); o && o != this) hit = o;
                if (hit || f >= d || !passable(world.tile(c))) break;
            }
            if (hit)
                hit->stain({BodyPart::Head, BodyPart::Torso, BodyPart::FrontArm, BodyPart::BackArm}, 1, SPLASH_INK);
            else
                world.blood().drop(world, x, y, r, id, rng_);
        }
    }
    gore_.sprays.clear();

    // Stepping into a thick wet stain wets the feet, which print it on for
    // a few steps after.
    if (!dead() && cell != gore_.trod) {
        gore_.trod = cell;
        const Stain* under = world.blood().at(cell);
        if (under && under->wet > 0.0f && under->total() >= FOOT_WET_MIN) {
            gore_.bloodySteps = FOOTPRINT_STEPS;
            gore_.feetBlood = under->mostly();
            stain({BodyPart::Feet}, 2, STEP_INK);
        } else if (gore_.bloodySteps > 0) {
            world.blood().drop(world, fx + rng_.range(-1.0f, 1.0f), fy - 1.5f, FOOTPRINT_RADIUS, gore_.feetBlood, rng_,
                               float(gore_.bloodySteps) / FOOTPRINT_STEPS);
            gore_.bloodySteps--;
            for (size_t i = 0; i < stains.amount.size(); i++)
                if (stains.parts[i] == uint8_t(BodyPart::Feet) && stains.wet[i] > 0.0f)
                    stains.amount[i] = uint8_t(stains.amount[i] - int(stains.amount[i] * FOOTPRINT_WIPE));
            stains.version++;
        }
    }

    float b = bleeding();
    if (b <= 0.0f) {
        gore_.drip = 0.0f;
        return;
    }
    gore_.drip += (DRIP_RATE + DRIP_RATE_PER_BLEED * b) * dt;
    float radius = std::min(DROP_RADIUS_MAX, DROP_RADIUS + DROP_RADIUS_PER_BLEED * b) * (dead() ? POOL_DROP_MUL : 1.0f);
    for (; gore_.drip >= 1.0f; gore_.drip -= 1.0f) {
        float pick = rng_.range(0.0f, b), x = fx, y = fy - rng_.range(1.0f, 4.0f);
        const Wound* w = &wounds.back();
        for (const Wound& o : wounds)
            if ((pick -= o.bleed) <= 0.0f) {
                w = &o;
                break;
            }
        float side = facingLeft ? -1.0f : 1.0f;
        if (dead()) {
            x += rng_.range(-BODY_HALF_PX, BODY_HALF_PX);
            y = fy + rng_.range(-BODY_HEIGHT_PX, POOL_SPREAD_PX);
        } else if (w->part == BodyPart::FrontArm)
            x += side * ARM_OFFSET_PX;
        else if (w->part == BodyPart::BackArm)
            x -= side * ARM_OFFSET_PX;
        else
            x += rng_.range(-1.5f, 1.5f);
        world.blood().drop(world, x, y, radius, id, rng_);
        stain({w->part}, 1, DRIP_INK);
    }
}

bool Human::startWash(World& world) {
    // The brave put up with more blood on them before bothering.
    float put = 0.5f + traits.brave;
    if (hungry() || thirsty() || fight_.fear > 0.0f || fight_.foe >= 0) return false;
    bool bloody = stains.total(false) >= WASH_MIN * put || stains.feet() >= WASH_FEET_MIN * put;
    if (!bloody && !wantsWashing()) return false;
    auto water = [&](Vec2i c) -> std::optional<Vec2i> {
        for (Vec2i d : DIRS4)
            if (drinkable(memory.tile(c + d))) return c + d;
        return std::nullopt;
    };
    // Not where someone else stands: that one may be washing there, or
    // waiting there for it.
    // Not in water someone it sees lives by, if there's other water.
    auto p = search(world, [&](Vec2i c) { return water(c).has_value(); }, true,
                    [&](Vec2i c) { return 1 + (othersWater(world, *water(c)) ? OTHERS_WATER_COST : 0); });
    if (!p) return false;
    goal = Goal::Wash;
    if (!p->empty()) {
        startWalking(Goal::Wash, *p);
        return true;
    }
    gore_.washAt = *water(cell);
    if (gore_.washAt.x != cell.x) facingLeft = gore_.washAt.x < cell.x;
    state = State::Washing;
    moving = false;
    path_.clear();
    timer = WASH_STEP;
    note(bloody ? "WASHED OFF BLOOD" : "WASHED OFF SHIT");
    offendDrinkers(world, gore_.washAt);
    return true;
}

void Human::wash(World& world) {
    if (observe(world)) rethink_ = true;
    if (rethink_ || hungry() || thirsty() || fight_.fear > 0.0f || fight_.foe >= 0) return choose(world);
    int left = WASH_INK, off = 0;
    for (size_t i = 0; i < stains.amount.size() && left > 0; i++) {
        int k = std::min({int(stains.amount[i]), WASH_PIXEL_INK, left});
        stains.amount[i] = uint8_t(stains.amount[i] - k);
        left -= k;
        off += k;
    }
    stains.version++;
    float tint = off / 255.0f * WASH_TINT_PER_PIXEL;
    world.blood().tintWater(gore_.washAt, tint);
    for (Vec2i d : DIRS4)
        if (drinkable(world.tile(gore_.washAt + d))) world.blood().tintWater(gore_.washAt + d, tint / 2.0f);
    // Feces once the blood is off, fouling the water (human_relief.cpp).
    bool dirty = !stains.any() && washDirt(world, gore_.washAt);
    if (!stains.any() && !dirty) return choose(world);
    timer = WASH_STEP;
}

// A trap that drew blood has it all round it. Pools are kept off for good;
// a fresh one is frightening besides, unless it is its own.
bool Human::noticeBlood(const World& world, Vec2i c) {
    const Stain& s = memory.blood[c];
    float total = s.total();
    bool news = false;
    if (total >= TRAP_BLOOD_MIN)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (const Trap* t = world.trap(c + Vec2i{dx, dy}))
                    news |= memory.traps.emplace(c + Vec2i{dx, dy}, t->kind).second;
    if (total >= POOL_MIN && memory.pools.insert(c).second) {
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) memory.shunned.insert(c + Vec2i{dx, dy});
        if (s.share(id) >= OWN_BLOOD) {
            if (s.wet > 0.0f && cheb(cell, c) < POOL_FEAR_DIST) {
                note("SAW A POOL OF MY OWN BLOOD");
                rememberAt(c, Episode::What::Pool, EPISODE_OWN_POOL, id);
                sayLater(talk::Moment::OwnPool);
            }
        } else if (s.wet > 0.0f && cheb(cell, c) < POOL_FEAR_DIST) {
            gore_.scare = c;
            gore_.scaredUntil = clock_ + POOL_FEAR_TIME;
            scare(DREAD_POOL);
            note("SAW A FRESH POOL OF BLOOD");
            rememberAt(c, Episode::What::Pool, EPISODE_POOL);
            sayLater(talk::Moment::Pool);
            news = true;
        }
    }
    return news;
}

// Bleeding explains the blood; without a wound, it's someone else's. Wet,
// it was spilled just now; a spot of it dried is long over, but someone
// caked in it did something.
bool Human::judge(const Human& o) {
    float total = o.stains.telling();
    if (!o.wounds.empty() || total < SUSPECT_BLOOD) return false;
    float fresh = o.stains.freshness();
    float weight = std::max(fresh, std::clamp((total - DRY_MIN) / (DRY_FULL - DRY_MIN), 0.0f, 1.0f));
    if (weight < SUSPECT_WEIGHT) return false;
    auto it = gore_.suspects.find(o.id);
    bool known = it != gore_.suspects.end() && it->second.until > clock_;
    bool news = !known || weight >= it->second.weight + HEAVIER;
    float until = clock_ + SUSPECT_TIME * weight;
    if (known) until = std::max(until, it->second.until);
    gore_.suspects[o.id] = {until, weight};
    if (news) {
        note(fresh >= ACCUSE_WEIGHT     ? "@ WAS BLOODY WITHOUT A WOUND"
             : weight >= ACCUSE_WEIGHT ? "@ WAS CAKED IN DRIED BLOOD"
                                       : "@ HAD DRIED BLOOD ON THEM",
             o.id);
        // Said to their face, only when they are near enough to hear it.
        if (weight >= ACCUSE_WEIGHT && cheb(cell, o.cell) <= SUSPECT_DIST) sayLater(talk::Moment::Accuse, o.id);
        regard(o.id, TRUST_SUSPECT * weight);
        scare(DREAD_SUSPECT * weight);
    }
    return news;
}

bool Human::suspicious(int id) const {
    auto it = gore_.suspects.find(id);
    return it != gore_.suspects.end() && it->second.until > clock_;
}

// The well off would rather not cross where someone bled lately.
int Human::bloodCost(Vec2i c) const {
    int extra = !memory.shunned.empty() && memory.shunned.count(c) ? POOL_STEPS * 10 : 0;
    if (cakes >= RICH_CAKES)
        if (auto it = memory.blood.find(c); it != memory.blood.end() && it->second.wet > 0.0f) extra += TRAIL_STEPS * 10;
    return extra;
}

// Foes are kept away from like suspects, but for one worth lying to when
// nothing presses.
bool Human::shuns(const Human& o) const {
    return &o != this && !o.dead() && o.id != fight_.foe && (suspicious(o.id) || (isFoe(o.id) && !(lieWorthy(o) && freeToLie()))) && sight.sees(o.cell) &&
           !(starving() && o.cakes > 0);
}

// Every walk keeps out of SUSPECT_DIST of them, or no nearer than it set
// out from, else it would walk back up to whoever it just kept away from.
std::function<bool(Vec2i)> Human::shunZone(const World& world, Vec2i start) const {
    std::vector<Vec2i> them;
    for (const auto& o : world.humans())
        if (shuns(*o)) them.push_back(o->cell);
    if (them.empty()) return nullptr;
    return [them, start](Vec2i c) {
        for (Vec2i o : them)
            if (cheb(c, o) < std::min(SUSPECT_DIST, cheb(start, o))) return true;
        return false;
    };
}

bool Human::keepAway(World& world) {
    Vec2i from;
    int dist = 0;
    // Neither the starving nor the erratic mind blood.
    if (gore_.scaredUntil > clock_ && !starving() && !isErratic() && cheb(cell, gore_.scare) < POOL_FEAR_DIST) {
        from = gore_.scare;
        dist = POOL_FEAR_DIST;
    } else {
        for (const auto& o : world.humans()) {
            if (!shuns(*o) || cheb(cell, o->cell) >= SUSPECT_DIST) continue;
            if (!dist || cheb(cell, o->cell) < cheb(cell, from)) from = o->cell;
            dist = SUSPECT_DIST + 1;
        }
    }
    if (!dist) return false;
    auto p = search(world, [&](Vec2i c) { return cheb(c, from) >= dist; }, true);
    if (!p || p->empty()) return false;
    startWalking(Goal::Avoid, *p);
    return true;
}

bool Human::wouldEscort(const Human& o) const {
    return traits.sociable >= LONER && escorting_.escortee < 0 && escorting_.by < 0 && !hungry() && health >= HELPER_HEALTH &&
           cakes > 0 && fight_.foe < 0 &&
           fight_.fear <= 0.0f && &o != this && !o.dead() && !o.wounds.empty() && o.stains.total() >= SUSPECT_BLOOD &&
           o.escorting_.by < 0 && o.escorting_.escortee < 0 && !o.hungry() && o.fight_.foe < 0 && o.fight_.fear <= 0.0f &&
           o.state != State::Washing && !suspicious(o.id) && !isFoe(o.id) && !o.isFoe(id) && sight.sees(o.cell) &&
           cheb(cell, o.cell) <= ESCORT_MEET;
}

// Whichever of the two knows the nearer water shows the way there: the
// helper learns the wounded one's way when theirs is nearer.
bool Human::offerEscort(World& world) {
    Human* o = nullptr;
    for (const auto& h : world.humans())
        if (wouldEscort(*h) && (!o || cheb(cell, h->cell) < cheb(cell, o->cell))) o = h.get();
    if (!o) return false;
    auto beside = [&](Vec2i c) {
        for (Vec2i d : DIRS4)
            if (drinkable(memory.tile(c + d)) || drinkable(o->memory.tile(c + d))) return true;
        return false;
    };
    auto mine = search(world, beside);
    auto theirs = o->search(world, beside);
    if (!mine && !theirs) return false;
    if (!mine || (theirs && theirs->size() < mine->size())) {
        for (Vec2i c : *theirs) memory.see(c, o->memory.tile(c));
        Vec2i end = theirs->empty() ? o->cell : theirs->back();
        memory.see(end, o->memory.tile(end));
        for (Vec2i d : DIRS4)
            if (drinkable(o->memory.tile(end + d))) memory.see(end + d, o->memory.tile(end + d));
        escorting_.to = end;
    } else {
        escorting_.to = mine->empty() ? cell : mine->back();
    }
    escorting_.escortee = o->id;
    escorting_.arrived = false;
    escorting_.drink = false;
    o->escorting_.by = id;
    o->rethink_ = true;
    if (!leadEscort(world)) return false;
    note("TOOK @ TO WATER", o->id);
    o->note("@ TOOK ME TO WATER", id);
    remember(Episode::What::Escort, EPISODE_ESCORT, id, o->id);
    o->remember(Episode::What::Escort, EPISODE_ESCORTED, id, o->id);
    sayLater(talk::Moment::Escort, o->id);
    o->sayLater(talk::Moment::Escorted, id);
    return true;
}

// There, it shows them the water and waits until they are done washing.
bool Human::leadEscort(World& world) {
    if (escorting_.escortee < 0) return false;
    Human& o = world.human(escorting_.escortee);
    bool on = !o.dead() && !hungry() && fight_.foe < 0 && fight_.fear <= 0.0f && o.fight_.foe < 0 && o.fight_.fear <= 0.0f && !o.hungry();
    if (on && escorting_.arrived) {
        // Still standing by after being let go, until it sees them wash, or
        // drink when it led them to drink (human_thirst.cpp).
        on = escorting_.drink ? o.state == State::Drinking || o.goal == Goal::Drink || o.goal == Goal::Escorted
                          : o.state == State::Washing || o.goal == Goal::Wash || o.goal == Goal::Escorted;
        if (on) {
            startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
            goal = Goal::Escort;
            return true;
        }
    }
    if (!on || o.escorting_.by != id) {
        if (o.escorting_.by == id) o.escorting_.by = -1;
        escorting_.escortee = -1;
        return false;
    }
    if (cell == escorting_.to) {
        escorting_.arrived = true;
        o.regard(id, TRUST_LED_TO_WATER);
        for (Vec2i d : DIRS4)
            if (drinkable(memory.tile(cell + d))) o.memory.see(cell + d, memory.tile(cell + d));
        o.escorting_.by = -1;
        o.rethink_ = true;
        startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
        goal = Goal::Escort;
        return true;
    }
    if (cheb(cell, o.cell) > ESCORT_MEET) {
        startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
        goal = Goal::Escort;
        return true;
    }
    Vec2i to = escorting_.to;
    auto p = search(world, [&](Vec2i c) { return c == to; });
    if (!p) {
        o.escorting_.by = -1;
        escorting_.escortee = -1;
        return false;
    }
    startWalking(Goal::Escort, *p);
    return true;
}

bool Human::followEscort(World& world) {
    if (escorting_.by < 0) return false;
    Human& h = world.human(escorting_.by);
    // Dead out of sight, they are gone after until come upon.
    bool gone = h.dead() && (sight.sees(h.cell) || cheb(cell, h.cell) <= ESCORT_FOLLOW);
    if (gone || h.escorting_.escortee != id || hungry() || fight_.foe >= 0 || fight_.fear > 0.0f) {
        escorting_.by = -1;
        return false;
    }
    if (cheb(cell, h.cell) <= ESCORT_FOLLOW) {
        startIdle(ESCORTED_WAIT_MIN, ESCORTED_WAIT_MAX);
        goal = Goal::Escorted;
        return true;
    }
    Vec2i at = h.cell;
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= ESCORT_FOLLOW; });
    if (!p) {
        escorting_.by = -1;
        return false;
    }
    startWalking(Goal::Escorted, *p);
    return true;
}

// Blood fresher than any by its feet lies further along, toward whoever
// is bleeding. Its own blood leads only back to itself.
bool Human::track(World& world) {
    if (cakes > 0) return false;
    float here = 0.0f;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (const Stain* s = world.blood().at(cell + Vec2i{dx, dy}); s && s->share(id) < OWN_BLOOD)
                here = std::max(here, s->wet);
    std::erase_if(gore_.tracked, [&](const auto& t) { return clock_ - t.second >= TRACK_FORGET; });
    std::optional<Vec2i> best;
    float bestWet = here;
    sight.forEach([&](Vec2i c) {
        const Stain* s = world.blood().at(c);
        if (s && s->wet > bestWet && s->share(id) < OWN_BLOOD && cheb(cell, c) <= TRACK_RANGE && cheb(cell, c) > 1 &&
            !gore_.tracked.count(c)) {
            best = c;
            bestWet = s->wet;
        }
    });
    if (!best) return false;
    Vec2i to = *best;
    auto p = search(world, [&](Vec2i c) { return cheb(c, to) <= 1; });
    if (!p || p->empty()) return false;
    gore_.tracked[to] = clock_;
    startWalking(Goal::Track, *p);
    return true;
}
