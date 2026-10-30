#pragma once
// Tuning and helpers shared by human.cpp and the files split from it
// (human_walk, human_social, human_fight, human_speech). Systems in their
// own human_*.cpp files keep their tuning at the top of those files.
#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace {

/// Seconds per straight step of one cell; diagonals take √2 times as long.
constexpr float STEP_TIME = 0.2f;
constexpr float DIAGONAL = 1.41421356f;
/// Seconds spent turning the key before stepping through.
constexpr float UNLOCK_TIME = 0.6f;
/// After reaching a spot, chance to stand and look around for a moment.
constexpr float LOOK_AROUND_CHANCE = 0.15f;
constexpr float LOOK_TIME_MIN = 0.4f, LOOK_TIME_MAX = 1.5f;
constexpr float IDLE_TIME_MIN = 1.0f, IDLE_TIME_MAX = 4.0f;
/// Instead of idling, chance to sit down for longer; seated, it looks round
/// this often.
constexpr float SIT_CHANCE = 0.6f;
/// A restless human sits this much less often, a stay-at-home this much more.
constexpr float SIT_CURIOUS = 0.4f;
constexpr float SIT_TIME_MIN = 5.0f, SIT_TIME_MAX = 15.0f;
constexpr float SIT_GLANCE = 0.5f;
/// Just made, a human lies where it came to this long, then sits up and
/// looks round this long before getting up. Dazed at first, it sees only
/// this far and walks this much slower, both wearing off over DAZE_TIME.
constexpr float WAKE_LIE_MIN = 3.0f, WAKE_LIE_MAX = 6.0f;
constexpr float WAKE_SIT_MIN = 2.0f, WAKE_SIT_MAX = 4.0f;
constexpr float DAZE_TIME = 40.0f;
constexpr int DAZE_SIGHT = 6;
constexpr float DAZE_SLOW = 0.6f;
/// Wanting out: everyone wakes at 1 and settles to OUT_REST over OUT_SETTLE
/// seconds. It adds up to OUT_EXPLORE to exploring and to opening doors
/// (keeping OpenDoor under Explore), and up to OUT_ASK to greeting, to ask
/// whoever it meets what this place is.
constexpr float OUT_REST = 0.6f;
constexpr float OUT_SETTLE = 600.0f;
constexpr float OUT_EXPLORE = 0.4f;
constexpr float OUT_ASK = 0.25f;
/// Wanting out past OUT_URGE_FROM, a key holder is up to DOOR_URGE_OUT
/// likely (the restless more, stay-at-homes less) to make for a locked
/// door ahead of exploring, each time it weighs it.
constexpr float OUT_URGE_FROM = 0.7f;
constexpr float DOOR_URGE_OUT = 0.3f;
/// Wanting out as much as can be, a place right against its hunch costs
/// as much to head for as this many tenths of a cell more walking, and a
/// door (the way on to more of the place) HUNCH_DOOR_COST. Going through a
/// door, the hunch turns this far toward the way it went.
constexpr int HUNCH_COST = 300;
constexpr int HUNCH_DOOR_COST = 1500;
/// Hope, 0 to 1 (more in human_hope.cpp): what each thing does to it. A new
/// chunk is in human_hope.cpp, as it wears off. Falls hit the timid harder.
constexpr float HOPE_DOOR = 0.1f;
constexpr float HOPE_FRIEND = 0.1f;
constexpr float HOPE_BODY = -0.1f;
/// Bones: someone died here long ago, and nobody ever got them out.
constexpr float HOPE_BONES = -0.15f;
constexpr float HOPE_FRIEND_DEAD = -0.25f;
constexpr float HOPE_STARVING = -0.1f;
constexpr float HOPE_PARCHED = -0.1f;
constexpr float HOPE_DEAD_END = -0.15f;
/// What gloom (hope gone below HOPE_LOW) does at its fullest: up to this
/// much likelier to sit, this much more wish to stroll, strolls this much
/// shorter, and lessons this much slower to fade.
constexpr float DESPAIR_SIT = 1.0f;
constexpr float DESPAIR_WANDER = 0.5f;
constexpr float DESPAIR_SHORT = 0.5f;
constexpr float LESSON_GLOOM = 0.5f;
/// Word of the way out. A listener takes it on as far as it trusts the
/// teller, plus WAY_STRANGER for anyone: pulled toward the told way by
/// how sure the teller sounds against how sure it is itself, surer by up
/// to WAY_SURE when they agree, shakier when they don't (never under
/// WAY_SURE_MIN), and a little hopeful (WAY_HOPE). Told there is none, it
/// loses up to WAY_GLOOM hope. Gloomy from WAY_GLOOMY gloom on, sure of
/// it once it has seen WAY_SEEN_ENOUGH chunks. A lie sounds WAY_LIE_SURE.
constexpr float WAY_STRANGER = 0.2f;
constexpr float WAY_SURE = 0.15f;
constexpr float WAY_SURE_MIN = 0.05f;
constexpr float WAY_HOPE = 0.04f;
constexpr float WAY_GLOOM = 0.15f;
constexpr float WAY_GLOOMY = 0.3f;
constexpr float WAY_SEEN_ENOUGH = 5.0f;
constexpr float WAY_LIE_SURE = 0.8f;
/// Told again only once it thinks this differently.
constexpr float WAY_RETELL = 0.785f;
/// Asks the way out only wanting out this much, and then only this often
/// at a first greeting (once per human, asked or not), and asks back after
/// answering this often; answers only this sure of its hunch, else it has
/// no idea.
constexpr float ASK_WAY_FROM = 0.4f;
constexpr float ASK_WAY_CHANCE = 0.15f;
constexpr float ASK_BACK_CHANCE = 0.3f;
constexpr float WAY_UNSURE = 0.15f;
constexpr float HUNCH_FOLLOW = 0.25f;
constexpr float TAU = 6.28318531f;

