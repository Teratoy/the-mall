#include "human/human_look.h"
#include "core/vec.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace {

constexpr int CX = SPRITE_BASE_X;
constexpr Rgba EYES = hex(0x141414);

constexpr uint32_t SKINS[] = {0xf3d2b5, 0xe6b48e, 0xcf9868, 0xa86a44, 0x7b4b2d, 0x4e301f};
constexpr uint32_t HAIRS[] = {0x1b1411, 0x3a2416, 0x6a4327, 0xb88a4c,
                              0xe2c47c, 0xa5471d, 0x8a8a8a, 0xdddddd};
/// Chance of hair dyed a random colour instead of a natural one.
constexpr float DYED_HAIR_CHANCE = 0.12f;
/// Chance a piece of clothing is black, white or grey rather than coloured.
constexpr float PLAIN_CLOTH_CHANCE = 0.25f;
constexpr float WHITE_SHOES_CHANCE = 0.2f;
constexpr float BEARD_CHANCE = 0.2f;
/// How much colour every human keeps: 0 is greyscale, 1 the full palette.
constexpr float COLOUR = 0.2f;

/// A dirty human's hair and clothes go dull with dirt, its shoes scuffed.
constexpr float DIRTY_HAIR_DIRT = 0.88f, DIRTY_CLOTH_DIRT = 0.86f, DIRTY_SHOE_DIRT = 0.8f;
/// Smudges of dirt on its skin, and its hands black with it, are the skin
/// in this much shade: of every hundred pixels of skin, this many smudged.
constexpr float DIRTY_SKIN = 0.78f;
constexpr int DIRTY_SMUDGED = 22;
/// Frayed cloth is darker still, by this much.
constexpr float DIRTY_FRAY_DARK = 0.75f;
/// Of every hundred pixels of cloth at a hem (the shirt's bottom, the end
/// of a sleeve or a trouser leg), this many are torn away to the skin; of
/// every hundred elsewhere, this many. Of every hundred, this many more are
/// frayed darker still.
constexpr int DIRTY_HEM_TORN = 45, DIRTY_TORN = 6, DIRTY_FRAYED = 12;
/// A gun tucked into the waistband: its grip, and the back of its slide.
constexpr Rgba GRIP = hex(0x7a6048), SLIDE = hex(0x8a8e98);
/// How far a pixel may be from a colour in shade and still count as it.
constexpr int SHADE_SLACK = 4;

/// A frame being drawn; pixels off the frame are dropped. Each pixel set
/// belongs to the body part being drawn, at the pixel of the standing pose
/// that to maps it to; or to nothing after off().
struct Paper {
    SpriteFrame f;
    uint8_t part = NO_PART;
    std::function<Vec2i(int, int)> to;

    void set(int x, int y, Rgba c) {
        if (x < 0 || y < 0 || x >= SPRITE_W || y >= SPRITE_H) return;
        size_t i = size_t(y * SPRITE_W + x);
        f.px[i] = c;
        f.rest[i] = -1;
        f.part[i] = NO_PART;
        if (part == NO_PART) return;
        Vec2i r = to(x, y);
        if (r.x < 0 || r.y < 0 || r.x >= SPRITE_W || r.y >= SPRITE_H) return;
        f.rest[i] = int16_t(r.y * SPRITE_W + r.x);
        f.part[i] = part;
    }
    /// What is drawn next is this part of the body, at to(x, y) when standing.
    void body(BodyPart p, std::function<Vec2i(int, int)> m) {
        part = uint8_t(p);
        to = std::move(m);
    }
    void off() { part = NO_PART; }
    void row(int x0, int x1, int y, Rgba c) {
        for (int x = x0; x <= x1; x++) set(x, y, c);
    }
};

Rgba cloth(Rng& rng) {
    if (rng.chance(PLAIN_CLOTH_CHANCE)) {
        uint8_t v = uint8_t(rng.range(25, 230));
        return {v, v, v, 255};
    }
    return hsv(rng.unit(), rng.range(0.25f, 0.85f), rng.range(0.3f, 0.9f));
}

/// Fades c towards the grey of the same brightness.
Rgba mute(Rgba c) {
    float grey = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
    auto f = [grey](uint8_t v) { return uint8_t(std::lround(grey + (v - grey) * COLOUR)); };
    return {f(c.r), f(c.g), f(c.b), c.a};
}

