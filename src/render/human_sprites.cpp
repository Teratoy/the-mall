#include "render/human_sprites.h"
#include "render/blood_art.h"
#include "render/bottle_art.h"
#include "render/gun_art.h"
#include "render/mess_art.h"
#include "render/remains_art.h"
#include "render/sprites.h"
#include <algorithm>
#include <cmath>

namespace {

/// Pocket Rave's walk speed.
constexpr float WALK_FPS = 8.0f;
/// The run cycle, played quicker than the walk for its quicker steps, and
/// the panting of a human out of breath.
constexpr float RUN_FPS = 14.0f;
constexpr float WINDED_FPS = 4.0f;
/// A greeting's wave is a quick flick of the hand, then they stand and talk.
constexpr float WAVE_FPS = 10.0f, WAVE_TIME = 0.4f;
/// Bites and chews.
constexpr float EAT_FPS = 2.5f;
/// Keys struck a second, typing.
constexpr float TYPE_FPS = 5.0f;
/// Falling dead: still upright until FALL_KNEEL, on the knees until
/// FALL_DOWN, then flat on the back for good.
constexpr float FALL_KNEEL = 0.15f, FALL_DOWN = 0.45f;
/// Seated, a short blink every so often.
constexpr float BLINK_EVERY = 3.2f, BLINK_TIME = 0.15f;
/// Coming to, the eyes stay shut this long, then blink slow and heavy.
constexpr float WAKE_EYES_SHUT = 1.5f, WAKE_BLINK_EVERY = 1.1f, WAKE_BLINK_TIME = 0.3f;
/// Reloading, the seconds one round takes; and the moment in it each frame
/// ends: in the pocket, bringing the round up, at the gun, pressing it in,
/// the hand going back down.
constexpr float RELOAD_ROUND_TIME = 1.2f;
constexpr float RELOAD_STEP_ENDS[RELOAD_FRAMES] = {0.3f, 0.55f, 0.8f, 1.0f, 1.2f};
/// Racking the slide: the moment each frame ends, gripping it, pulling it
/// back, letting go; the last, the hand dropped away, holds on.
constexpr float RACK_STEP_ENDS[RACK_FRAMES] = {0.15f, 0.35f, 0.5f, 0.6f};
/// Shooting, the kick lasts this long after each shot.
constexpr float SHOT_TIME = 0.1f;
/// Feeding, the head dips a pixel this often.
constexpr float FEED_FPS = 3.0f;
/// Nodding off, the head drops, jerks back up, then drops for good.
constexpr float NOD_FIRST = 0.9f, NOD_JERK = 1.5f, NOD_LAST = 2.0f;
/// Asleep, a slow breath in and out, breathing in for part of it.
constexpr float BREATH_TIME = 3.4f, BREATH_IN = 1.5f;
/// Just woken, a slow blink every so often.
constexpr float ROUSE_BLINK_EVERY = 0.7f, ROUSE_BLINK_TIME = 0.2f;
/// Z's rise from a sleeper's head: one every Z_EVERY seconds, each
/// drifting up Z_RISE pixels and on past the head Z_DRIFT over Z_LIFE, swaying Z_SWAY pixels
/// either side, fading in and out.
constexpr float Z_EVERY = 1.3f, Z_LIFE = 2.6f, Z_RISE = 15.0f, Z_DRIFT = 4.0f, Z_SWAY = 1.0f, Z_FADE = 0.3f;
/// Each Z is the sprite "sleeper's z" in data/sprites.txt, this many pixels square.
constexpr int Z_SIZE = 4;
/// The remains are redrawn only every this much of eating and of rot.
constexpr int EATEN_STEPS = 12, ROT_STEPS = 24;

/// A cake carried, its bottom-left on sprite pixel (x, y), drawn so: a pixel
/// being scale screen pixels, origin the screen point of pixel (0, 0).
void drawHeldCake(Vector2 origin, int x, int y, bool flip, int scale, Color tint) {
    for (int dy = 0; dy < HELD_CAKE_H; dy++)
        for (int dx = 0; dx < HELD_CAKE_W; dx++) {
            int px = flip ? SPRITE_W - 1 - (x + dx) : x + dx;
            Rgba c = dy == 0 ? CAKE_BASE : CAKE_TOP;
            DrawRectangle(int(origin.x) + px * scale, int(origin.y) + (y - dy) * scale, scale, scale,
                          {uint8_t(c.r * tint.r / 255), uint8_t(c.g * tint.g / 255), uint8_t(c.b * tint.b / 255),
                           uint8_t(c.a * tint.a / 255)});
        }
}

/// Whether the head hangs this far into nodding off.
bool nodding(float t) { return (t >= NOD_FIRST && t < NOD_JERK) || t >= NOD_LAST; }

Texture2D toTexture(const SpriteFrame& f) {
    // Rgba and raylib's Color are both four bytes in the same order.
    Image img{const_cast<Rgba*>(f.px.data()), SPRITE_W, SPRITE_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

/// Which way a shooting pose aims (GunSpot::aim), or none when it doesn't shoot.
constexpr int NO_AIM = 9;
int shootAim(Pose pose) {
    return pose == Pose::ShootUp          ? -2
           : pose == Pose::ShootUpAhead   ? -1
           : pose == Pose::Shoot          ? 0
           : pose == Pose::ShootDownAhead ? 1
           : pose == Pose::ShootDown      ? 2
                                          : NO_AIM;
}

/// Which reload frame shows this long after starting, a round at a time.
size_t reloadFrame(float t) {
    float into = std::fmod(std::max(t, 0.0f), RELOAD_ROUND_TIME) / RELOAD_ROUND_TIME * RELOAD_STEP_ENDS[RELOAD_FRAMES - 1];
    size_t i = 0;
    while (i + 1 < RELOAD_FRAMES && into >= RELOAD_STEP_ENDS[i]) i++;
    return i;
}

/// Which rack frame shows this long after starting; the last holds on.
size_t rackFrame(float t) {
    size_t i = 0;
    while (i + 1 < RACK_FRAMES && t >= RACK_STEP_ENDS[i]) i++;
    return i;
}

} // namespace

template <class F> void HumanSprites::eachFrame(const HumanLook& look, F&& add) {
    add(idle_, look.idle());
    add(guard_, look.guard());
    add(punch_, look.punch());
    add(offer_, look.offer());
    add(point_, look.point(0));
    add(pointUp_, look.point(-1));
    add(pointDown_, look.point(1));
    add(kneel_, look.kneel());
    add(dead_, look.dead());
    add(crouch_, look.crouch());
    for (int i = 0; i < WALK_FRAMES; i++) add(walk_[size_t(i)], look.walk(i));
    for (int i = 0; i < RUN_FRAMES; i++) add(run_[size_t(i)], look.run(i));
    for (int i = 0; i < WINDED_FRAMES; i++) add(winded_[size_t(i)], look.winded(i));
    for (int i = 0; i < WAVE_FRAMES; i++) add(wave_[size_t(i)], look.wave(i));
    for (int i = 0; i < EAT_FRAMES; i++) add(eat_[size_t(i)], look.eat(i));
    for (int i = 0; i < SIT_FRAMES; i++) add(sit_[size_t(i)], look.sit(i));
    for (int i = 0; i < DOZE_FRAMES; i++) add(doze_[size_t(i)], look.doze(i));
    for (int i = 0; i < CURL_FRAMES; i++) {
        add(curl_[size_t(i)], look.curl(i));
        add(curlCarry_[size_t(i)], look.curl(i, true));
    }
    add(woken_, look.curl(CURL_FRAMES));
    add(wokenCarry_, look.curl(CURL_FRAMES, true));
    add(sip_, look.sip());
    add(fill_, look.fill());
    add(pour_, look.pour());
    add(offerBottle_, look.offerBottle());
    for (int a = 0; a < SHOOT_AIMS; a++)
        for (int fire = 0; fire < 2; fire++) {
            add(shoot_[size_t(a)][size_t(fire)], look.shoot(a - 2, fire == 1));
            if (a >= 1 && a <= 3) add(shootFar_[size_t(a - 1)][size_t(fire)], look.shoot(a - 2, fire == 1, true));
        }
    for (int i = 0; i < RELOAD_FRAMES; i++) {
        add(reload_[size_t(i)], look.reload(i));
        add(reloadFar_[size_t(i)], look.reload(i, true));
    }
    for (int i = 0; i < RACK_FRAMES; i++) {
        add(rack_[size_t(i)], look.rack(i));
        add(rackFar_[size_t(i)], look.rack(i, true));
    }
    for (int i = 0; i < TYPE_FRAMES; i++) add(type_[size_t(i)], look.type(i));
}

HumanSprites::HumanSprites(const HumanLook& look) : look_(look), skin_(look.skin) {
    eachFrame(look, [this](Texture2D& t, SpriteFrame f) { add(t, std::move(f)); });
    // Where the head is and how far the body reaches ahead, curled up and
    // breathing in, cake and all.
    const SpriteFrame curled = look.curl(1, true);
    int headPx = 0, front = 0;
    for (int y = 0; y < SPRITE_H; y++)
        for (int x = 0; x < SPRITE_W; x++) {
            size_t i = size_t(y * SPRITE_W + x);
            if (curled.px[i].a) front = std::max(front, x);
            if (curled.part[i] != uint8_t(BodyPart::Head)) continue;
            curlHead_.x += x + 0.5f;
            curlHead_.y += y + 0.5f;
            headPx++;
        }
    if (headPx) curlHead_ = {curlHead_.x / headPx, curlHead_.y / headPx};
    curlSink_ = SPRITE_W - 1 - front;
}

HumanSprites::~HumanSprites() {
    for (auto& [t, f] : frames_) UnloadTexture(*t);
}

void HumanSprites::add(Texture2D& t, SpriteFrame f) {
    t = toTexture(f);
    spots_[&t] = {f.held, f.hand, f.gun};
    frames_.emplace_back(&t, std::move(f));
}

void HumanSprites::setDirty(bool dirty) {
    if (dirty == look_.dirty) return;
    look_.dirty = dirty;
    // The frames come in the same order as they were added in.
    size_t i = 0;
    eachFrame(look_, [&](Texture2D& t, SpriteFrame f) {
        spots_[&t] = {f.held, f.hand, f.gun};
        frames_[i].second = std::move(f);
        paint(t, frames_[i++].second);
    });
}

void HumanSprites::dirty(const BodyBlood& dirt, const BodyBlood& soaked) {
    if (dirt.version == dirtVersion_ && soaked.version == soakedVersion_) return;
    dirtVersion_ = dirt.version;
    soakedVersion_ = soaked.version;
    dirt_ = &dirt;
    soaked_ = &soaked;
    for (auto& [t, f] : frames_) paint(*t, f);
}

void HumanSprites::bloody(const BodyBlood& blood) {
    if (blood.version == bloodVersion_) return;
    bloodVersion_ = blood.version;
    blood_ = &blood;
    for (auto& [t, f] : frames_) paint(*t, f);
}

void HumanSprites::remains(float eaten, float decay, uint64_t seed, bool curled) {
    int e = int(std::ceil(eaten * EATEN_STEPS)), r = int(decay * ROT_STEPS);
    if (e == eatenStep_ && r == rotStep_ && curled == curledDead_) return;
    eatenStep_ = e;
    rotStep_ = r;
    seed_ = seed;
    curledDead_ = curled;
    for (auto& [t, f] : frames_)
        if (showsRemains(t)) paint(*t, f);
}

bool HumanSprites::showsRemains(const Texture2D* t) const { return t == &dead_ || (curledDead_ && t == &curl_[0]); }

// Each pixel takes the blood on the bit of body it shows, blended over it
// as thickly as on the floor.
void HumanSprites::paint(Texture2D& t, const SpriteFrame& f) {
    std::array<Rgba, SPRITE_PX> px = f.px;
    if (blood_)
        for (size_t i = 0; i < px.size(); i++) {
            int r = f.rest[i];
            if (r < 0 || !blood_->amount[size_t(r)]) continue;
            Color c = bloodColor(blood_->wet[size_t(r)]);
            float a = bloodAlpha(blood_->amount[size_t(r)]) / 255.0f;
            px[i].r = uint8_t(px[i].r + (c.r - px[i].r) * a);
            px[i].g = uint8_t(px[i].g + (c.g - px[i].g) * a);
            px[i].b = uint8_t(px[i].b + (c.b - px[i].b) * a);
        }
    // Piss under feces; piss stains thinner.
    for (const BodyBlood* layer : {soaked_, dirt_})
        for (size_t i = 0; layer && i < px.size(); i++) {
            int r = f.rest[i];
            if (r < 0 || !layer->amount[size_t(r)]) continue;
            bool piss = layer == soaked_;
            Color c = piss ? pissColor(layer->wet[size_t(r)]) : fecesColor(layer->wet[size_t(r)]);
            float a = bloodAlpha(layer->amount[size_t(r)]) / 255.0f * (piss ? 0.6f : 1.0f);
            px[i].r = uint8_t(px[i].r + (c.r - px[i].r) * a);
            px[i].g = uint8_t(px[i].g + (c.g - px[i].g) * a);
            px[i].b = uint8_t(px[i].b + (c.b - px[i].b) * a);
        }
    // frames_ starts with the standing frame every other maps back to.
    if (showsRemains(&t))
        paintRemains(px, f, frames_.front().second, skin_, float(eatenStep_) / EATEN_STEPS,
                     float(rotStep_) / ROT_STEPS, seed_);
    UpdateTexture(t, px.data());
}

// The walk spreads its legs on frames 1 and 3, the run on 0 and 2.
int footfalls(Pose pose, float animTime) {
    if (pose == Pose::Walk) return (int(animTime * WALK_FPS) + 1) / 2;
    if (pose == Pose::Run) return int(animTime * RUN_FPS) / 2 + 1;
    return -1;
}

void HumanSprites::draw(Vector2 feet, Pose pose, float animTime, bool flip, int scale, Color tint,
                        int cakes, const Bottle* bottle, bool inHand) const {
    bool carrying = cakes > 0;
    // Shooting straight up or south faces no way, and mirrored it would
    // shoot with the other hand.
    if (pose == Pose::ShootUp || pose == Pose::ShootDown) flip = false;
    // Mirroring keeps the base pixel in place: it is the middle column.
    Rectangle src{0, 0, flip ? -float(SPRITE_W) : float(SPRITE_W), float(SPRITE_H)};
    bool lying = (pose == Pose::Fall && animTime >= FALL_DOWN) || pose == Pose::Wake || pose == Pose::Lie;
    if (lying) {
        // Turned a quarter about the sprite's middle so the head falls back,
        // then set down with the shoulders on the bottom of the feet's cell.
        // Lying awake, it blinks as one seated does.
        bool shut = pose == Pose::Lie ? std::fmod(animTime, BLINK_EVERY) >= BLINK_EVERY - BLINK_TIME
                    : pose == Pose::Fall || animTime < WAKE_EYES_SHUT ||
                        std::fmod(animTime - WAKE_EYES_SHUT, WAKE_BLINK_EVERY) >= WAKE_BLINK_EVERY - WAKE_BLINK_TIME;
        float lift = SPRITE_W * scale / 2.0f;
        Vector2 mid{std::round(feet.x), std::round(feet.y) - lift};
        DrawTexturePro(shut ? dead_ : idle_, src, {mid.x, mid.y, float(SPRITE_W * scale), float(SPRITE_H * scale)},
                       {lift, SPRITE_H * scale / 2.0f}, flip ? 90.0f : -90.0f, tint);
        return;
    }
    if (pose == Pose::DiedAsleep) return drawCurled(feet, curl_[0], flip, scale, tint, -1.0f);
    if (pose == Pose::Sleep && animTime >= LIE_DOWN_TIME) {
        float t = animTime - LIE_DOWN_TIME;
        const auto& curl = carrying ? curlCarry_ : curl_;
        drawCurled(feet, curl[std::fmod(t, BREATH_TIME) < BREATH_IN ? 1 : 0], flip, scale, tint, t);
        return;
    }
    if (pose == Pose::WakeUp && animTime < ROUSE_TIME) {
        bool blink = std::fmod(animTime, ROUSE_BLINK_EVERY) >= ROUSE_BLINK_EVERY - ROUSE_BLINK_TIME;
        const Texture2D& tex = blink ? (carrying ? curlCarry_ : curl_)[0] : carrying ? wokenCarry_ : woken_;
        drawCurled(feet, tex, flip, scale, tint, -1.0f);
        return;
    }
    const int aim = shootAim(pose);
    const bool shooting = aim != NO_AIM;
    const Texture2D& seated = sit_[std::fmod(animTime, BLINK_EVERY) < BLINK_EVERY - BLINK_TIME ? 0 : 1];
    const Texture2D& tex = pose == Pose::Walk  ? walk_[size_t(int(animTime * WALK_FPS) % WALK_FRAMES)]
                           : pose == Pose::Run ? run_[size_t(int(animTime * RUN_FPS) % RUN_FRAMES)]
                           : pose == Pose::Winded ? winded_[size_t(int(animTime * WINDED_FPS) % WINDED_FRAMES)]
                           : pose == Pose::Wave && animTime < WAVE_TIME ? wave_[size_t(int(animTime * WAVE_FPS) % WAVE_FRAMES)]
                           : pose == Pose::Eat  ? eat_[size_t(int(animTime * EAT_FPS) % EAT_FRAMES)]
                           : pose == Pose::Guard ? guard_
                           : pose == Pose::Punch ? punch_
                           : pose == Pose::Offer ? offer_
                           : pose == Pose::Point ? point_
                           : pose == Pose::Type ? type_[size_t(int(animTime * TYPE_FPS) % TYPE_FRAMES)]
                           : pose == Pose::PointUp ? pointUp_
                           : pose == Pose::PointDown ? pointDown_
                           : pose == Pose::Sip   ? sip_
                           : pose == Pose::Fill  ? fill_
                           : pose == Pose::Pour  ? pour_
                           : pose == Pose::OfferBottle ? offerBottle_
                           : pose == Pose::Reload ? (flip ? reloadFar_ : reload_)[reloadFrame(animTime)]
                           : pose == Pose::Rack ? (flip ? rackFar_ : rack_)[rackFrame(animTime)]
                           : shooting  ? (flip ? shootFar_[size_t(aim + 1)] : shoot_[size_t(aim + 2)])
                                                 [animTime < SHOT_TIME ? 1 : 0]
                           : pose == Pose::Fall  ? (animTime < FALL_KNEEL ? idle_ : kneel_)
                           : pose == Pose::Sit   ? (animTime < SIT_DOWN_TIME ? crouch_ : seated)
                           : pose == Pose::Rise && animTime < STAND_UP_TIME ? crouch_
                           : pose == Pose::Sleep ? (animTime < SIT_DOWN_TIME ? crouch_
                                                    : doze_[nodding(animTime) ? 1 : 0])
                           : pose == Pose::WakeUp && animTime < ROUSE_TIME + STAND_UP_TIME ? crouch_
                           : pose == Pose::Wash || pose == Pose::Search || pose == Pose::Feed || pose == Pose::Squat
                               ? crouch_
                               : idle_;
    float x = std::round(feet.x) - (SPRITE_BASE_X + 0.5f) * scale;
    float y = std::round(feet.y) - SPRITE_H * scale;
    // Feeding, it dips down to the body and comes up chewing.
    if (pose == Pose::Feed) y += float(int(animTime * FEED_FPS) % 2 * scale);
    DrawTexturePro(tex, src, {std::round(x), y, float(SPRITE_W * scale), float(SPRITE_H * scale)},
                   {0, 0}, 0.0f, tint);
    auto it = spots_.find(&tex);
    if (it == spots_.end()) return;
    auto [held, hand, gun] = it->second;
    Vector2 origin{std::round(x), y};
    drawHeldGun(origin, gun, flip, scale, tint);
    Vector2 port, side;
    float drop;
    if (casingOut(feet, pose, flip, scale, port, side, drop)) drawEjectedCasing(port, side, drop, animTime, scale, tint);
    // The cake being eaten is in the front hand, not on the pile.
    int piled = held.valid() ? std::max(0, cakes - (pose == Pose::Eat ? 1 : 0)) : 0;
    for (int i = 0; i < piled; i++) drawHeldCake(origin, held.x, held.y - i * HELD_CAKE_H, flip, scale, tint);
    if (!bottle) return;
    bool onPile = !inHand || !hand.valid();
    BottleSpot at = onPile ? held : hand;
    if (!at.valid()) return;
    if (onPile) at.y = int8_t(at.y - piled * HELD_CAKE_H);
    drawBottle(origin, at.x, at.y, at.turn, flip, *bottle, scale, tint);
    if (pose == Pose::Pour) drawPourStream(origin, at.x, at.y, SPRITE_H - 1, flip, *bottle, scale, tint, animTime);
}

bool HumanSprites::casingOut(Vector2 feet, Pose pose, bool flip, int scale, Vector2& port, Vector2& side,
                             float& drop) const {
    const int aim = shootAim(pose);
    if (aim == NO_AIM) return false;
    if (aim == -2 || aim == 2) flip = false;
    // From the frame of the shot, so it leaves from where the gun went off.
    const Texture2D& fired = flip ? shootFar_[size_t(aim + 1)][1] : shoot_[size_t(aim + 2)][1];
    GunSpot gun = spots_.at(&fired).gun;
    // Out of the slide, just over the hand.
    int px = flip ? SPRITE_W - 1 - gun.x : gun.x, py = gun.y - 1;
    float x = std::round(std::round(feet.x) - (SPRITE_BASE_X + 0.5f) * scale);
    float y = std::round(feet.y) - SPRITE_H * scale;
    port = {x + float(px * scale), y + float(py * scale)};
    drop = float(SPRITE_H - 1 - py);
    // Out to the right of the way the gun points, screen y down.
    float ax = aim == -2 || aim == 2 ? 0.0f : flip ? -1.0f : 1.0f;
    float ay = aim < 0 ? -1.0f : aim > 0 ? 1.0f : 0.0f;
    float len = std::sqrt(ax * ax + ay * ay);
    side = {-ay / len, ax / len};
    return true;
}

bool HumanSprites::casingRest(Vector2 feet, Pose pose, bool flip, int scale, Vector2& rest) const {
    Vector2 port, side;
    float drop;
    if (!casingOut(feet, pose, flip, scale, port, side, drop)) return false;
    rest = ejectedCasingRest(port, side, drop, scale);
    return true;
}

void HumanSprites::drawCurled(Vector2 feet, const Texture2D& tex, bool flip, int scale, Color tint,
                              float sleepTime) const {
    // Turned a quarter the other way from the dead, so the front, knees
    // drawn up, faces down, the body sunk onto the bottom of the feet's cell.
    Rectangle src{0, 0, flip ? -float(SPRITE_W) : float(SPRITE_W), float(SPRITE_H)};
    float lift = SPRITE_W * scale / 2.0f;
    Vector2 mid{std::round(feet.x), std::round(feet.y) - lift + float(curlSink_ * scale)};
    DrawTexturePro(tex, src, {mid.x, mid.y, float(SPRITE_W * scale), float(SPRITE_H * scale)},
                   {lift, SPRITE_H * scale / 2.0f}, flip ? -90.0f : 90.0f, tint);
    if (sleepTime < 0) return;
    // The head lies off the middle along the body, towards the way it faces.
    float along = SPRITE_H / 2.0f - curlHead_.y, across = curlHead_.x - SPRITE_W / 2.0f;
    float ahead = flip ? -1.0f : 1.0f;
    Vector2 head{mid.x + ahead * along * scale, mid.y + across * scale};
    const Sprite& zSprite = sprites::get("sleeper's z", Z_SIZE, Z_SIZE);
    for (int n = 0; n * Z_EVERY < Z_LIFE; n++) {
        float age = std::fmod(sleepTime, Z_EVERY) + n * Z_EVERY;
        if (age > sleepTime || age >= Z_LIFE) continue;
        float k = age / Z_LIFE;
        float fade = std::min({1.0f, k / Z_FADE, (1.0f - k) / Z_FADE});
        float x = head.x + ahead * (1.0f + k * Z_DRIFT + std::sin(k * 6.2832f) * Z_SWAY) * scale;
        float y = head.y - (5.0f + k * Z_RISE) * scale;
        int zx = int(std::round(x)) - Z_SIZE / 2 * scale, zy = int(std::round(y)) - Z_SIZE / 2 * scale;
        for (int gy = 0; gy < Z_SIZE; gy++)
            for (int gx = 0; gx < Z_SIZE; gx++) {
                Rgba z = zSprite.pixel(gx, gy);
                if (!z.solid()) continue;
                Color c{uint8_t(z.r * tint.r / 255), uint8_t(z.g * tint.g / 255), uint8_t(z.b * tint.b / 255),
                        uint8_t(z.a * fade * tint.a / 255)};
                DrawRectangle(zx + gx * scale, zy + gy * scale, scale, scale, c);
            }
    }
}