/// With nothing left to find, humans stroll to known spots this close:
/// the restless twice as far, stay-at-homes half.
constexpr int WANDER_RADIUS = 12;
/// With nothing left to find, how often a human goes back to a locked door
/// to see whether someone opened it, how long before it checks the same
/// door again, and how close counts as having checked.
constexpr float CHECK_DOOR_CHANCE = 0.5f;
/// Which the restless do up to this much more often.
constexpr float CHECK_DOOR_CURIOUS = 0.4f;
constexpr float DOOR_RECHECK_TIME = 90.0f;
constexpr int DOOR_CHECK_RANGE = 2;
/// Cells a single search may expand before giving up.
constexpr int SEARCH_LIMIT = 200000;
/// Steps in a row a walker waits for someone in the way before giving up.
constexpr int GIVE_UP_AFTER = 5;
/// Stepped aside for someone, it waits this long for them to pass.
constexpr float GIVE_WAY_WAIT_MIN = 1.0f, GIVE_WAY_WAIT_MAX = 2.0f;
/// Humans this close (in cells, diagonals counting as one) greet.
constexpr int GREET_RADIUS = 2;
/// One walking away can be called to from this close.
constexpr int HAIL_RADIUS = 4;
/// Seconds one too busy to be greeted is let be before trying again.
constexpr float BUSY_WAIT = 8.0f;
constexpr float GREET_TIME = 1.5f;
/// Greeters still talking stay together until done, up to this long.
constexpr float GREET_LONGEST = 10.0f;
/// Someone greeted this long ago is greeted again, when either has news
/// for the other: that is when tips, trades and lies change hands.
constexpr float CATCH_UP_TIME = 60.0f;
// Turned away at a greeting, it stands this long before going on.
constexpr float SNUBBED_WAIT_MIN = 2.5f, SNUBBED_WAIT_MAX = 3.5f;
/// A follower keeps this close to the key holder, then waits and watches.
constexpr int FOLLOW_GAP = 2;
constexpr float FOLLOW_WAIT_MIN = 0.2f, FOLLOW_WAIT_MAX = 0.6f;