Rgba shade(Rgba c, float k) {
    return {uint8_t(c.r * k), uint8_t(c.g * k), uint8_t(c.b * k), c.a};
}

/// Whether p is c in some shade: the shades frames are drawn with run from
/// half to full.
bool shadeOf(Rgba p, Rgba c) {
    int sum = c.r + c.g + c.b;
    if (!p.a || sum == 0) return false;
    float k = float(p.r + p.g + p.b) / float(sum);
    if (k < 0.5f || k > 1.02f) return false;
    return std::abs(p.r - c.r * k) <= SHADE_SLACK && std::abs(p.g - c.g * k) <= SHADE_SLACK &&
           std::abs(p.b - c.b * k) <= SHADE_SLACK;
}

/// A die for one pixel of the standing pose, the same in every frame, so a
/// tear stays where it is on the body: 0 to 99.
int pixelDie(int rest, uint32_t salt) {
    uint32_t h = uint32_t(rest) * 0x9e3779b1u ^ salt;
    h ^= h >> 15;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    return int(h % 100);
}

} // namespace

HumanLook HumanLook::random(Rng& rng) {
    HumanLook l;
    l.skin = hex(rng.pick(SKINS));
    l.hair = rng.chance(DYED_HAIR_CHANCE) ? hsv(rng.unit(), 0.7f, 0.85f) : hex(rng.pick(HAIRS));
    l.top = cloth(rng);
    l.bottom = cloth(rng);
    l.hat = cloth(rng);
    l.shoes = rng.chance(WHITE_SHOES_CHANCE) ? hex(0xe8e8e8)
                                             : hsv(rng.unit(), 0.3f, rng.range(0.1f, 0.35f));
    for (Rgba* c : {&l.skin, &l.hair, &l.top, &l.bottom, &l.hat, &l.shoes}) *c = mute(*c);
    l.hairStyle = Hair(rng.range(0, int(Hair::Cap)));
    l.topStyle = Top(rng.range(0, int(Top::Tank)));
    l.bottomStyle = Bottom(rng.range(0, int(Bottom::Skirt)));
    l.torsoW = rng.chance(0.5f) ? 5 : 7;
    l.torsoH = rng.range(4, 5);
    l.legH = rng.range(3, 5);
    l.beard = rng.chance(BEARD_CHANCE);
    return l;
}

SpriteFrame HumanLook::idle() const { return draw({}); }
SpriteFrame HumanLook::draw(const Stance& s) const {
    if (!dirty) return drawBody(s);
    // The same body gone dull with dirt; drawBody messes up its hair.
    HumanLook l = *this;
    l.hair = shade(hair, DIRTY_HAIR_DIRT);
    l.top = shade(top, DIRTY_CLOTH_DIRT);
    l.bottom = shade(bottom, DIRTY_CLOTH_DIRT);
    l.shoes = shade(shoes, DIRTY_SHOE_DIRT);
    l.hat = shade(hat, DIRTY_CLOTH_DIRT);
    l.gunDirt_ = DIRTY_CLOTH_DIRT;
    SpriteFrame f = l.drawBody(s);
    l.wear(f, l.drawBody({}));
    return f;
}

void HumanLook::wear(SpriteFrame& f, const SpriteFrame& stand) const {
    // The standing pose's row of the hands.
    const int restHand = SPRITE_H - legH;
    const uint32_t salt = uint32_t(top.r) << 24 ^ uint32_t(bottom.g) << 16 ^ uint32_t(skin.b) << 8 ^ uint32_t(hair.r);
    auto cloth = [&](Rgba p) { return (shadeOf(p, top) || shadeOf(p, bottom)) && !shadeOf(p, skin); };
    // The tucked gun is not cloth: it neither tears nor frays.
    auto gun = [&](Rgba p) {
        if (!tuckedGun) return false;
        for (Rgba c : {GRIP, SLIDE})
            if (Rgba g = shade(c, gunDirt_); p.r == g.r && p.g == g.g && p.b == g.b) return true;
        return false;
    };
    for (size_t i = 0; i < f.px.size(); i++) {
        int r = f.rest[i];
        if (r < 0) continue;
        int ry = r / SPRITE_W;
        auto part = BodyPart(f.part[i]);
        Rgba& p = f.px[i];
        // Dirt on the skin: the hands black with it, smudges elsewhere.
        if (shadeOf(p, skin)) {
            bool hand = (part == BodyPart::FrontArm || part == BodyPart::BackArm) && ry == restHand;
            if (hand || pixelDie(r, ~salt) < DIRTY_SMUDGED) p = shade(p, DIRTY_SKIN);
            continue;
        }
        if (part == BodyPart::Head || part == BodyPart::Feet || !cloth(p) || gun(p)) continue;
        // A hem: standing, the cloth stops just below.
        Rgba here = stand.px[size_t(r)];
        Rgba below = ry + 1 < SPRITE_H ? stand.px[size_t(r + SPRITE_W)] : Rgba{};
        bool hem = !cloth(below) || !(shadeOf(below, here) && shadeOf(here, below));
        int die = pixelDie(r, salt), torn = hem ? DIRTY_HEM_TORN : DIRTY_TORN;
        if (die < torn)
            p = skin;
        else if (die < torn + DIRTY_FRAYED)
            p = shade(p, DIRTY_FRAY_DARK);
    }
}


