#pragma once
#include "core/color.h"
#include "core/rng.h"
#include "human/wound.h"
#include <array>
#include <cstdint>

/// Pocket Rave's 5x6 humans at three times the pixels. The base pixel, the
/// one that stands on a cell, is the bottom centre.
inline constexpr int SPRITE_W = 15;
inline constexpr int SPRITE_H = 18;
inline constexpr int SPRITE_BASE_X = SPRITE_W / 2;
inline constexpr int WALK_FRAMES = 4;
inline constexpr int WAVE_FRAMES = 2;
inline constexpr int EAT_FRAMES = 2;
/// Seated: eyes open, then a blink.
inline constexpr int SIT_FRAMES = 2;
/// Nodding off seated: eyes shut head up, then the head dropped.
inline constexpr int DOZE_FRAMES = 2;
/// Curled up asleep: breathing out, breathing in.
inline constexpr int CURL_FRAMES = 2;
/// Seconds crouching on the way down to sit, and on the way back up.
inline constexpr float SIT_DOWN_TIME = 0.2f;
inline constexpr float STAND_UP_TIME = 0.2f;
/// Seconds nodding off seated before lying down to sleep, and lying awake
/// curled before getting up.
inline constexpr float LIE_DOWN_TIME = 2.6f;
inline constexpr float ROUSE_TIME = 1.2f;

inline constexpr int SPRITE_PX = SPRITE_W * SPRITE_H;
/// Not part of the body: a cake held, or nothing.
inline constexpr uint8_t NO_PART = 0xff;

/// Which way a bottle points, from its base to its cap, facing right.
enum class BottleTurn : uint8_t { Up, Down, Ahead, Back };
/// Where a frame has a bottle: the pixel its base is on, and which way it
/// points. None when x is below 0.
struct BottleSpot {
    int8_t x = -1, y = -1;
    BottleTurn turn = BottleTurn::Up;
    bool valid() const { return x >= 0; }
};

/// One frame's pixels, and for each, the pixel of the standing pose (idle,
/// facing right) it is the same bit of body as, so whatever is on the body
/// moves with it from pose to pose, and the body part it belongs to.
struct SpriteFrame {
    std::array<Rgba, SPRITE_PX> px{};
    /// Index into a standing frame, or -1 off the body.
    std::array<int16_t, SPRITE_PX> rest;
    /// A BodyPart, or NO_PART.
    std::array<uint8_t, SPRITE_PX> part;
    /// Where a bottle shows: in the back pocket, and held in the front hand
    /// (sipping, filling, pouring, handing it over). The bottle itself is
    /// drawn over the frame, as full and as dirty as it is.
    BottleSpot pocket, hand;

    SpriteFrame() {
        rest.fill(-1);
        part.fill(NO_PART);
    }
};

/// Matted is never rolled: it is the hair grown wild on a feral human.
enum class Hair { Bald, Short, Long, Spiky, Bun, Mohawk, Ponytail, Cap, Matted };
enum class Top { Tee, LongSleeve, Tank };
enum class Bottom { Pants, Shorts, Skirt };

/// Everything that makes one human look like themself, all rolled at random.
/// Frames are drawn facing right; facing left flips them.
struct HumanLook {
    Rgba skin, hair, top, bottom, shoes, hat;
    Hair hairStyle = Hair::Short;
    Top topStyle = Top::Tee;
    Bottom bottomStyle = Bottom::Pants;
    /// Body proportions in sprite pixels.
    int torsoW = 5, torsoH = 5, legH = 4;
    bool beard = false;
    /// Gone feral from eating its own kind: every frame is repainted with
    /// matted hair, sallow skin, red eyes, a bloodied mouth, filthy hands,
    /// bare feet and torn, grimy clothes. The body keeps the same pixels in
    /// every pose, so cakes, bottles, pointing and blood all land as before.
    bool feral = false;

    static HumanLook random(Rng& rng);
    /// The skin as the frames draw it: sallow when feral.
    Rgba bodySkin() const;

    /// carrying holds a cake in the back hand.
    SpriteFrame idle(bool carrying = false) const;
    /// One frame of the walk cycle: 0 and 2 stand, 1 and 3 stride.
    SpriteFrame walk(int frame, bool carrying = false) const;
    /// One frame of a wave with the front arm.
    SpriteFrame wave(int frame) const;
    /// Fist up in front, about to punch.
    SpriteFrame guard() const;
    /// The front arm thrown out straight, the front foot forward.
    SpriteFrame punch() const;
    /// Eating a cake: 0 bites it, 1 holds it down while chewing.
    SpriteFrame eat(int frame) const;
    /// Holding a cake out to someone.
    SpriteFrame offer() const;
    /// The front arm held out pointing the way: -1 raised high (a way
    /// ahead, north), 0 level, 1 lowered (back, south).
    SpriteFrame point(int tilt) const;
    /// Knees giving way, eyes shut: the middle of falling down dead.
    SpriteFrame kneel() const;
    /// Standing straight with eyes shut; drawn turned on its back.
    SpriteFrame dead() const;
    /// Knees bent, eyes open: halfway between standing and sitting.
    SpriteFrame crouch(bool carrying = false) const;
    /// Sitting on the floor, legs out ahead, leaning on the back hand:
    /// 0 looks ahead, 1 blinks. A carried cake rests on the floor behind.
    SpriteFrame sit(int frame, bool carrying = false) const;
    /// Seated nodding off, eyes shut: 0 head up, 1 head dropped.
    SpriteFrame doze(int frame, bool carrying = false) const;
    /// Curled up asleep, knees drawn up, the front hand under the chin;
    /// drawn turned onto its side. 0 breathes out, 1 in (the back swells),
    /// 2 has the eyes open, just woken. A carried cake is hugged to the chest.
    SpriteFrame curl(int frame, bool carrying = false) const;
    /// Drinking from a bottle: standing, the front hand holding it up on its
    /// side, its neck to the mouth.
    SpriteFrame sip() const;
    /// Kneeling at the water, the front arm reaching down, the bottle on its
    /// side ahead in the water.
    SpriteFrame fill() const;
    /// Standing, the front arm out holding the bottle upside down.
    SpriteFrame pour() const;
    /// Holding a bottle out to someone, upright.
    SpriteFrame offerBottle() const;

private:
    /// What the front arm is doing.
    enum class Arm { Swing, WaveIn, WaveOut, Guard, Punch, Bite, Chew, Offer, Point, PointUp, PointDown, Pillow, Sip, Fill, Pour, OfferBottle };
    struct Stance {
        /// 0 standing, or +1 / -1 for a step with either leg.
        int stride = 0;
        Arm front = Arm::Swing;
        bool kneel = false;
        /// On the floor with the legs straight out ahead.
        bool sit = false;
        /// Curled: thighs out ahead, shins tucked back under, back arm
        /// hidden behind the body.
        bool curl = false;
        /// The back swells a pixel, breathing in.
        bool breathe = false;
        /// Pixels the head hangs down, nodding off.
        int nod = 0;
        /// A cake in the back hand.
        bool carry = false;
        bool eyesShut = false;
    };
    /// The frame for this stance, feral or not.
    SpriteFrame draw(const Stance& s) const;
    /// The frame for this stance in this look's own colours and hair.
    SpriteFrame drawBody(const Stance& s) const;
    /// Tears, grime and blood on a frame drawn by drawBody from the feral
    /// colours, each at a pixel of the standing pose, so it moves with the
    /// body; stand is that standing pose, for where the hems are.
    void wear(SpriteFrame& f, const SpriteFrame& stand) const;
};