/// Hunger gained per second by each action. Everything a human does costs
/// some; a new action needs its own cost here.
constexpr float HUNGER_PER_SECOND[] = {
    1.0f / 600.0f, // Rest
    1.0f / 1500.0f, // Sit
    1.0f / 300.0f, // Walk
    1.0f / 160.0f, // Run
    1.0f / 480.0f, // Greet
    1.0f / 600.0f, // Eat
    1.0f / 600.0f, // Drink
    1.0f / 600.0f, // Relieve
    1.0f / 120.0f, // Punch
    1.0f / 480.0f, // Give
    1.0f / 400.0f, // Wash
    1.0f / 450.0f, // Loot
    1.0f / 450.0f, // Feed
    1.0f / 1800.0f, // Sleep
    1.0f / 1500.0f, // Watch
};
static_assert(std::size(HUNGER_PER_SECOND) == size_t(Human::Action::Count));
/// Thirst gained per second by each action, the same rule: a new action
/// needs its own cost here too. For now hunger's own rates, but for
/// eating, as cake is dry, and drinking.
constexpr float THIRST_PER_SECOND[] = {
    1.0f / 600.0f, // Rest
    1.0f / 1500.0f, // Sit
    1.0f / 300.0f, // Walk
    1.0f / 160.0f, // Run
    1.0f / 480.0f, // Greet
    1.0f / 300.0f, // Eat
    0.0f, // Drink
    1.0f / 600.0f, // Relieve
    1.0f / 120.0f, // Punch
    1.0f / 480.0f, // Give
    1.0f / 400.0f, // Wash
    1.0f / 450.0f, // Loot
    1.0f / 450.0f, // Feed
    1.0f / 1800.0f, // Sleep
    1.0f / 1500.0f, // Watch
};
static_assert(std::size(THIRST_PER_SECOND) == size_t(Human::Action::Count));
/// Hungry from here on: food comes first. Starving from STARVING, and
/// losing health once hunger is full.
constexpr float HUNGRY = 0.5f;
constexpr float STARVING = 0.8f;
/// New humans start with up to this much hunger.
constexpr float START_HUNGER_MAX = 0.35f;
/// Health lost per second when starving, and healed per second when not hungry.
constexpr float STARVE_RATE = 1.0f / 60.0f;
/// Needing to go, Want::Relieve scores up to this much more as it nears
/// bursting (from WANTING's base at needBar()); the rest of going is in
/// human_relief.cpp.
constexpr float RELIEF_DRIVE = 2.0f;
/// A fright this big may make it wet itself (see accident()).
constexpr float FRIGHT_WET = 0.3f;
/// Feces on its feet add up to this much more to wanting to wash, at
/// middling modesty.
constexpr float FILTH_WASH = 1.0f;
/// Seeing a fresh pile of someone else's feces frightens the most modest
/// this much, by how disgusting it is.
constexpr float DISGUST_DREAD = 0.05f;
/// Thirst's own HUNGRY, STARVING and START_HUNGER_MAX; parched through, it
/// wears health down as fast as starving does, and both together twice as
/// fast.
constexpr float THIRSTY = 0.5f;
constexpr float PARCHED = 0.8f;
constexpr float START_THIRST_MAX = 0.35f;
/// New humans start this rested at worst: they have just slept.
constexpr float START_TIRED_MAX = 0.2f;
constexpr float PARCH_RATE = 1.0f / 60.0f;
constexpr float HEAL_RATE = 1.0f / 90.0f;
/// Asleep, health comes back and wounds close this many times faster.
constexpr float SLEEP_HEAL = 2.0f;
/// Hit in its sleep, it wakes this frightened, this dazed, and slow to
/// answer for WOKEN_STUN seconds; whoever sees it done thinks as badly of
/// the hitter as of a killing, and is this frightened.
constexpr float DREAD_WOKEN = 0.2f;
constexpr float HOPE_WOKEN_HIT = -0.2f;
constexpr float WOKEN_DAZE = 0.6f;
constexpr float WOKEN_STUN = 0.8f;
constexpr float TRUST_SAW_SLEEPER_HIT = -0.5f;
constexpr float DREAD_SAW_SLEEPER_HIT = 0.3f;
/// After cakes, a holder asleep in sight looks this many cells nearer.
constexpr int SLEEPER_NEARER = 6;
/// Died asleep, it is seen to be dead only from this close; farther off
/// it looks asleep.
constexpr int STILL_SEEN = 2;
/// A sleeper with a cake in sight tempts the greedy this much more, and
/// up to ROB_HUNGRY more as hunger presses.
constexpr float ROB_TEMPT = 0.4f;
constexpr float ROB_HUNGRY = 0.8f;
/// Exhausted, every step takes this much longer.
constexpr float EXHAUSTED_SLOW = 1.15f;
/// What one cake takes off hunger, and how long it takes to eat.
constexpr float CAKE_FOOD = 0.5f;
constexpr float EAT_TIME = 2.5f;
/// Most cakes one human carries: a middling one; the generous one fewer,
/// the grasping one more.
constexpr int CARRY_MAX = 3;
/// Holding a cake out to someone.
constexpr float GIVE_TIME = 0.8f;
/// Lessons, 0 to 1: what one starving spell with no cake, or one dead end
/// with no key, teaches; what a lesson paying off adds (eating from a
/// stock, unlocking a door); how fast they are forgotten, all of one in
/// half an hour; and what a full one does: extra cakes carried, and how
/// much more fetching is wanted, enough to come before exploring for all
/// but the most curious.
constexpr float LESSON_HARD = 0.5f;
constexpr float LESSON_PAID = 0.1f;
constexpr float LESSON_FADE = 1.0f / 1800.0f;
constexpr float LESSON_CARRY = 2.0f;
constexpr float LESSON_FETCH = 0.6f;
/// Kneeling by a body, going through it.
constexpr float LOOT_TIME = 2.0f;
/// A starving human below this health gets beaten up rather than fed;
/// the greedy raise it by up to PREY_HEALTH_GREEDY.
constexpr float PREY_HEALTH = 0.35f;
constexpr float PREY_HEALTH_GREEDY = 0.25f;
/// Running steps, to a fight or away from one, take this much of a walking step's time.
constexpr float RUN = 0.7f;
/// A punch: fist up, then out and landing for some health, then back.
constexpr float PUNCH_WINDUP = 0.25f, PUNCH_FOLLOW = 0.2f;
constexpr float PUNCH_DAMAGE_MIN = 0.08f, PUNCH_DAMAGE_MAX = 0.16f;
/// The fist lunges this far, catching a foe who stepped away during the wind-up.
constexpr int PUNCH_REACH = 2;
constexpr float HURT_TIME = 0.15f;
/// A human hit keeps fighting back this long after the last blow it took,
/// and up to twice as long when it was a foe.
constexpr float GRUDGE_TIME = 4.0f;