SpriteFrame HumanLook::walk(int frame) const { return draw({.stride = frame == 1 ? 1 : frame == 3 ? -1 : 0}); }

SpriteFrame HumanLook::run(int frame) const {
    constexpr int STRIDES[RUN_FRAMES] = {2, 1, -2, -1};
    return draw({.stride = STRIDES[frame], .lean = 1, .run = true});
}

SpriteFrame HumanLook::winded(int frame) const {
    return draw({.breathe = frame == 1, .nod = 1, .lean = 1, .bend = true});
}

SpriteFrame HumanLook::wave(int frame) const { return draw({.front = frame == 0 ? Arm::WaveIn : Arm::WaveOut}); }

SpriteFrame HumanLook::guard() const { return draw({.front = Arm::Guard}); }

SpriteFrame HumanLook::punch() const { return draw({.stride = 1, .front = Arm::Punch}); }

SpriteFrame HumanLook::eat(int frame) const { return draw({.front = frame == 0 ? Arm::Bite : Arm::Chew}); }

SpriteFrame HumanLook::offer() const { return draw({.front = Arm::Offer}); }

SpriteFrame HumanLook::point(int tilt) const {
    return draw({.front = tilt < 0 ? Arm::PointUp : tilt > 0 ? Arm::PointDown : Arm::Point});
}

SpriteFrame HumanLook::kneel() const { return draw({.kneel = true, .eyesShut = true}); }

SpriteFrame HumanLook::dead() const { return draw({.eyesShut = true}); }

SpriteFrame HumanLook::crouch() const { return draw({.kneel = true}); }

SpriteFrame HumanLook::sit(int frame) const { return draw({.sit = true, .eyesShut = frame == 1}); }

SpriteFrame HumanLook::doze(int frame) const { return draw({.sit = true, .nod = frame, .eyesShut = true}); }

SpriteFrame HumanLook::curl(int frame, bool carrying) const {
    return draw({.front = Arm::Pillow, .curl = true, .breathe = frame == 1, .carry = carrying, .eyesShut = frame != 2});
}

SpriteFrame HumanLook::sip() const { return draw({.front = Arm::Sip}); }

SpriteFrame HumanLook::fill() const { return draw({.front = Arm::Fill, .kneel = true}); }

SpriteFrame HumanLook::pour() const { return draw({.front = Arm::Pour}); }

SpriteFrame HumanLook::offerBottle() const { return draw({.front = Arm::OfferBottle}); }

