#pragma once
#include "human/body_blood.h"
#include "human/bottle.h"
#include "human/human_look.h"
#include <array>
#include <raylib.h>
#include <unordered_map>
#include <utility>
#include <vector>

/// Guard and Punch are the two halves of one punch. Fall is dying: animTime
/// counts from the moment of death, and the body stays down after. Sit
/// counts from starting to sit down: a crouch for SIT_DOWN_TIME, then seated
/// for as long as it lasts. Rise counts from starting to get up: a crouch
/// for STAND_UP_TIME, then standing. Wash crouches at the water. Feed
/// crouches over a body, dipping its head to it. Wake lies on its back like
/// the dead, counting from coming to: eyes shut, then open and blinking.
/// Point holds the front arm out showing the way, level, raised (PointUp)
/// or lowered (PointDown). Squat crouches over its
/// feces; Piss stands still, hands low. Sleep counts from settling down:
/// a crouch for SIT_DOWN_TIME, nodding off seated until LIE_DOWN_TIME,
/// then curled up on its side for as long as it lasts, breathing slow,
/// z's rising. WakeUp counts from waking: still curled, eyes open and
/// blinking for ROUSE_TIME, a crouch for STAND_UP_TIME, then standing.
/// DiedAsleep lies curled as in sleep, but still: no breath, no z's.
/// Sip drinks from a bottle held up to the mouth; Fill kneels with it in
/// the water; Pour holds it out upside down, water running out of it;
/// OfferBottle holds it out to someone. Run is the run cycle; Winded stands
/// bent over, hands on its knees, panting. Shooting holds the gun out at
/// arm's length, aimed straight up (ShootUp), up ahead (ShootUpAhead),
/// ahead (Shoot), down ahead (ShootDownAhead) or south, out at the one
/// looking on (ShootDown);
/// facing left mirrors them, for eight ways in all. animTime counts from
/// the shot: the kick knocks the hand back, tips the muzzle up and jolts
/// the head back for SHOT_TIME, then it holds the aim. Type stands at a computer's keys, tapping.
enum class Pose { Idle, Walk, Wave, Eat, Guard, Punch, Offer, Fall, Sit, Rise, Wash, Search, Feed, Wake, Point, PointUp, PointDown, Squat, Piss, Sleep, WakeUp, DiedAsleep, Sip, Fill, Pour, OfferBottle, Lie, Run, Winded, Shoot, ShootUp, ShootUpAhead, ShootDownAhead, ShootDown, Type };


/// One human's frames as textures, made from its look.
class HumanSprites {
public:
    explicit HumanSprites(const HumanLook& look);
    ~HumanSprites();
    HumanSprites(const HumanSprites&) = delete;
    HumanSprites& operator=(const HumanSprites&) = delete;

    /// Draws with the base pixel's bottom edge centred on feet; a body lying
    /// dead keeps its back on the ground across the feet's cell. tint
    /// multiplies every pixel. The cakes it carries pile up in the back hand
    /// (seated, on the floor behind), the bottle standing on top of them; or
    /// the bottle is in the front hand when inHand (and the pose holds it
    /// there), as full and dirty as it is. Curled up, one cake shows, hugged
    /// to the chest; lying down, nothing does.
    void draw(Vector2 feet, Pose pose, float animTime, bool flip, int scale = 1, Color tint = WHITE,
              int cakes = 0, const Bottle* bottle = nullptr, bool inHand = false) const;
    /// Redraws every frame with this blood on the body, unless it already shows it.
    void bloody(const BodyBlood& blood);
    /// The same for the feces and the piss on it, painted over the blood.
    void dirty(const BodyBlood& dirt, const BodyBlood& soaked);
    /// Redraws every frame dirty from going unwashed (HumanLook::dirty), or
    /// clean again, unless it already shows so.
    void setDirty(bool dirty);
    /// Redraws the body lying dead eaten this far (0 whole to 1 picked over)
    /// and rotten this far (0 fresh to 1 bare bones), unless it already
    /// shows about that much; seed keeps the bites its own. curled: it died
    /// asleep, and lies curled up.
    void remains(float eaten, float decay, uint64_t seed, bool curled = false);

private:
    Texture2D idle_, guard_, punch_, offer_, point_, pointUp_, pointDown_, kneel_, dead_, crouch_, woken_, wokenCarry_,
        sip_, fill_, pour_, offerBottle_;
    /// Where each texture's frame holds what it carries, and a bottle in
    /// the front hand.
    struct Spots {
        BottleSpot held, hand;
        GunSpot gun;
    };
    std::unordered_map<const Texture2D*, Spots> spots_;
    /// Shooting, each way it aims (GunSpot::aim + 2): holding the aim, and going off.
    std::array<std::array<Texture2D, 2>, SHOOT_AIMS> shoot_;
    std::array<Texture2D, WALK_FRAMES> walk_;
    std::array<Texture2D, RUN_FRAMES> run_;
    std::array<Texture2D, WINDED_FRAMES> winded_;
    std::array<Texture2D, WAVE_FRAMES> wave_;
    std::array<Texture2D, EAT_FRAMES> eat_;
    std::array<Texture2D, TYPE_FRAMES> type_;
    std::array<Texture2D, SIT_FRAMES> sit_;
    std::array<Texture2D, DOZE_FRAMES> doze_;
    std::array<Texture2D, CURL_FRAMES> curl_, curlCarry_;
    /// Curled up, the middle of the head in sprite pixels, and the empty
    /// columns ahead of the body, which turned onto its side lie under it.
    Vector2 curlHead_{};
    int curlSink_ = 0;
    /// Every texture with the frame it was made from, clean.
    std::vector<std::pair<Texture2D*, SpriteFrame>> frames_;
    /// The BodyBlood version the textures show; none yet.
    int bloodVersion_ = 0;
    const BodyBlood* blood_ = nullptr;
    /// Likewise the feces and the piss on it.
    int dirtVersion_ = 0, soakedVersion_ = 0;
    const BodyBlood* dirt_ = nullptr;
    const BodyBlood* soaked_ = nullptr;
    /// The look the frames are drawn from, dirty or not.
    HumanLook look_;
    /// For the remains: the skin as it was, and how eaten and how rotten
    /// the dead body shows, in steps.
    Rgba skin_;
    int eatenStep_ = 0, rotStep_ = 0;
    bool curledDead_ = false;
    uint64_t seed_ = 0;

    /// Calls add(texture, frame) for every texture with its frame drawn
    /// from look, always in the same order.
    template <class F> void eachFrame(const HumanLook& look, F&& add);
    void add(Texture2D& t, SpriteFrame f);
    /// Uploads the frame with the blood on it, and on the dead body its remains.
    void paint(Texture2D& t, const SpriteFrame& f);
    /// Whether t is drawn eaten and rotten: the dead body, curled if it
    /// died asleep.
    bool showsRemains(const Texture2D* t) const;
    /// Curled up asleep or just woken, turned onto its side; asleep, z's
    /// rise from the head, sleepTime counting from lying down.
    void drawCurled(Vector2 feet, const Texture2D& tex, bool flip, int scale, Color tint, float sleepTime) const;
};