/// Trust, -1 to 1: what each thing seen does to it, where it tips into a
/// friend or a foe, how fast it fades back toward nothing (a sociable human
/// forgives and forgets up to half as fast again), and how much closer a
/// foe's cakes look than a stranger's, in cells, when hungry.
constexpr float TRUST_HIT = -0.4f;
constexpr float TRUST_SAW_HIT = -0.15f;
constexpr float TRUST_FED = 0.5f;
constexpr float TRUST_SAW_FEED = 0.1f;
constexpr float TRUST_GREETED = 0.1f;
constexpr float TRUST_TOLD = 0.1f;
constexpr float TRUST_SAW_LOOT = -0.1f;
/// What being paid for a tip does, what a told cake or key that isn't
/// there does, and what a trap on a told route does.
constexpr float TRUST_PAID = 0.1f;
constexpr float TRUST_STALE = -0.15f;
constexpr float TRUST_LIED = -0.6f;
/// Greedy from here on, a human keeps cake tips from all but friends and
/// sells trap tips; greedy from here and brave from CUNNING_BRAVE, it lies
/// to foes.
constexpr float STINGY = 0.67f;
constexpr float CUNNING_GREEDY = 0.5f;
constexpr float CUNNING_BRAVE = 0.4f;
constexpr float TRUST_FRIEND = 0.4f;
constexpr float TRUST_FOE = -0.4f;
constexpr float TRUST_FADE = 1.0f / 900.0f;
constexpr int TRUST_STEPS = 10;
/// Below this health a human runs from a fight, unless starving: the
/// timid at one and a half times this, the fearless at half.
constexpr float FLEE_HEALTH = 0.35f;
/// It runs this far from who hit it, and keeps away from them for a while.
constexpr int FLEE_DIST = 10;
/// How long a fright lasts, for a middling human: the timid stay scared
/// half as long again, the fearless half as long.
constexpr float FEAR_TIME = 20.0f;
/// Dread, 0 to 1: what each scare adds for a middling human (the timid
/// half as much again, the fearless half), how much fades per second (the
/// brave faster, and twice as fast seated), and what it does: taken off
/// the wish to explore and added to the wish to stroll, how much shorter
/// a stroll, how much sooner a fight is run from, how much likelier to sit.
constexpr float DREAD_HIT = 0.4f;
constexpr float DREAD_SAW_HIT = 0.15f;
constexpr float DREAD_TRAP_HIT = 0.5f;
constexpr float DREAD_TRAP_SEEN = 0.25f;
/// A body seen: fresh, whoever did it may be close; rotting, less so; bare
/// bones, long over.
constexpr float DREAD_CORPSE_FRESH = 0.4f;
constexpr float DREAD_CORPSE = 0.3f;
constexpr float DREAD_BONES = 0.15f;
constexpr float DREAD_FADE = 1.0f / 120.0f;
constexpr float DREAD_SIT_FADE = 2.0f;
constexpr float DREAD_EXPLORE = 0.4f;
constexpr float DREAD_WANDER = 0.5f;
constexpr float DREAD_SHORT = 0.5f;
constexpr float DREAD_FLEE = 0.5f;
constexpr float DREAD_SIT = 0.5f;
/// Taken off the wish to go through a body.
constexpr float DREAD_LOOT = 0.5f;
/// Guilt over flesh eaten, at its fullest: taken off the wish to meet,
/// follow and escort others, and how much likelier it sits alone. The rest
/// of flesh and the mind is in human_flesh.cpp.
constexpr float GUILT_ALONE = 0.5f;
constexpr float GUILT_SIT = 0.5f;
/// How much each thing strikes a human, as an episode to talk about later
/// (0 to 1; human_episode.cpp): done, done to it, or only seen.
constexpr float EPISODE_WOKE = 0.5f;
constexpr float EPISODE_HIT = 0.4f, EPISODE_GOT_HIT = 0.6f, EPISODE_ROBBED = 0.7f, EPISODE_SAW_FIGHT = 0.4f;
constexpr float EPISODE_KILLED = 0.9f, EPISODE_SAW_KILL = 0.9f;
constexpr float EPISODE_BODY = 0.7f, EPISODE_FRIEND_BODY = 1.0f;
constexpr float EPISODE_SPIKES = 1.0f, EPISODE_OWN_TRAP = 0.8f, EPISODE_DART = 0.7f, EPISODE_SAW_TRAP = 0.6f;
constexpr float EPISODE_SPOTTED = 0.3f;
constexpr float EPISODE_GAVE = 0.4f, EPISODE_GIVEN = 0.6f, EPISODE_PAID = 0.3f, EPISODE_YIELDED = 0.5f;
constexpr float EPISODE_SAW_GIFT = 0.3f;
constexpr float EPISODE_DOOR = 0.4f, EPISODE_SAW_DOOR = 0.3f;
constexpr float EPISODE_LOOTED = 0.4f, EPISODE_SAW_LOOT = 0.35f;
constexpr float EPISODE_FRIEND = 0.6f, EPISODE_FOE = 0.6f;
constexpr float EPISODE_LIED = 0.7f, EPISODE_STALE_TIP = 0.4f;
constexpr float EPISODE_NEED = 0.5f;
/// Keeping company (human_company.cpp): wanting out adds this much to the
/// wish, times how much it wants out, and keeping with a friend this much.
constexpr float COMPANY_OUT = 0.3f;
constexpr float COMPANY_FRIEND = 0.3f;
/// And keeping with someone, this much less wish to go exploring alone.
constexpr float COMPANY_STAY = 0.3f;
/// Below this much bravery a human never picks a fight for cakes unless
/// starving, and never goes through a body but for the cakes on it when hungry.
constexpr float TIMID = 0.3f;
/// Greeting, the chance a human gives its name: this much, plus this much
/// more for the most sociable, plus this much again when the other has just
/// given theirs. A friend always gets it.
constexpr float INTRODUCE_BASE = 0.2f;
constexpr float INTRODUCE_SOCIABLE = 0.6f;
constexpr float INTRODUCE_RETURN = 0.3f;
/// Gossip: two who trust each other this much both ways talk about others.
/// Well under TRUST_FRIEND, which humans rarely reach yet: in test runs the
/// closest pairs got to about 0.2 each way in half an hour. An opinion this
/// firm is worth passing on, and worth passing on again once it has moved
/// this far since last told; the listener takes on this much of it, times
/// how far it trusts the teller.
constexpr float TRUST_CONFIDE = 0.15f;
constexpr float GOSSIP_WORTH = 0.2f;
/// The cunning speak of anyone they are wary of as this dangerous; a
/// lie when it is this much worse than they really think.
constexpr float SLANDER = -0.8f;
constexpr float SLANDER_LIE = 0.3f;
/// A false tip sends a foe at least this far, to a table it has never seen.
constexpr int FALSE_TIP_FAR = 10;
/// How much more a cunning human wants to go up to a foe worth lying to
/// (the only foes it would greet) than to anyone else.
constexpr float LIE_URGE = 0.5f;
/// Lies for gain: a hungry rival this close to a cake the liar wants is
/// sent away from it; spikes that aren't there are sold at a floor cell
/// this far from the buyer.
constexpr int RIVAL_NEAR = 20;
constexpr int FAKE_TRAP_NEAR = 4, FAKE_TRAP_FAR = 15;
/// Only the greediest of the cunning lie for gain.
constexpr float GAIN_LIE_GREEDY = 0.6f;
constexpr float GOSSIP_NEWS = 0.15f;
constexpr float GOSSIP_WEIGHT = 0.6f;
/// Talking: a line is heard this far off, in cells, by anyone who can see
/// the speaker. It shows over the head this long, and a little longer per
/// letter; this many at most wait their turn, the rest go unsaid. Between
/// lines, one's own or the other's, comes this pause; a line kept waiting
/// this long by others talking goes unsaid (a cry, this long).
constexpr int HEAR_RADIUS = 6;
constexpr float SPEECH_TIME = 1.2f, SPEECH_PER_LETTER = 0.05f;
constexpr size_t SPEECH_QUEUE = 3;
constexpr float SPEECH_PAUSE = 0.35f;
constexpr float SPEECH_PATIENCE = 5.0f, CRY_PATIENCE = 0.5f;
// Word to pass on (a tell, a name) waits as long as a greeting can last.
constexpr float WORD_PATIENCE = GREET_LONGEST;
/// What a warm or a rude line does to the trust of whoever it is said to,
/// and how much a frightened line scares a listener who neither trusts nor
/// distrusts the speaker (up to twice that for a full friend).
constexpr float TRUST_WARM = 0.04f;
constexpr float TRUST_RUDE = -0.06f;
constexpr float DREAD_TALK = 0.1f;
/// Begging: the chance a middling human begs a holder close by when hungry
/// (the timid, the sociable and the generous likelier), and how much
/// likelier when starving; it begs the same human again only this much
/// later, and waits this long for the cake.
constexpr float BEG_CHANCE = 0.4f;
constexpr float BEG_STARVING = 0.3f;
constexpr float BEG_AGAIN = 30.0f;
constexpr float BEG_WAIT_MIN = 2.5f, BEG_WAIT_MAX = 3.5f;
/// Begged, the chance a middling holder is moved (the sociable and the
/// generous likelier), this much likelier by a friend, and for how long.
/// Moved, it shares with the beggar as with a friend, hungry or starving.
constexpr float PLEA_MOVES = 0.5f;
constexpr float PLEA_FRIEND = 0.3f;
constexpr float MOVED_TIME = 30.0f;
/// Threats: made this close, at this chance plus this much times how brave
/// and greedy the threatener is on average, to the same human again only
/// this much later; it waits this long for the cake.
constexpr int THREAT_RANGE = 3;
constexpr float THREAT_BASE = 0.1f, THREAT_BOLD = 0.8f;
constexpr float THREAT_AGAIN = 30.0f;
constexpr float THREAT_WAIT_MIN = 0.8f, THREAT_WAIT_MAX = 1.2f;
/// The chance the most timid human hands a cake over when threatened (none
/// for the fearless), up to twice that when frightened, never when
/// starving. Not handing it over, it thinks this much worse of the
/// threatener; anyone else hearing it, this much.
constexpr float YIELD_CHANCE = 0.7f;
constexpr float TRUST_THREAT = -0.12f;
constexpr float TRUST_HEARD_THREAT = -0.03f;