// Built from the feet up, so short legs and torsos leave empty rows on top.
SpriteFrame HumanLook::drawBody(const Stance& s) const {
    Paper p;
    const int stride = s.stride;
    // Kneeling folds the legs, lowering everything above them; sitting
    // lays them along the floor, two rows thick, under the hips. Curled,
    // they take a row more, bent at the knee.
    const int legs = s.sit    ? 2
                     : s.curl ? legH + 1
                     : s.kneel ? std::max(1, legH - 2)
                     : s.bend  ? std::max(1, legH - 1)
                               : legH;
    const int feet = SPRITE_H - 1;
    const int legTop = feet - legs + 1;
    const int torsoBottom = legTop - 1;
    const int torsoTop = torsoBottom - torsoH + 1;
    const int neck = torsoTop - 1;
    const int headBottom = neck - 1;
    const int headTop = headBottom - 4;
    const int tl = CX - torsoW / 2;
    const int tr = CX + torsoW / 2;
    // First leg row below the shorts.
    const int shortsEnd = legTop + std::max(1, (legs - 1) / 2);
    // The same rows standing, which every pixel is mapped back to.
    const int restLegTop = feet - legH + 1;
    const int restTorsoTop = restLegTop - torsoH;
    const int restShoulder = restTorsoTop + 1;
    const int restHand = restLegTop;
    const int up = restTorsoTop - torsoTop;
    auto lifted = [up](int x, int y) { return Vec2i{x, y + up}; };

    // Seated, the near leg hides the far one: out ahead from under the
    // torso, the knee a little raised, the shoe standing up at the end.
    // Shorts stop at the knee.
    const int toe = std::min(SPRITE_W - 1, tr + legH + 1);
    const int knee = (tr + 1 + toe) / 2;
    if (s.sit) {
        // Along the leg from the hip is down it from the hip standing.
        const int length = std::max(1, toe - (CX - 2));
        p.body(BodyPart::Legs, [&, length](int x, int y) {
            int along = std::clamp(x - (CX - 2), 0, length - 1);
            return Vec2i{CX + 1 + (y == feet ? 1 : 0), restLegTop + along * std::max(1, legH - 1) / length};
        });
        for (int x = CX - 2; x < toe; x++) {
            Rgba c = bottomStyle == Bottom::Pants                    ? bottom
                     : bottomStyle == Bottom::Shorts && x < knee     ? bottom
                                                                     : skin;
            for (int y = legTop; y <= feet; y++) p.set(x, y, c);
            if (x == knee || x == knee - 1) p.set(x, legTop - 1, x < knee || bottomStyle == Bottom::Pants ? c : skin);
        }
        p.body(BodyPart::Feet, [&](int, int y) { return Vec2i{CX + 1 + (y & 1), feet}; });
        for (int y = feet - 2; y <= feet; y++) p.set(toe, y, shoes);
    }

    // Curled, the knees drawn up: each leg bends forward to the knee and
    // back down to the foot, the far one a step behind and in shade.
    const int bentKnee = legTop + (legs - 1) / 2;
    auto bend = [&](int y) { return y <= bentKnee ? y - legTop : 2 * bentKnee - legTop - y; };
    for (int leg = 0; leg < 2 && s.curl; leg++) {
        int lx = leg == 0 ? CX - 1 : CX;
        float k = leg == 0 ? 0.8f : 1.0f;
        for (int y = legTop; y <= feet; y++) {
            int sx = bend(y);
            p.body(y == feet ? BodyPart::Feet : BodyPart::Legs,
                   [&, sx](int x, int y) { return Vec2i{x - sx + (lx == CX ? 1 : -1), y}; });
            Rgba c = y == feet                                          ? shoes
                     : bottomStyle == Bottom::Pants                     ? bottom
                     : bottomStyle == Bottom::Shorts && y < shortsEnd   ? bottom
                                                                        : skin;
            p.row(lx + sx, lx + 1 + sx, y, shade(c, k));
        }
        p.set(lx + 2 + bend(feet), feet, shade(shoes, k));
    }

    // Legs; the lower two rows swing with the stride, toes point ahead.
    for (int leg = 0; leg < 2 && !s.sit && !s.curl; leg++) {
        int lx = leg == 0 ? CX - 2 : CX + 1;
        int swing = leg == 0 ? -stride : stride;
        for (int y = legTop; y <= feet; y++) {
            int sx = y >= feet - 1 ? swing : 0;
            // Kneeling folds the legs from the top: feet stay on the bottom row.
            p.body(y == feet ? BodyPart::Feet : BodyPart::Legs, [sx](int x, int y) { return Vec2i{x - sx, y}; });
            Rgba c = y == feet                                          ? shoes
                     : bottomStyle == Bottom::Pants                     ? bottom
                     : bottomStyle == Bottom::Shorts && y < shortsEnd   ? bottom
                                                                        : skin;
            p.row(lx + sx, lx + 1 + sx, y, c);
        }
        p.set(lx + 2 + swing, feet, shoes);
    }

    if (bottomStyle == Bottom::Skirt && s.curl) {
        // Draped over the hips and the top of the thighs.
        p.body(BodyPart::Legs, [&](int x, int y) { return Vec2i{std::clamp(x, tl, tr), restLegTop + y - legTop}; });
        p.row(tl, tr, legTop, bottom);
        p.row(tl + 1, tr + 2, legTop + 1, bottom);
    } else if (bottomStyle == Bottom::Skirt && s.sit) {
        // Spread over the lap and the floor behind.
        p.body(BodyPart::Legs, [&](int x, int y) {
            return Vec2i{std::clamp(x, tl - 1, tr + 1), restLegTop + (y == feet ? 1 : 0)};
        });
        p.row(tl, tr + 1, legTop, bottom);
        p.row(tl - 1, tr + 2, feet, bottom);
    } else if (bottomStyle == Bottom::Skirt) {
        int rows = std::min(3, legs - 1);
        p.body(BodyPart::Legs, [&](int x, int y) { return Vec2i{x, y - legTop + restLegTop}; });
        for (int i = 0; i < rows; i++)
            p.row(tl - (i + 1) / 2, tr + (i + 1) / 2, legTop + i, bottom);
    }

    // Torso with rounded shoulders; tall torsos show the waistband.
    p.body(BodyPart::Torso, lifted);
    for (int y = torsoTop; y <= torsoBottom; y++) {
        bool waist = y == torsoBottom && torsoH >= 5;
        int inset = y == torsoTop ? 1 : 0;
        p.row(tl + inset, tr - inset, y, waist ? bottom : top);
    }
    // Breathing in, the back swells out a pixel.
    if (s.breathe) {
        p.body(BodyPart::Torso, [&](int, int y) { return Vec2i{tl, y + up}; });
        for (int y = torsoTop + 1; y < torsoBottom; y++) p.set(tl - 1, y, top);
    }

    // A gun tucked into the waistband behind the hip: the grip sticks up
    // and back out of it, the slide's end beside it; the rest is hidden in
    // the clothes. It moves with the torso, so blood lands on it as on the body.
    if (tuckedGun) {
        p.body(BodyPart::Torso, lifted);
        p.set(CX - 2, torsoBottom - 2, shade(GRIP, gunDirt_));
        p.set(CX - 2, torsoBottom - 1, shade(GRIP, gunDirt_));
        p.set(CX - 1, torsoBottom - 1, shade(SLIDE, gunDirt_));
    }

    // An arm pixel, counted from the shoulder: tees cover the first two.
    auto arm = [&](int fromShoulder) {
        return topStyle == Top::LongSleeve                    ? top
               : topStyle == Top::Tee && fromShoulder < 2     ? top
                                                              : skin;
    };
    // An arm pixel i from the shoulder, or its hand, on the back arm (side
    // 0) or the front one, mapped to the arm hanging straight down.
    auto armPx = [&](int side, int x, int y, int i, bool hand) {
        int rx = side == 0 ? tl - 1 : tr + 1;
        int ry = hand ? restHand : std::min(restShoulder + i, restHand - 1);
        p.body(side == 0 ? BodyPart::BackArm : BodyPart::FrontArm, [rx, ry](int, int) { return Vec2i{rx, ry}; });
        p.set(x, y, hand ? skin : arm(i));
    };
    auto cake = [&](int x, int y) {
        p.off();
        p.row(x, x + 1, y, CAKE_TOP);
        p.row(x, x + 1, y + 1, CAKE_BASE);
    };

    // Arms swing against the leg on their side: the one swung ahead looks shorter.
    // The front arm may be busy instead: waving beside the face, the hand
    // rocking out and back; up in a guard or thrown in a punch; or bringing
    // a cake to the mouth.
    for (int side = 0; side < 2; side++) {
        int x = side == 0 ? tl - 1 : tr + 1;
        int shoulder = torsoTop + 1;
        if (side == 1 && s.front != Arm::Swing) {
            switch (s.front) {
            case Arm::WaveIn:
            case Arm::WaveOut: {
                int hand = torsoTop - 3;
                for (int y = hand; y <= shoulder; y++)
                    armPx(side, y == hand && s.front == Arm::WaveOut ? x + 1 : x, y, shoulder - y, y == hand);
                break;
            }
            case Arm::Guard:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x, shoulder + 1, 1, false);
                armPx(side, x + 1, shoulder, 2, true);
                break;
            case Arm::Punch:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder, 1, false);
                armPx(side, x + 2, shoulder, 2, true);
                break;
            case Arm::Bite:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x, shoulder - 1, 1, false);
                armPx(side, x, shoulder - 2, 2, true);
                cake(CX + 3, shoulder - 4);
                break;
            case Arm::Chew:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x, shoulder + 1, 1, false);
                armPx(side, x + 1, shoulder + 1, 2, true);
                cake(x + 1, shoulder - 1);
                break;
            case Arm::Offer:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder, 1, false);
                armPx(side, x + 2, shoulder, 2, true);
                cake(x + 2, shoulder - 2);
                break;
            case Arm::Point:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder, 1, false);
                armPx(side, x + 2, shoulder, 2, false);
                armPx(side, x + 3, shoulder, 3, true);
                break;
            case Arm::PointUp:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder - 1, 1, false);
                armPx(side, x + 2, shoulder - 2, 2, false);
                armPx(side, x + 2, shoulder - 3, 3, true);
                break;
            case Arm::PointDown:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder + 1, 1, false);
                armPx(side, x + 2, shoulder + 2, 2, false);
                armPx(side, x + 3, shoulder + 3, 3, true);
                break;
            case Arm::Pillow:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder - 1, 1, false);
                armPx(side, x + 1, shoulder - 2, 2, true);
                if (s.carry) cake(x + 1, shoulder);
                break;
            case Arm::Sip:
                // Up to the face, the bottle on its side above the hand,
                // its neck at the mouth.
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder - 1, 1, false);
                armPx(side, x + 1, shoulder - 2, 2, true);
                p.f.hand = {int8_t(CX + 6), int8_t(shoulder - 3), BottleTurn::Back};
                break;
            case Arm::Fill:
                // Reaching down ahead, the bottle in the water.
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder + 1, 1, false);
                armPx(side, x + 2, shoulder + 2, 2, true);
                p.f.hand = {int8_t(x + 3), int8_t(shoulder + 2), BottleTurn::Ahead};
                break;
            case Arm::Pour:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder, 1, false);
                armPx(side, x + 2, shoulder, 2, true);
                p.f.hand = {int8_t(x + 2), int8_t(shoulder - 2), BottleTurn::Down};
                break;
            case Arm::OfferBottle:
                armPx(side, x, shoulder, 0, false);
                armPx(side, x + 1, shoulder, 1, false);
                armPx(side, x + 2, shoulder, 2, true);
                p.f.hand = {int8_t(x + 2), int8_t(shoulder - 1), BottleTurn::Up};
                break;
            case Arm::Swing:
                break;
            }
            continue;
        }
        // Curled, the back arm lies hidden behind the body.
        if (s.curl) continue;
        if (s.sit) {
            if (side == 0) {
                // Leaning back on a hand flat on the floor.
                for (int y = shoulder; y <= torsoBottom + 1; y++) armPx(side, x, y, y - shoulder, false);
                armPx(side, x - 1, feet, 0, true);
                // Put down on the floor behind.
                p.f.held = {int8_t(x - 3), int8_t(feet), BottleTurn::Up};
            } else {
                // Reaching to the raised knee, the hand on top of it.
                for (int y = shoulder; y < torsoBottom; y++) armPx(side, x, y, y - shoulder, false);
                armPx(side, x + 1, torsoBottom - 1, 0, true);
                if (knee > x + 2) armPx(side, x + 2, torsoBottom - 1, 0, true);
            }
            continue;
        }
        int swing = side == 0 ? stride : -stride;
        // Bent over winded: the arm hangs straight down to the knee, the hand on it.
        if (s.bend) {
            int knee = legTop + 1;
            for (int y = shoulder; y <= knee; y++) armPx(side, x, y, y - shoulder, y == knee);
            if (side == 0) p.f.held = {int8_t(x - 2), int8_t(knee), BottleTurn::Up};
            continue;
        }
        // Running, bent at the elbow: the arm swung ahead brings its hand up
        // (the back one only as far as its side, behind the body), the one
        // swung back trails it behind.
        if (s.run) {
            bool tucked = swing > 0 && side == 0;
            int handX = swing > 0 && side == 1 ? x + 1 : swing < 0 ? x - 1 : x;
            int handY = swing > 0 ? shoulder + 1 : shoulder + 2;
            armPx(side, x, shoulder, 0, false);
            armPx(side, x, shoulder + 1, 1, tucked);
            if (!tucked) armPx(side, handX, handY, 2, true);
            if (side == 0) p.f.held = {int8_t(handX - 2), int8_t(handY), BottleTurn::Up};
            continue;
        }
        int hand = torsoBottom + 1 - (swing > 0 ? 1 : 0);
        for (int y = shoulder; y <= hand; y++) armPx(side, x, y, y - shoulder, y == hand);
        // Carried at the side, behind the hand.
        if (side == 0) p.f.held = {int8_t(x - 2), int8_t(hand), BottleTurn::Up};
    }

    // Leaning into a run, or bent over, the head is thrust ahead.
    const int hx = CX + s.lean;
    p.body(BodyPart::Head, [up, lean = s.lean](int x, int y) { return Vec2i{x - lean, y + up}; });
    p.row(hx - 1, hx + 1, neck, skin);
    // Nodding off, the head hangs down over the neck.
    const int hTop = headTop + s.nod, hBottom = headBottom + s.nod;
    p.body(BodyPart::Head, [up, nod = s.nod, lean = s.lean](int x, int y) { return Vec2i{x - lean, y + up - nod}; });
    for (int y = hTop; y <= hBottom; y++) {
        int edge = (y == hTop || y == hBottom) ? 1 : 0;
        p.row(hx - 2 + edge, hx + 2 - edge, y, skin);
    }
    Rgba eyes = s.eyesShut ? shade(skin, 0.7f) : EYES;
    p.set(hx - 1, hTop + 2, eyes);
    p.set(hx + 1, hTop + 2, eyes);
    if (beard) p.row(hx - 1, hx + 1, hBottom, hair);
    // Dirty, the beard grows out ragged under the chin.
    if (beard && dirty) p.set(hx, hBottom + 1, hair);

    // Hair sits on the head; the back of the head is the left side.
    auto crop = [&] {
        p.row(hx - 2, hx + 2, hTop, hair);
        p.set(hx - 2, hTop + 1, hair);
    };
    switch (hairStyle) {
    case Hair::Bald:
        break;
    case Hair::Short:
        crop();
        break;
    case Hair::Long:
        crop();
        for (int y = hTop + 1; y <= hBottom + 1; y++) p.row(hx - 3, hx - 2, y, hair);
        break;
    case Hair::Spiky:
        crop();
        p.set(hx - 2, hTop - 1, hair);
        p.set(hx, hTop - 1, hair);
        p.set(hx + 2, hTop - 1, hair);
        break;
    case Hair::Bun:
        crop();
        p.row(hx - 2, hx - 1, hTop - 1, hair);
        break;
    case Hair::Mohawk:
        p.row(hx - 1, hx, hTop - 1, hair);
        p.row(hx - 1, hx, hTop, hair);
        break;
    case Hair::Ponytail:
        crop();
        for (int y = hTop + 1; y <= hTop + 3; y++) p.set(hx - 3, y, hair);
        break;
    case Hair::Cap:
        p.set(hx - 2, hTop + 1, hair);
        p.row(hx - 1, hx + 1, hTop - 1, hat);
        p.row(hx - 2, hx + 3, hTop, hat);
        break;
    }

    // Dirty, its own hair grows out messy: tufts and strays standing off
    // whatever it had, hair sticking out from under a cap. The bald stay bald.
    if (dirty) {
        auto strays = [&](std::initializer_list<Vec2i> at) {
            for (Vec2i v : at) p.set(hx + v.x, hTop + v.y, hair);
        };
        switch (hairStyle) {
        case Hair::Bald:
            break;
        case Hair::Short:
            strays({{-2, -1}, {1, -1}, {-3, 1}, {2, 1}, {-3, 3}});
            break;
        case Hair::Long:
            // Grown into a tangled mop: a clump over the brow, hanks down
            // the back to the neck with strays off them.
            strays({{-2, -1}, {1, -1}, {2, 1}, {-1, 1}, {-2, 2}, {-4, 1}, {-4, 3}, {-3, 6}, {-2, 6}});
            break;
        case Hair::Spiky:
            strays({{0, -2}, {-3, 0}, {3, 0}, {-3, 2}});
            break;
        case Hair::Bun:
            // Coming undone.
            strays({{-3, -1}, {-3, 1}, {-3, 2}, {-4, 3}, {2, 1}});
            break;
        case Hair::Mohawk:
            // Grown out and flopped over, stubble coming back at the sides.
            strays({{-1, -2}, {1, -1}, {1, 0}, {-2, 0}, {-2, 1}});
            break;
        case Hair::Ponytail:
            strays({{-2, -1}, {1, -1}, {-4, 2}, {-3, 4}, {2, 1}});
            break;
        case Hair::Cap:
            strays({{-3, 1}, {-3, 2}, {-2, 2}, {-4, 3}, {2, 1}});
            break;
        }
    }

    return p.f;
}
