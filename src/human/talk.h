#pragma once
#include "core/rng.h"
#include "human/topic.h"
#include <optional>
#include <string>
#include <unordered_map>

class Human;

/// What humans say to each other. Every line comes from data/lines.txt,
/// filed under the moment it is said at, each with the conditions it
/// needs (how the speaker is doing, what it is like, how it sees the one
/// it talks to) and a tone, which is what the line does to whoever hears
/// it. The file's head explains how to write it.
namespace talk {

/// When something is said. The order of the sections' names in talk.cpp.
enum class Moment {
    // Greetings and tells.
    Meet, CatchUp, Introduce, TellCake, TellKey, TellTrap, TellDoor, SellTrap, Lie, Warn, Praise, ThankTip,
    // Turning someone away at a greeting, and their answer.
    Refuse, Snubbed,
    // Cakes and fights.
    Beg, Give, Thanks, Pay, Threat, Yield, Defy, Hit, Ouch, Flee, Killed,
    // Caught by a trap, said to nobody: a dart, spikes, shut in a slam room.
    Darted, Spiked, Slammed,
    // Shut in with no key, crying for help (to nobody, through the walls);
    // a hearer setting off to let them out (to nobody); let out (to whoever
    // did it).
    Help, HelpComing, Freed,
    // Death and blood.
    FoundBody, Loot, Shame, Pool, OwnPool, Escort, Escorted, Accuse,
    // Lessons learnt, said to nobody.
    LearnCakes, LearnKeys,
    // Flesh: the mind snapping, going for someone to eat, eating a body,
    // rotten flesh, seeing someone eat, a half-eaten body, the body it ate
    // haunting it, craving more.
    Snap, Hunt, Feed, Sick, SawEat, Gnawed, Haunted, Craving,
    // Coming to somewhere it doesn't know, said to nobody.
    Wake,
    // The first key, cake, door, water, other human and someone else's
    // shitty footprints and toilet it ever sees, said to nobody, once each.
    FirstKey, FirstCake, FirstDoor, FirstWater, FirstHuman, FirstPrints, FirstToilet,
    // Hope: losing it, getting it back, giving up and making a home (all
    // said to nobody), and warning someone off that home.
    Despair, Hope, Settle, Guard,
    // Word of the way out at a greeting: asking it, then the answer: which
    // way it thinks, that there is none, the wrong way to a foe, or no idea.
    AskWay, TellWay, NoWayOut, LieWay, NoIdea,
    // Taking up with someone, to keep with them, and their answer;
    // following someone with a key (to them).
    Join, JoinReply, Follow,
    // Splitting up: lost them (to nobody), turned on them, kept away while
    // they're filthy, their keys run out, off with a key of its own or
    // after one (all to them).
    PartLost, PartTurned, PartFilthy, PartNoKeys, PartOwnKey,
    // Chit-chat, one per kind of topic (topic.h), in that order.
    ChatHere, ChatShared, ChatStory, ChatSomeone, ChatFeeling, ChatWay, ChatGroup, ChatBefore,
    // The answer to it, one per Topic::Reply from Agree on; then the
    // opener's follow-up to the answer, and the answer's close.
    ReplyAgree, ReplyAdd, ReplyDisagree, ReplyUnheard, ReplyBrush, ChatFollow, ChatClose,
    // Thirst and going: parched, foul water drunk, needing to go, an
    // accident, stepping in feces (all said to nobody); seen going (to the
    // one who saw); someone going in the water (to them); laughing at an
    // accident (to whoever had it); looking away for someone in the group
    // (to them); waiting for the toilet (to whoever is on it).
    Parched, FoulWater, NeedToGo, Accident, SteppedIn, SeenGoing, IntoWater, Mock, Privacy, ToiletWait,
    // Drinking red water (to nobody); seeing someone wash in its water (to them).
    BloodyWater, Fouled,
    // A settler to someone thirsty come for its water: turning them away,
    // letting them drink, asking a cake for it; and to whoever drank and
    // didn't pay.
    MyWater, WaterFree, WaterPrice, WaterCheat,
    // The way to water at a greeting: told, sold, a way over spikes to a
    // foe; and asking those it walks with where water is.
    TellWater, SellWater, LieWater, AskWater,
    // Leading someone parched to water (to them), and their answer.
    LeadDrink, LedDrink,
    // Getting thirsty and drinking (to nobody); learning to keep near water.
    Thirsty, Drink, LearnWater,
    // Drinking from a toilet (to nobody), and the cruel seeing it (to them).
    ToiletDrink, ToiletMock,
    // Sleep: getting tired, exhausted, lying down, waking, a dream (all to
    // nobody); woken by a thief (to them); a cake gone on waking (to
    // nobody, about whom it blames); asking someone to keep watch (to
    // them), and their answer: yes, for a cake, no; waking the others for
    // a threat, and the next for a turn (to them); blaming a watcher who
    // slept or went (to them); trying to wake a body it took for asleep
    // (to nobody, about them).
    Tired, Exhausted, SleepNow, Woke, Dream, RobbedSleep, CakeGone, WatchMe, WatchYes, WatchCake, WatchNo, WakeUp,
    YourWatch, FailedWatch, WakeThem,
    // Looking at a fresh pool of the blood it spilled itself (to nobody, about whose).
    Spilled,
    // Putting someone seen bloody together with a fresh body: shouted to
    // anyone in earshot, about the one it blames.
    Blame,
    // Someone shut in: telling of them at a greeting (to the listener, about
    // them); calling back through the wall (to nobody, about them), and
    // hearing it from inside (to nobody); off for a key to let them out (to
    // nobody); giving up on finding them (to nobody, about them).
    TellShutIn, CallBack, HeardBack, KeyForShutIn, LostShutIn,
    // Bottles: drinking from it, running it dry, filling it, pouring it out
    // (all to nobody); asking someone for a drink (to them), and their
    // answer: pouring some, for a cake, lying that it's empty, refusing;
    // the asker seeing through the lie (to them); a bottle snatched in a
    // fight (to whoever took it).
    Sip, BottleDry, FillBottle, PourOut, AskDrink, GiveDrink, SellDrink, BottleLie, CaughtBottleLie, RefuseDrink,
    BottleTaken,
    // Putting things down on the floor, and picking up what lies there
    // (both to nobody).
    PutDown, PickUp,
    // Stashing things in a stash of its own, and taking from it (both to
    // nobody).
    StashIt, FromStash,
    // Seeing someone stash things (to nobody, about them); finding its own
    // stash short (to nobody, about whom it blames).
    SawStash, StashRobbed,
    // Stealing from someone's stash (to nobody, about them); catching a
    // thief at its stash (to them); resenting a hoarder (to nobody, about
    // them).
    StealStash, CaughtStealing, Resent,
    // A stash at a greeting: told, sold, lied about (all to the one told);
    // its owner overhearing (to the teller); asking someone if a stash is
    // theirs (to them), and their answer: owning up, denying it; a denial
    // seen through (to the denier).
    TellStash, SellStash, LieStash, StashTold, AskStash, OwnStash, DenyStash, CaughtDenying,
    // Parched and lost: asked for water and knowing of none (to the asker);
    // boxed in, calling through a locked door (to nobody, carrying through
    // it); leaving a companion as lost as itself (to them); erratic, going
    // for someone's bottle (to them).
    NoWater, CallDoor, PartDry, BottleRage,
    // A lesson it still held, learnt again the hard way, and learnt for good
    // (to nobody); the first drink after thirst began to kill it (to nobody).
    LearnAgain, LearnMax, Quenched,
    // Drinking water it knew was foul (to nobody).
    FoulAgain,
    // Leaving the one it kept with or followed after hitting or robbing
    // them (to nobody, about them).
    PartWronged,
    // Wary after a trap, leaving a cake alone (to nobody): it might be on
    // a dart plate; it lies in a room with one door and no key in hand.
    WaryCake, WarySlam,
    // Taking a fuller bottle for its own, poured into it (to nobody).
    SwapBottle,
    // Warning someone it walks with off foul water (to them); drinking foul
    // water it was warned of (to nobody, about whoever warned it).
    WarnFoul, FoulWarned,
    // Bottle-less, finding a bottle lying (to nobody); coming back to find it
    // gone (to nobody), or stolen once it holds on hard (to nobody, about
    // whom it blames); the first time it runs its bottle dry or fills it,
    // seeing that water is scarce (to nobody).
    SeeBottle, BottleGone, BottleStolen, Scarce,
    // Turning dirty from going unwashed, and washed clean of it again (both
    // to nobody).
    GotDirty, WashedClean,
    // Smell: starting to smell itself, smelling death it can't see, setting
    // off after that smell, walking off from a smell to eat (all to nobody).
    Stink, SmellDeath, FollowStench, PutOff,
    // Vending machines: the first one it ever sees, buying a cake or a
    // bottle with keys, finding it empty (all to nobody); a machine at a
    // greeting, told or sold (to the one told).
    FirstVending, VendCake, VendBottle, VendEmpty, TellVending, SellVending,
    // Coming to: stirring while it still lies with its eyes shut, and
    // getting up out of the waking sit (both to nobody).
    WakeStir, WakeStand,
    // Still dazed, the first giant statue and the first table it sees, and
    // finding nobody about once it has looked round (all to nobody).
    FirstStatue, FirstTable, EmptyRoom,
    // Still dazed, trying to remember anything from before and finding
    // only its own name (to nobody).
    Blank,
    // Up, alone and finding nothing new for a while, the quiet of the place
    // (to nobody).
    Quiet,
    // Setting foot outside the place it woke in for the first time, and the
    // first new place that no longer lifts its hope (both to nobody).
    ItGoesOn, NeverEnds,
    // Calling out on its first door out of where it woke (shouted, to
    // nobody); hearing a call from out of sight (to nobody, about the
    // caller), and answering it (shouted); hearing its call answered (to
    // nobody, about who answered).
    CallOut, HeardCall, CallAnswer, CallAnswered,
    // Server rooms: the first one it ever sees; setting off, tired, with
    // keys to spend, to sleep in one it knows; lying down to sleep among
    // the humming machines (all to nobody).
    FirstServerRoom, SeekHum, SleepHum,
    // Running out of breath, bent over winded (to nobody).
    Winded,
    // The server room airlock (AI/server_room_keys_plan.md): another locked
    // door behind the one it just opened, the first time; the second time,
    // learning the double doors eat keys; letting them be to keep its keys;
    // opening them anyway, curious (all to nobody); telling someone what
    // they cost (to them); feeling the hum inside one the first time, and
    // drowsy, lying down there rather than leave (to nobody).
    AirlockSurprise, AirlockLesson, SpareKeys, DareAirlock, TellAirlock, HumLesson, StayHum,
    // Water dispensers: the first one it ever sees; drinking at one's tap;
    // finding one dry it came for or keeps; pouring its bottle into the one
    // it keeps (all to nobody).
    FirstDispenser, DispenserDrink, DispenserDry, RefillDispenser,
    // Looking at the first hidden trap it ever spots (to nobody).
    SpotTrap,
    // Toilets: seeing one fouled by someone else, and the first time it uses
    // one and finds no flush (both to nobody).
    ToiletFoul, NoFlush,
    // Computers (AI/computers_plan.md): the first terminal and the first
    // console desk it tries, and every later one, each locked by a password
    // (all to nobody); seeing someone else at the keys (to nobody, about them).
    TryTerminal, TryDesk, TryAgain, WatchTyping,
    // Looking after its needs (AI/needs_planning_plan.md): drinking at water
    // it passes before it is thirsty; turning back, too far from water;
    // exploring for water, for food, it knows none of; the other need while
    // there; going back to an errand it was turned aside from (all to nobody).
    TopUp, TurnBack, SeekWater, SeekFood, WhileHere, ErrandBack,
    // The same thing again, at the 4th, 8th, 15th and 30th time (all to
    // nobody; a body's about whose it was), in the order of Human::Repeat:
    // a cake eaten, a body found, a drink, a door unlocked, caught by a
    // trap, blood washed off, a fight, going, waking up here, the quiet, a
    // face caught up with (about them), sinking low (rotten flesh, toilet
    // or foul water).
    CakesAgain, BodiesAgain, WaterAgain, DoorsAgain, TrapsAgain, BloodAgain, FightsAgain, GoingAgain, SleepAgain,
    QuietAgain, FacesAgain, LowAgain,
    // The dark (AI/light_plan.md step 5): the first time it stands where no
    // light falls; going into the dark again after a while in light; coming
    // out of a long spell in it; making for light it sees from the dark
    // (all to nobody).
    FirstDark, IntoDark, OutOfDark, TowardLight,
    // Lying down to sleep in the dark, out of sight, as the bold would; under
    // a light, as the timid would (to nobody, in place of sleep_now).
    SleepDark, SleepLit,
    // Hiding in the dark: the modest going where it hides them; making a
    // stash where no light falls (both to nobody).
    ReliefDark, StashDark,
    // Stepping in feces it couldn't see for the dark (to nobody, in place of
    // stepped_in).
    SteppedInDark,
    // Trying doors (AI/door_trying_plan.md): a shut door's handle that won't
    // give; an airlock door that gives at a push from inside; finding
    // enough locked to stop trying; boxed in, trying them all again (all to
    // nobody).
    TryDoorLocked, DoorGave, LearnDoors, TryDoorsBoxed,
    Count
};

/// Blows and cries: said at once, or not at all.
inline bool isCry(Moment m) {
    return m == Moment::Hit || m == Moment::Ouch || m == Moment::Darted || m == Moment::Spiked ||
           m == Moment::Slammed;
}
/// An emergency: blows and cries, and the shout of who killed. These cut in
/// over a line that has shown long enough to read, and the lines still
/// waiting are dropped for them.
inline bool isUrgent(Moment m) { return isCry(m) || m == Moment::Blame; }
/// The first lines on coming to: stirring, waking, getting up, reaching back for anything from before.
inline bool isWaking(Moment m) {
    return m == Moment::WakeStir || m == Moment::Wake || m == Moment::WakeStand || m == Moment::Blank;
}

/// The moment for bringing up a topic of this kind.
inline Moment chatMoment(Topic::Kind k) { return Moment(int(Moment::ChatHere) + int(k)); }
inline Moment replyMoment(Topic::Reply r) { return Moment(int(Moment::ReplyAgree) + int(r) - 1); }

/// What a line does to whoever hears it. Warm and rude lines move the
/// trust of the one spoken to; a plea may move them to share; a frightened
/// line scares everyone in earshot who doesn't distrust the speaker; a
/// threat may make the one threatened hand over a cake.
enum class Tone { Plain, Warm, Rude, Plead, Fear, Threat };

/// A line picked to be said, with the names in it filled in, and which
/// names it said aloud: the one spoken to, the speaker's own, the third.
struct Said {
    std::string text;
    Tone tone = Tone::Plain;
    bool namesThem = false, namesMe = false, namesWho = false;
};

/// A line for the speaker to say at the moment, to `to` (or nobody in
/// particular) and about `about` (or nobody): one of those whose conditions
/// all hold and whose names the speaker knows, the more conditions the
/// likelier. Nothing when none fits, or when the moment's chance in the
/// file says it keeps quiet this time.
///
/// Bringing up a topic (a chat moment) nothing is left to chance: of the
/// fitting lines, one the speaker hasn't said in the last few minutes, the
/// one with the most conditions, among those the one it said longest ago
/// (by `used`, line text to world time, which is updated), and among those
/// the one this speaker likes best (fixed for each human).
std::optional<Said> pick(Moment m, const Human& speaker, const Human* to, const Human* about, Rng& rng,
                         const Topic* topic = nullptr, std::unordered_map<std::string, float>* used = nullptr,
                         float now = 0.0f);

/// Reads the lines again, so the file can be edited while playing. False
/// if no file was found; humans then say nothing.
bool reload();

} // namespace talk