/// How much a human wants each thing it might set out to do, when nothing
/// presses: a base, plus what each trait adds at its fullest, 0 to 1 each.
/// Hunger adds up to 1 more to Food and Track, which only the hungry want,
/// and to Flesh and Hunt, which only those hungry enough for them want, with
/// appetite on top, so a taste for flesh puts it before cakes.
/// OpenDoor sits a little under Explore so that, by default, key holders
/// pick a chunk clean before unlocking it; wanting out adds to both alike
/// (OUT_EXPLORE). The order of Human::Want.
struct Wanting {
    float base, brave, greedy, curious, sociable;
};
constexpr Wanting WANTING[] = {
    {2.00f, 0.0f, 0.3f, 0.0f, 0.0f}, // Food
    {2.00f, 0.0f, 0.0f, 0.0f, 0.0f}, // Drink
    {0.80f, 0.0f, 0.0f, 0.0f, 0.0f}, // Relieve
    {2.00f, 0.0f, 0.0f, 0.0f, 0.0f}, // Sleep
    {1.95f, 0.0f, 0.0f, 0.0f, 0.0f}, // Flesh
    {1.92f, 0.0f, 0.0f, 0.0f, 0.0f}, // Hunt
    {1.90f, 0.3f, 0.0f, 0.0f, 0.0f}, // Track
    {0.50f, 0.0f, 0.0f, 0.0f, 0.5f}, // Escort
    {0.85f, -0.3f, 0.0f, 0.0f, 0.0f}, // Wash
    {0.30f, 0.0f, 0.0f, 0.0f, 0.6f}, // Meet
    {1.50f, 0.2f, 0.0f, 0.0f, 0.0f}, // Rescue
    {0.70f, 0.3f, 0.0f, 0.0f, 0.0f}, // UrgedDoor
    {0.40f, 0.2f, 0.45f, 0.0f, -0.15f}, // Loot
    {0.30f, 0.2f, 0.6f, 0.0f, -0.2f}, // Rob
    {0.45f, 0.0f, 0.25f, 0.25f, 0.0f}, // Fetch
    {0.35f, 0.0f, 0.0f, 0.0f, 0.45f}, // Follow
    {0.15f, 0.0f, 0.0f, 0.0f, 0.70f}, // Company
    {0.25f, 0.0f, 0.0f, 0.35f, 0.0f}, // Explore
    {0.20f, 0.0f, 0.0f, 0.35f, 0.0f}, // OpenDoor
    {0.10f, 0.0f, 0.0f, 0.2f, 0.0f}, // CheckDoor
    {0.05f, 0.0f, 0.0f, 0.0f, 0.0f}, // Wander
};
static_assert(std::size(WANTING) == size_t(Human::Want::Count));
/// Hungry with nothing to eat, or frightened, a key holder weighs this
/// often whether to make for a locked door before exploring: at least
/// DOOR_URGE_HUNGRY likely when hungry, sure when starving, and up to
/// DOOR_URGE_FEAR likely when as frightened as can be.
constexpr float DOOR_URGE_EVERY = 10.0f;
constexpr float DOOR_URGE_HUNGRY = 0.2f;
constexpr float DOOR_URGE_FEAR = 0.6f;
/// Chance, each step, to spot a hidden trap on a cell beside it.
constexpr float NOTICE_TRAP_CHANCE = 0.35f;
/// Leading someone wounded to water, it waits for them once they fall this
/// far behind, for a moment at a time.
constexpr int ESCORT_GAP = 3;
constexpr float ESCORT_WAIT_MIN = 0.3f, ESCORT_WAIT_MAX = 0.5f;
/// Darts leave at least this much health: they never kill.
constexpr float DART_SURVIVE_HEALTH = 0.05f;
/// Health a fresh wound bleeds per second, per damage, by kind: Blunt,
/// Pierce, Slash. It bleeds less and less as it heals.
constexpr float BLEED_PER_DAMAGE[] = {0.02f, 0.04f, 0.04f};
/// Seconds a wound takes to heal, per damage.
constexpr float HEAL_PER_DAMAGE = 60.0f;
/// Starving, wounds heal this much slower, so they bleed for longer.
constexpr float STARVE_HEAL_MUL = 0.5f;
/// Blows taken are forgotten this fast; past BEATING_BLEED, about three in
/// a few seconds, the beating bleeds.
constexpr float BEATING_DECAY = 0.06f;
constexpr float BEATING_BLEED = 0.3f;
/// Spikes leave at most this much health, and a wound this deep that bleeds
/// this many times harder than another: most never stop bleeding in time.
constexpr float SPIKE_LEFT = 0.25f;
constexpr float SPIKE_WOUND_MIN = 0.35f, SPIKE_WOUND_MAX = 0.55f;
constexpr float SPIKE_BLEED_MUL = 1.5f;
/// The dead bleed this much longer at most, less and less.
constexpr float CORPSE_BLEED_TIME = 8.0f;

/// What a human can take from where it stands: its own cell, and the tables round it.
constexpr Vec2i REACH[] = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};

/// A trap as the log names it.
inline const char* trapWord(TrapKind kind) {
    switch (kind) {
    case TrapKind::Spikes: return "SPIKES";
    case TrapKind::Slam: return "A SLAM ROOM";
    case TrapKind::Darts: return "DARTS";
    }
    return "A TRAP";
}

/// Too hurt to fight on; the starving fight anyway.
inline bool weak(const Human& h) { return h.health < h.fleeHealth() && h.hunger < 1.0f; }

/// A trait's pull on a tuning value: 1 - half at 0, 1 + half at 1.
inline float sway(float trait, float half = 0.5f) { return 1.0f + (trait - 0.5f) * 2.0f * half; }

/// Everyone alive who can see the actor, apart from the two involved, thinks
/// of the actor this much differently for what they did to the other; those
/// who count the other a foe think nothing of it.
inline void witness(World& world, const Human& actor, const Human& other, float delta, float scare, Episode::What what,
                    float struck, int detail = 0) {
    for (const auto& h : world.humans()) {
        if (h.get() == &actor || h.get() == &other || h->dead() || !h->sight.sees(actor.cell)) continue;
        h->scare(scare);
        h->rememberAt(actor.cell, what, struck, actor.id, other.id, detail);
        if (h->isFoe(other.id)) continue;
        h->regard(actor.id, delta);
    }
}

} // namespace
