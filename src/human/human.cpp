#include "human/human.h"
#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <queue>

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

int cheb(Vec2i a, Vec2i b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

/// What a human can take from where it stands: its own cell, and the tables round it.
constexpr Vec2i REACH[] = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};

/// A trap as the log names it.
const char* trapWord(TrapKind kind) {
    switch (kind) {
    case TrapKind::Spikes: return "SPIKES";
    case TrapKind::Slam: return "A SLAM ROOM";
    case TrapKind::Darts: return "DARTS";
    }
    return "A TRAP";
}

/// Too hurt to fight on; the starving fight anyway.
bool weak(const Human& h) { return h.health < h.fleeHealth() && h.hunger < 1.0f; }

/// A trait's pull on a tuning value: 1 - half at 0, 1 + half at 1.
float sway(float trait, float half = 0.5f) { return 1.0f + (trait - 0.5f) * 2.0f * half; }

/// Everyone alive who can see the actor, apart from the two involved, thinks
/// of the actor this much differently for what they did to the other; those
/// who count the other a foe think nothing of it.
void witness(World& world, const Human& actor, const Human& other, float delta, float scare, Episode::What what,
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

Human::Human(int id, const HumanLook& look, Vec2i cell, uint64_t seed, std::string chosenName)
    : id(id), name(chosenName.empty() ? randomName(seed) : std::move(chosenName)), look(look), traits(Traits::random(seed)), cell(cell), prev(cell), stains(look), dirt(look), soaked(look),
      rng_(seed), target_(cell), trod_(cell) {
    hunger = rng_.range(0.0f, START_HUNGER_MAX);
    thirst = rng_.range(0.0f, START_THIRST_MAX);
    tiredness = rng_.range(0.0f, START_TIRED_MAX);
    timer = rng_.range(WAKE_LIE_MIN, WAKE_LIE_MAX);
    hunch_ = rng_.range(0.0f, TAU);
}

void Human::note(std::string what, int who, int whom) { log.add(born + clock_, std::move(what), who, whom); }

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
        if (animTime >= LIE_DOWN_TIME) sayings_.clear();
        speak(world, dt);
        if (hurtTime <= 0.0f) return sleep(world, dt);
        wakeUp(world, true, -1, "WOKE UP HURT");
    }
    speak(world, dt);
    chatUrges(world, dt);
    callForHelp(world, dt);
    timer -= dt;
    // A hit is answered at once, or as soon as the step under way is done.
    if (provoked_ && stunned_ <= 0.0f && (!moving || timer <= 0.0f)) {
        react(world);
        if (state == State::Walking) step(world);
        return;
    }
    if (state == State::Waking) {
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
        if ((rethink_ && !comingTo_) || timer <= 0.0f) startRising();
        return;
    }
    if (timer > 0.0f) return;
    switch (state) {
    case State::Greeting: {
        // Not walking off mid-sentence, while the other still stands there.
        const Human& p = world.human(partner_);
        bool with = !p.dead() && p.state == State::Greeting && p.partner_ == id;
        if (with && (talking() || p.talking()) && timer > GREET_TIME - GREET_LONGEST) return;
        endGreet(world);
        return;
    }
    case State::Eating:
        endEating();
        return;
    case State::Drinking:
        return drink(world);
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
        return goal == Goal::Fight || goal == Goal::Flee ? Action::Run : Action::Walk;
    case State::Greeting:
        return Action::Greet;
    case State::Eating:
        return Action::Eat;
    case State::Drinking:
        return Action::Drink;
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
    if (how >= FRIGHT_WET) frightWet_ = true;
}

float Human::fearTime() const { return FEAR_TIME * sway(1.0f - traits.brave); }

int Human::carryMax() const {
    return CARRY_MAX + int(std::lround((traits.greedy - 0.5f) * 2.0f + LESSON_CARRY * lessons.hoard));
}

float Human::preyHealth() const { return PREY_HEALTH + PREY_HEALTH_GREEDY * traits.greedy; }

// The generous share from two cakes, the grasping from three.
int Human::spareFrom() const { return traits.greedy < 0.67f ? 1 : 2; }

float Human::trust(int id) const {
    auto it = trust_.find(id);
    return it == trust_.end() ? 0.0f : it->second;
}

void Human::regard(int id, float delta) {
    if (id == this->id) return;
    float& t = trust_[id];
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

void Human::live(World& world, float dt) {
    bool wasHungry = hungry(), wasStarving = starving();
    bool wasThirsty = thirsty(), wasParched = parched();
    hunger = std::min(1.0f, hunger + HUNGER_PER_SECOND[size_t(action())] * dt);
    thirst = std::min(1.0f, thirst + THIRST_PER_SECOND[size_t(action())] * dt);
    tire(dt);
    press(dt);
    if (hunger >= 1.0f) health -= STARVE_RATE * dt;
    if (thirst >= 1.0f) health -= PARCH_RATE * dt;
    if (!hungry() && !thirsty() && wounds.empty() && sick_ <= 0.0f)
        health = std::min(1.0f, health + HEAL_RATE * (state == State::Sleeping ? SLEEP_HEAL : 1.0f) * dt);
    // Darts' bleeding stops short of killing, as darts do.
    float lethal, bled = bleedOut(dt, lethal);
    health -= lethal;
    if (health > DART_SURVIVE_HEALTH) health = std::max(DART_SURVIVE_HEALTH, health - (bled - lethal));
    beating_ = std::max(0.0f, beating_ - BEATING_DECAY * dt);
    hurtTime = std::max(0.0f, hurtTime - dt);
    grudge_ -= dt;
    stunned_ = std::max(0.0f, stunned_ - dt);
    fear_ -= dt;
    daze_ = std::max(0.0f, daze_ - dt / DAZE_TIME);
    out_ = std::max(OUT_REST, out_ - (1.0f - OUT_REST) / OUT_SETTLE * dt);
    dread_ = std::max(0.0f, dread_ - DREAD_FADE * sway(traits.brave) * (state == State::Sitting ? DREAD_SIT_FADE : 1.0f) * dt);
    float fade = TRUST_FADE * sway(traits.sociable) * dt;
    for (auto& [who, t] : trust_) t = t > 0.0f ? std::max(0.0f, t - fade) : std::min(0.0f, t + fade);
    urgeWait_ -= dt;
    // Without hope, what going without taught it sticks.
    lessons.hoard = std::max(0.0f, lessons.hoard - LESSON_FADE * (1.0f - LESSON_GLOOM * gloom()) * dt);
    lessons.keysFirst = std::max(0.0f, lessons.keysFirst - LESSON_FADE * dt);
    lessons.nearWater = std::max(0.0f, lessons.nearWater - LESSON_FADE * dt);
    lessons.safeSleep = std::max(0.0f, lessons.safeSleep - LESSON_FADE * dt);
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

void Human::learn(float& lesson, bool hard) {
    lesson = std::min(1.0f, lesson + (hard ? LESSON_HARD : LESSON_PAID));
    if (!hard) return;
    if (&lesson == &lessons.safeSleep) {
        note("LEARNT TO FIND A SAFE PLACE TO SLEEP IN TIME");
        return;
    }
    if (&lesson == &lessons.nearWater) {
        note("LEARNT TO KEEP NEAR WATER");
        sayLater(talk::Moment::LearnWater);
        return;
    }
    bool cakes = &lesson == &lessons.hoard;
    note(cakes ? "LEARNT TO KEEP MORE CAKES" : "LEARNT TO TAKE KEYS FIRST");
    sayLater(cakes ? talk::Moment::LearnCakes : talk::Moment::LearnKeys);
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
    sight.look(world, cell, int(std::lround(DAZE_SIGHT + (Sight::RADIUS - DAZE_SIGHT) * (1.0f - daze_))));
    bool news = false;
    // The first key, cake, door, water, human and someone else's footprints
    // it ever sees are remarked on, one a look so they don't all come out
    // at once, and not before it has come to.
    bool remarked = state == State::Waking;
    auto firstSight = [&](talk::Moment m, int about = -1) {
        unsigned bit = 1u << (int(m) - int(talk::Moment::FirstKey));
        if (remarked || (firstSights_ & bit)) return;
        remarked = true;
        if (sayLater(m, -1, about)) firstSights_ |= bit;
    };
    sight.forEach([&](Vec2i c) {
        Tile t = world.tile(c);
        if (t == Tile::DoorLocked || t == Tile::DoorOpen) firstSight(talk::Moment::FirstDoor);
        if (drinkable(t)) firstSight(talk::Moment::FirstWater);
        if (t != Tile::Void) {
            if (memory.tile(c) == Tile::Void) {
                lastFound_ = clock_;
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
                if (d.kind == MessKind::Print && d.who != id) firstSight(talk::Moment::FirstPrints, d.who);
                if (memory.mess.insert(i).second && !isErratic() && d.who != id)
                    scare(DISGUST_DREAD * modesty() * Mess::disgust(d, world.time()));
            }
        if (const Stain* b = world.blood().at(c)) {
            memory.blood[c] = *b;
            news |= noticeBlood(world, c);
        }
        if (world.hasKey(c)) {
            firstSight(talk::Moment::FirstKey);
            news |= memory.keys.insert(c).second;
            checkTold(c, true);
        } else if (memory.keys.erase(c)) {
            checkTold(c, false);
        }
        if (world.hasCake(c)) {
            firstSight(talk::Moment::FirstCake);
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
    for (const auto& o : world.humans()) {
        if (o.get() == this || !sight.sees(o->cell)) continue;
        if (!o->dead()) inSight_.push_back(o->id);
        if (!o->dead()) firstSight(talk::Moment::FirstHuman, o->id);
        if (!o->dead()) noticeFilth(*o);
        // Died asleep, from farther off it still looks asleep.
        if (o->dead() && o->diedAsleep_ && !seenDead_.count(o->id) && cheb(cell, o->cell) > STILL_SEEN) continue;
        if (o->dead()) {
            seenCakes_.erase(o->id);
            Body& b = bodies_[o->id];
            b.cell = o->cell;
            b.cakes = o->cakes;
            b.meals = o->meals();
            b.spoiled = o->spoiled();
            seeRemains(*o);
            if (seenDead_.insert(o->id).second) {
                // Bare bones could be anyone's: not even a friend's are known.
                bool known = !o->bones() || o->killer_ == id;
                if (!known) nameless_.insert(o->id);
                bool friendDead = known && isFriend(o->id);
                scare(o->bones() ? DREAD_BONES : o->spoiled() ? DREAD_CORPSE : DREAD_CORPSE_FRESH);
                lift(friendDead ? HOPE_FRIEND_DEAD : o->bones() ? HOPE_BONES : HOPE_BODY);
                if (o->killer_ == id && !o->killerSaw_) {
                    note("FOUND @ DEAD. MY BEATING DID IT", o->id);
                    rememberAt(o->cell, Episode::What::Kill, EPISODE_KILLED, id, o->id);
                } else if (o->killer_ != id) {
                    if (known)
                        note(o->diedAsleep_ ? "FOUND @ DEAD. I THOUGHT THEY WERE ASLEEP" : "FOUND @ DEAD", o->id);
                    // Taken for asleep, it tries to wake them first.
                    if (known && o->diedAsleep_) sayLater(talk::Moment::WakeThem, -1, o->id);
                    else
                        note("FOUND SOMEONE'S BONES");
                    rememberAt(o->cell, Episode::What::Body, friendDead ? EPISODE_FRIEND_BODY : EPISODE_BODY, -1,
                               known ? o->id : -1);
                    sayLater(talk::Moment::FoundBody, -1, o->id);
                }
                news = true;
            }
            continue;
        }
        lastSeen_[o->id] = o->cell;
        bool knewCakes = seenCakes_.count(o->id) > 0;
        if (o->cakes > 0)
            seenCakes_[o->id] = o->cakes;
        else
            seenCakes_.erase(o->id);
        news |= !met.count(o->id);
        news |= hungry() && cakes == 0 && o->cakes > 0 && !knewCakes;
        news |= o->starving() && cakes > 1 && !starving();
        news |= judge(*o);
    }
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
    robbee_ = -1;
    if (goal != Goal::OpenDoor) urged_ = false;
    takeInReach(world);
    if (dead()) return;
    if (foe_ >= 0 && grudge_ > 0.0f && startFight(world)) return;
    // Worn out: drops asleep where it stands, once the fight is over.
    if (tiredness >= 1.0f) return startSleeping(world, true);
    // Killed to eat: eats.
    if (prey_ >= 0 && foe_ != prey_) {
        if (world.human(prey_).dead() && startFeed(world, prey_)) return;
        prey_ = -1;
    }
    // Can't hold on: the erratic go where they stand, the rest have an
    // accident and carry on.
    if (bursting()) {
        if (isErratic()) return startRelieving(world);
        accident(world, false);
    }
    if (hungry() && cakes > 0) return startEating();
    if (thirsty())
        if (auto w = waterBeside(cell); w && wouldDrink(world, cell, *w)) return startDrinking(*w);
    if (thirsty()) askGroupForWater(world);
    if (helpStarving(world)) return;
    if (foe_ >= 0 && startFight(world)) return;
    if (keepAway(world)) return;
    if (leadEscort(world) || followEscort(world)) return;
    if (plead(world)) return;
    if (guardHome(world)) return;
    if (givePrivacy(world)) return;
    if (keepWatch(world)) return;
    if (!hungry() && !thirsty() && !needsToGo() && waitForCompany(world)) return;

    // Someone new to greet, unless hungry or thirsty, when nobody is.
    bool needy = hungry() || thirsty() || needsToGo();
    if (needy) {
        partner_ = -1;
    } else if (Human* o = unmetInSight(world)) {
        if (o->id != partner_) partnerSince_ = clock_;
        partner_ = o->id;
    } else if (goal != Goal::Meet || (greetedAt_.count(partner_) && greetedAt_[partner_] >= partnerSince_)) {
        partner_ = -1;
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
    else if (float back = headBack(); back > 0.0f)
        want(Want::Sleep, back - WANTING[size_t(Want::Sleep)].base);
    // Up to RELIEF_DRIVE more as it nears bursting.
    if (needsToGo())
        want(Want::Relieve, RELIEF_DRIVE * (std::max(bladder, bowels) - needBar()) / (1.0f - needBar()));
    if (!needy) {
        // Guilt keeps it to itself.
        want(Want::Escort, -GUILT_ALONE * guilt_ + parchedToLead(world));
        want(Want::Wash, wantsWashing() ? FILTH_WASH * (0.5f + modesty()) : 0.0f);
        if (partner_ >= 0) want(Want::Meet, OUT_ASK * outDrive() - GUILT_ALONE * guilt_);
    }
    if (hunger >= fleshBar()) want(Want::Flesh, drive + appetite_);
    if (hunger >= huntBar()) want(Want::Hunt, drive + appetite_);
    if (rescuee_ >= 0) want(Want::Rescue);
    if (keys > 0 && (urged_ || doorUrge())) want(Want::UrgedDoor);
    if (traits.brave >= TIMID &&
        std::any_of(bodies_.begin(), bodies_.end(), [&](const auto& b) { return worthLooting(b.first); }))
        want(Want::Loot, -DREAD_LOOT * dread_);
    if (std::any_of(world.humans().begin(), world.humans().end(), [&](const auto& o) { return wouldRob(world, *o); }))
        want(Want::Rob, ROB_TEMPT + ROB_HUNGRY * drive);
    bool fetchCakes = cakes < carryMax() && !memory.cakes.empty();
    if (fetchCakes || !memory.keys.empty())
        want(Want::Fetch, LESSON_FETCH * std::max(fetchCakes ? lessons.hoard : 0.0f,
                                                  memory.keys.empty() ? 0.0f : lessons.keysFirst));
    // Settled, it has stopped looking for a way out, and follows nobody away.
    if (!settled_) {
        if (leader_ >= 0) want(Want::Follow, -GUILT_ALONE * guilt_);
        // A group goes looking too, so keeping with one shares in wanting
        // out; a friend there pulls harder.
        else if (companion_ >= 0 || pickCompany(world) >= 0)
            want(Want::Company, COMPANY_OUT * outDrive() - GUILT_ALONE * guilt_ +
                                    (companion_ >= 0 && isFriend(companion_) ? COMPANY_FRIEND : 0.0f));
        // Keeping company, it leaves the looking to whoever leads.
        want(Want::Explore, OUT_EXPLORE * outDrive() - DREAD_EXPLORE * dread_ -
                                (companion_ >= 0 && leader_ < 0 ? COMPANY_STAY : 0.0f) - waterTether() - sleepTether());
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
        return seekWater(world);
    case Want::Relieve:
        return seekRelief(world);
    case Want::Sleep:
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
    case Want::Meet:
        return partner_ >= 0 && startMeet(world);
    case Want::Rescue:
        return startRescue(world);
    case Want::UrgedDoor: {
        // Frightened, never a door that lies toward whoever scared it.
        auto it = lastSeen_.find(feared_);
        bool afraid = fear_ > 0.0f && it != lastSeen_.end();
        if (auto p = search(world, [&](Vec2i c) {
                auto d = lockedDoorBeside(c);
                return d && !(afraid && cheb(*d, it->second) < cheb(cell, it->second));
            }, afraid, [&](Vec2i c) { return hunchCost(c, HUNCH_DOOR_COST); })) {
            if (outUrged_ && !urgedNoted_) note("TRYING A DOOR. THERE HAS TO BE A WAY OUT");
            urgedNoted_ = outUrged_;
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
        bool wantCakes = cakes < carryMax() && !memory.cakes.empty();
        auto p = search(world, [&](Vec2i c) {
            return inReach(c, memory.keys) || (wantCakes && inReach(c, memory.cakes));
        });
        if (!p) return false;
        if (!inReach(p->empty() ? cell : p->back(), memory.keys)) {
            startWalking(Goal::FetchCake, *p);
            return true;
        }
        if (leader_ >= 0) sayLater(talk::Moment::PartOwnKey, leader_);
        leader_ = -1;
        startWalking(Goal::FetchKey, *p);
        return true;
    }
    case Want::Follow:
        return leader_ >= 0 && startFollow(world);
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
    case Want::OpenDoor:
        if (keys <= 0) return false;
        if (auto p = search(world, [&](Vec2i c) { return lockedDoorBeside(c).has_value(); }, false,
                            [&](Vec2i c) { return hunchCost(c, HUNCH_DOOR_COST); })) {
            startWalking(Goal::OpenDoor, *p);
            return true;
        }
        return false;
    case Want::CheckDoor:
        if (auto p = search(world, [&](Vec2i c) { return doorDue(c); })) {
            startWalking(Goal::CheckDoor, *p);
            return true;
        }
        return false;
    case Want::Wander: {
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

// One cell along the path. A cell in a slot nobody has entered yet is
// generated just before the human steps into it. If someone stands on the
// next cell, the path is replanned around everyone in the way.
void Human::step(World& world) {
    moving = false;
    // Stepping aside, it thinks again once out of the way.
    if (rethink_ && goal != Goal::GiveWay) {
        choose(world);
        if (state != State::Walking) return;
    }
    if (goal == Goal::Meet && partner_ >= 0) {
        Human& o = world.human(partner_);
        if (sight.sees(o.cell) && cheb(cell, o.cell) <= GREET_RADIUS) return tryGreet(world, o);
    }
    if (goal == Goal::Fight && foe_ >= 0) {
        Human& f = world.human(foe_);
        if (!f.dead() && sight.sees(f.cell) && cheb(cell, f.cell) <= 1) return startPunch(f);
    }
    if (goal == Goal::Share && sharee_ >= 0) {
        Human& o = world.human(sharee_);
        if (wouldShare(o) && sight.sees(o.cell) && cheb(cell, o.cell) <= 1) {
            give(o, Gift::Share);
            witness(world, *this, o, TRUST_SAW_FEED, 0.0f, Episode::What::Gift, EPISODE_SAW_GIFT, int(Gift::Share));
            return;
        }
    }
    // Leading someone to water, it lets them catch up.
    if (goal == Goal::Escort && escortee_ >= 0 && cheb(cell, world.human(escortee_).cell) > ESCORT_GAP)
        return startIdle(ESCORT_WAIT_MIN, ESCORT_WAIT_MAX);
    // Off on its own business ahead of its group, it lets them catch up.
    if ((goal == Goal::Explore || goal == Goal::Wander || goal == Goal::CheckDoor || goal == Goal::OpenDoor) &&
        waitForCompany(world))
        return;
    if (path_.empty()) return arrive(world);
    Vec2i next = path_.front();
    world.ensureGenerated(next);
    if (!passable(world.tile(next)) || avoids(next)) {
        // The world isn't what it remembered, say a gate slammed, or a trap
        // was found on the way: look again, then think again.
        observe(world);
        return startIdle(0.0f, LOOK_TIME_MIN);
    }
    if (Human* o = world.occupant(next); o && o != this) {
        // A sitter gets up for someone wanting past.
        if (o->state == State::Sitting) o->rethink_ = true;
        auto around = search(world, [&](Vec2i c) { return c == target_; }, true);
        if (!around || around->empty() || world.occupant(around->front())) {
            // No way round: after a moment one of the two steps aside, else
            // in the end it gives up and thinks again.
            if (stuck_ > 0 && makeWay(world, *o)) return;
            if (++stuck_ >= GIVE_UP_AFTER) return startIdle(0.0f, LOOK_TIME_MIN);
            timer = STEP_TIME * rng_.range(0.5f, 1.0f);
            return;
        }
        path_.assign(around->begin(), around->end());
        next = path_.front();
    }
    path_.pop_front();
    move(world, next);
}

// Two walking into each other: the shorter step aside wins, so whoever is
// nearer a side room or a corner backs into it. Someone just standing
// there is the one to move; nobody busy eating, greeting, fighting or
// seated is asked.
bool Human::makeWay(World& world, Human& o) {
    bool standing = o.state == State::Idle && !o.provoked_;
    bool blocked = o.state == State::Walking && !o.moving && o.goal != Goal::GiveWay;
    if (!standing && !blocked) return false;
    bool headOn = blocked && !o.path_.empty() && o.path_.front() == cell;
    auto theirs = o.asideFrom(world, *this);
    auto mine = headOn ? asideFrom(world, o) : std::nullopt;
    if (mine && (!theirs || mine->size() < theirs->size() || (mine->size() == theirs->size() && id < o.id))) {
        startWalking(Goal::GiveWay, *mine);
        return true;
    }
    if (!theirs) return false;
    o.startWalking(Goal::GiveWay, *theirs);
    o.timer = 0.0f;
    stuck_ = 0;
    timer = STEP_TIME;
    return true;
}

std::optional<std::vector<Vec2i>> Human::asideFrom(const World& world, const Human& passer) const {
    std::unordered_set<Vec2i, Vec2iHash> way(passer.path_.begin(), passer.path_.end());
    way.insert(passer.cell);
    return search(world, [&](Vec2i c) { return !way.count(c) && !world.occupant(c); }, true);
}

void Human::move(World& world, Vec2i next) {
    prev = cell;
    world.moveOccupant(cell, next, this);
    cell = next;
    moving = true;
    stuck_ = 0;
    if (next.x != prev.x) facingLeft = next.x < prev.x;
    bool running = goal == Goal::Fight || goal == Goal::Flee;
    stepTime = STEP_TIME * (running ? RUN : 1.0f) * (next.x != prev.x && next.y != prev.y ? DIAGONAL : 1.0f) *
               (1.0f + DAZE_SLOW * daze_) * (exhausted() ? EXHAUSTED_SLOW : 1.0f);
    timer = stepTime;
    world.setOff(cell, *this);
    if (dead()) return;
    takeInReach(world);
    enterChunk(world);
    bool news = observe(world);
    // A hidden trap beside it may catch its eye.
    for (Vec2i d : DIRS8)
        if (const Trap* t = world.trap(cell + d); t && !memory.traps.count(cell + d) && rng_.chance(NOTICE_TRAP_CHANCE)) {
            news |= memory.traps.emplace(cell + d, t->kind).second;
            note(std::string("SPOTTED HIDDEN ") + trapWord(t->kind));
            rememberAt(cell + d, Episode::What::Spotted, EPISODE_SPOTTED, -1, -1, int(t->kind));
        }
    checkDoorsNear();
    if (news && goal != Goal::PassDoor && goal != Goal::GiveWay) rethink_ = true;
    // Seen opened from afar: no need to walk up to it.
    if (goal == Goal::CheckDoor && !lockedDoorBeside(target_)) rethink_ = true;
    if (goal == Goal::Explore && !isFrontier(target_) && !openDoorBeside(target_)) rethink_ = true;
    if (goal == Goal::FetchCake && !inReach(target_, memory.cakes)) rethink_ = true;
    if (goal == Goal::Loot && !worthLooting(corpse_, traits.brave < TIMID)) rethink_ = true;
    if (goal == Goal::Feed && !wouldEat(meal_)) rethink_ = true;
    // They move too; keep the plan fresh.
    if (goal == Goal::Meet || goal == Goal::Follow || goal == Goal::Company || goal == Goal::Fight ||
        goal == Goal::Share)
        rethink_ = true;
}

void Human::arrive(World& world) {
    switch (goal) {
    case Goal::OpenDoor: {
        // Come to let someone out, it opens their gate whatever else is beside it.
        auto door = lockedDoorBeside(cell);
        bool rescue = rescuee_ >= 0 && std::abs(rescueGate_.x - cell.x) + std::abs(rescueGate_.y - cell.y) == 1 &&
                      memory.tile(rescueGate_) == Tile::DoorLocked;
        if (rescue) door = rescueGate_;
        if (door && keys > 0 && world.unlock(*door)) {
            keys--;
            note("UNLOCKED A DOOR");
            rememberAt(*door, Episode::What::Door, EPISODE_DOOR, id);
            for (const auto& h : world.humans())
                if (h.get() != this && !h->dead() && h->sight.sees(*door))
                    h->rememberAt(*door, Episode::What::Door, EPISODE_SAW_DOOR, id);
            lift(HOPE_DOOR);
            if (lessons.keysFirst > 0.0f) learn(lessons.keysFirst, false);
            observe(world);
            // Letting someone out, it waits for them at the gate.
            if (letOut(world, *door)) return startIdle(UNLOCK_TIME, UNLOCK_TIME * 2.0f);
            startPassDoor(*door);
            timer = UNLOCK_TIME;
            return;
        }
        return choose(world);
    }
    case Goal::Explore:
        if (auto door = openDoorBeside(cell)) return startPassDoor(*door);
        break;
    case Goal::Wander:
        return rest();
    case Goal::Wash:
        if (startWash(world)) return;
        return choose(world);
    case Goal::Drink:
        if (seekWater(world)) return;
        return choose(world);
    case Goal::Relieve:
        if (seekRelief(world)) return;
        return choose(world);
    case Goal::Sleep:
        if (seekSleep(world)) return;
        return choose(world);
    case Goal::CheckDoor:
        // Still locked: a look at it, then on to something else.
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    case Goal::Meet:
    case Goal::Follow:
    case Goal::Company:
    case Goal::FetchCake:
    case Goal::Fight:
    case Goal::Share:
        return choose(world);
    case Goal::Flee:
        return startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    case Goal::GiveWay:
        startIdle(GIVE_WAY_WAIT_MIN, GIVE_WAY_WAIT_MAX);
        goal = Goal::GiveWay;
        return;
    case Goal::Avoid:
    case Goal::Escort:
    case Goal::Escorted:
    case Goal::Track:
        return choose(world);
    case Goal::Rob:
        if (startRob(world)) return;
        return choose(world);
    case Goal::Loot:
        if (startLoot(world, traits.brave < TIMID)) return;
        return choose(world);
    case Goal::Feed:
        if (startFeed(world, meal_)) return;
        return choose(world);
    default:
        break;
    }
    if (rng_.chance(LOOK_AROUND_CHANCE))
        startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    else
        choose(world);
}

void Human::startIdle(float lo, float hi) {
    state = State::Idle;
    moving = false;
    stuck_ = 0;
    path_.clear();
    timer = rng_.range(lo, hi);
}

void Human::rest() {
    if (!rng_.chance(SIT_CHANCE * sway(1.0f - traits.curious, SIT_CURIOUS) * (1.0f + DREAD_SIT * dread_) *
                     (1.0f + GUILT_SIT * guilt_) * (1.0f + DESPAIR_SIT * gloom())))
        return startIdle(IDLE_TIME_MIN, IDLE_TIME_MAX);
    sitDown();
}

void Human::sitDown() {
    restedAt_ = clock_;
    startIdle(SIT_TIME_MIN, SIT_TIME_MAX);
    state = State::Sitting;
    animTime = 0.0f;
    glance_ = SIT_GLANCE;
}

void Human::sitUp() {
    state = State::Sitting;
    animTime = 0.0f;
    timer = rng_.range(WAKE_SIT_MIN, WAKE_SIT_MAX);
    glance_ = 0.0f;
    comingTo_ = true;
    note("WOKE UP, REMEMBERING NOTHING");
    remember(Episode::What::Woke, EPISODE_WOKE, id);
    sayLater(talk::Moment::Wake);
}

void Human::startRising() {
    comingTo_ = false;
    roused_ = false;
    state = State::Rising;
    animTime = 0.0f;
    timer = STAND_UP_TIME;
}

void Human::startWalking(Goal g, std::vector<Vec2i> path) {
    state = State::Walking;
    awaiting_ = false;
    goal = g;
    path_.assign(path.begin(), path.end());
    target_ = path.empty() ? cell : path.back();
}

// Straight through: the door, its other half, and one step in.
// Through a door, the way out feels a little more like the way it went.
void Human::startPassDoor(Vec2i door) {
    Vec2i d = door - cell;
    float hx = std::cos(hunch_) * (1.0f - HUNCH_FOLLOW) + float(d.x) * HUNCH_FOLLOW;
    float hy = std::sin(hunch_) * (1.0f - HUNCH_FOLLOW) + float(d.y) * HUNCH_FOLLOW;
    if (hx != 0.0f || hy != 0.0f) hunch_ = std::atan2(hy, hx);
    startWalking(Goal::PassDoor, {door, door + d, door + d * 2});
}

bool Human::startMeet(World& world) {
    Human& o = world.human(partner_);
    if (!wouldGreet(o)) {
        partner_ = -1;
        return false;
    }
    bool seen = sight.sees(o.cell);
    Vec2i at = seen ? o.cell : lastSeen_[o.id];
    if (seen && cheb(cell, at) <= GREET_RADIUS) {
        tryGreet(world, o);
        return true;
    }
    if (!seen && cheb(cell, at) <= 1) {
        // Got where they were; they're gone.
        partner_ = -1;
        return false;
    }
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= 1; });
    if (!p) {
        partner_ = -1;
        return false;
    }
    startWalking(Goal::Meet, *p);
    return true;
}

// Both wave; someone busy waving at another, or eating, is waited for.
void Human::tryGreet(World& world, Human& other) {
    if (other.dead()) {
        partner_ = -1;
        return startIdle(0.0f, 0.0f);
    }
    if (!other.approachable()) return startIdle(FOLLOW_WAIT_MIN, FOLLOW_WAIT_MAX);
    // It takes two: turned away, it leaves them be a while.
    if (!other.welcomes(*this)) {
        snubbed_[other.id] = clock_;
        partner_ = -1;
        note("@ WOULDN'T GREET ME", other.id);
        other.note("WOULDN'T GREET @", id);
        other.sayLater(talk::Moment::Refuse, id);
        // It stands there a moment, to hear it and maybe answer back.
        return startIdle(SNUBBED_WAIT_MIN, SNUBBED_WAIT_MAX);
    }
    greet(other);
    other.greet(*this);
    // Each decides whether to say who it is; being told a name invites one back.
    bool mine = wouldIntroduce(other, false);
    bool theirs = other.wouldIntroduce(*this, mine);
    if (mine) introduce(other);
    if (theirs) other.introduce(*this);
    // And each tells the other something, or lies to them.
    exchange(world, other);
    other.exchange(world, *this);
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
    for (const auto& [who, opinion] : trust_) {
        if (who == o.id || who == id || std::abs(opinion) < GOSSIP_WORTH) continue;
        auto h = o.memory.heard.find(Memory::gossipKey(id, who));
        float told = h == o.memory.heard.end() ? 0.0f : h->second;
        if (std::abs(opinion - told) < GOSSIP_NEWS) continue;
        t.people.push_back({who, nameFor(who), opinion});
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
    for (const auto& [who, t] : trust_) n += t >= TRUST_FRIEND;
    return n;
}

int Human::foeCount() const {
    int n = 0;
    for (const auto& [who, t] : trust_) n += t <= TRUST_FOE;
    return n;
}

void Human::exchange(World& world, Human& o) {
    if (auto word = gossipFor(o)) {
        tell(o, *word);
        gossiping = o.gossiping = true;
    }
    // One asks; the other answers, then may ask back.
    bool asked = std::any_of(o.sayings_.begin(), o.sayings_.end(), [&](const Saying& s) {
        return s.moment == talk::Moment::AskWay && s.to == id;
    });
    if (!asked && wouldAskWay(o)) {
        askedWay_.insert(o.id);
        if (rng_.chance(ASK_WAY_CHANCE)) sayLater(talk::Moment::AskWay, o.id);
    }
    if (auto lie = lieFor(world, o)) return tell(o, *lie);
    bool forPay = false;
    // Water ahead of anything else to the thirsty who'd sooner drink than
    // eat; else after it (human_thirst.cpp).
    bool waterFirst = o.thirsty() && o.thirst >= o.hunger;
    if (waterFirst)
        if (auto w = waterFor(world, o, forPay)) {
            w->sold = forPay;
            return tell(o, *w);
        }
    auto news = newsFor(o, forPay);
    if (!news && !waterFirst) news = waterFor(world, o, forPay);
    if (!news) return;
    news->sold = forPay;
    tell(o, *news);
}

std::optional<Human::Tell> Human::newsFor(const Human& o, bool& forPay) const {
    forPay = false;
    if (isFoe(o.id)) return std::nullopt;
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
    return outDrive() >= ASK_WAY_FROM && !isFoe(o.id) && !askedWay_.count(o.id);
}

// Lies and gloom are told however unsure; an honest hunch only when it
// has one worth the name.
void Human::answerWay(Human& asker) {
    auto t = wayFor(asker);
    if (t && (t->lie || t->gloomy || hunchSure_ >= WAY_UNSURE))
        tell(asker, *t);
    else
        sayLater(talk::Moment::NoIdea, asker.id);
    if (wouldAskWay(asker)) {
        askedWay_.insert(asker.id);
        if (rng_.chance(ASK_BACK_CHANCE)) sayLater(talk::Moment::AskWay, asker.id);
    }
}

// Settlers have seen the most, and say so.
std::optional<Human::Tell> Human::wayFor(const Human& o) const {
    bool foe = isFoe(o.id), cunning = traits.greedy >= CUNNING_GREEDY && traits.brave >= CUNNING_BRAVE;
    if (foe && !cunning) return std::nullopt;
    Tell t{Tell::Kind::Way, cell};
    if (foe) {
        // The other way from where it thinks out is, or at spikes.
        t.lie = true;
        t.sure = WAY_LIE_SURE;
        t.way = hunch_ + TAU / 2.0f;
        int best = 0;
        for (const auto& [plate, kind] : memory.traps)
            if (kind == TrapKind::Spikes && !o.memory.traps.count(plate) && plate != o.cell &&
                (best == 0 || cheb(plate, o.cell) < best)) {
                best = cheb(plate, o.cell);
                t.cell = plate;
                t.way = std::atan2(float(plate.y - o.cell.y), float(plate.x - o.cell.x));
            }
    } else if (settled_ || gloom() >= WAY_GLOOMY) {
        t.gloomy = true;
        t.sure = std::clamp(float(chunksIn_.size()) / WAY_SEEN_ENOUGH, hunchSure_, 1.0f);
    } else {
        t.way = hunch_;
        t.sure = hunchSure_;
    }
    if (auto it = wayTold_.find(o.id); it != wayTold_.end()) {
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
    if (traits.greedy < CUNNING_GREEDY || traits.brave < CUNNING_BRAVE || !isFoe(foe.id) || foe.keys == 0)
        return std::nullopt;
    for (const auto& [plate, kind] : memory.traps) {
        if (kind != TrapKind::Spikes || foe.memory.traps.count(plate)) continue;
        auto there = search(world, [&](Vec2i c) { return c != plate && cheb(c, plate) == 1; });
        if (!there) continue;
        auto on = searchFrom(world, plate, [&](Vec2i c) {
            auto d = lockedDoorBeside(c);
            return d && foe.memory.tile(*d) == Tile::Void;
        }, false);
        if (!on || on->empty()) continue;
        Tell t{Tell::Kind::Route, *lockedDoorBeside(on->back())};
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
    switch (t.kind) {
    case Tell::Kind::Cake:
        news = o.memory.cakes.insert(t.cell).second;
        if (news) o.memory.toldBy[t.cell] = id;
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
        // The cells along it and all round them, so they can cut the same
        // corners; only what they have never seen, and only tiles. A way to
        // water ends beside it, so the water comes with it.
        // Every cell they never saw is marked as told first: seeing round
        // one cell would otherwise hide the next (a plate on a lie) as seen.
        for (Vec2i c : t.route)
            if (o.memory.tile(c) == Tile::Void) o.memory.toldBy[c] = id;
        for (Vec2i c : t.route) {
            for (Vec2i d : DIRS8)
                for (Vec2i m : {c, c + d})
                    if (Tile tile = memory.tile(m); tile != Tile::Void && o.memory.tile(m) == Tile::Void) {
                        o.memory.see(m, tile);
                        news = true;
                    }
        }
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
            o.regard(w.about, delta);
            news = true;
        }
        break;
    case Tell::Kind::Way: {
        // Believed as far as the teller is trusted, and as sure as it sounds.
        float w = std::clamp(o.trust(id) + WAY_STRANGER, 0.0f, 1.0f);
        news = true;
        wayTold_[o.id] = t.gloomy ? -2.0f * TAU : t.way;
        if (t.gloomy) {
            o.note("@ SAYS THERE IS NO WAY OUT", id);
            note("TOLD @ THERE IS NO WAY OUT", o.id);
            o.lift(-WAY_GLOOM * w * t.sure);
            break;
        }
        float pull = w * t.sure / (t.sure + o.hunchSure_);
        float agree = std::cos(t.way - o.hunch_);
        float x = std::cos(o.hunch_) * (1.0f - pull) + std::cos(t.way) * pull;
        float y = std::sin(o.hunch_) * (1.0f - pull) + std::sin(t.way) * pull;
        if (x != 0.0f || y != 0.0f) o.hunch_ = std::atan2(y, x);
        o.hunchSure_ = std::clamp(o.hunchSure_ + WAY_SURE * w * agree, WAY_SURE_MIN, 1.0f);
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
                                       "ABOUT OTHERS", "THE WAY OUT", "THE WAY TO WATER"};
    std::string what = WHAT[size_t(t.kind)];
    if (t.kind != Tell::Kind::People) o.note("@ TOLD ME " + what, id);
    note(t.lie ? (t.kind == Tell::Kind::Water ? "TOLD @ A WAY TO WATER OVER SPIKES" : "TOLD @ A WAY OVER SPIKES")
               : "TOLD @ " + what,
         o.id);
    // A tip sold is owed for as the greeting ends; said once it is over, it went for nothing.
    if (t.sold && o.state == State::Greeting && o.partner_ == id) o.owes_ = id;
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
    case State::Greeting: return partner_;
    case State::Punching: return foe_;
    case State::Giving: return sharee_;
    case State::Looting: return robbee_ >= 0 ? robbee_ : corpse_;
    case State::Feeding: return meal_;
    default: break;
    }
    switch (goal) {
    case Goal::Meet: return partner_;
    case Goal::Follow: return leader_;
    case Goal::Company: return companion_;
    case Goal::Fight: return foe_;
    case Goal::Flee: return feared_;
    case Goal::Rob: return robbee_;
    case Goal::Watch: return watchee_;
    case Goal::Share: return sharee_;
    case Goal::Escort: return escortee_;
    case Goal::Escorted: return escort_;
    case Goal::Loot: return corpse_;
    case Goal::Feed: return meal_;
    default: return -1;
    }
}

void Human::greet(Human& other) {
    state = State::Greeting;
    animTime = 0.0f;
    gossiping = false;
    moving = false;
    path_.clear();
    partner_ = other.id;
    bool first = met.insert(other.id).second;
    note(first ? "MET @" : "CAUGHT UP WITH @", other.id);
    sayLater(first ? talk::Moment::Meet : talk::Moment::CatchUp, other.id);
    greetedAt_[other.id] = clock_;
    regard(other.id, TRUST_GREETED);
    if (other.cell.x != cell.x) facingLeft = other.cell.x < cell.x;
    timer = GREET_TIME;
}

// Without a key, the human now follows a new friend who has one.
void Human::endGreet(World& world) {
    if (keys == 0 && world.human(partner_).keys > 0 && !isFoe(partner_) && leader_ != partner_) {
        leader_ = partner_;
        note("FOLLOWING @, WHO HAS A KEY", leader_);
        sayLater(talk::Moment::Follow, leader_);
    }
    partner_ = -1;
    // A tip bought is paid for now, if they are still there to pay.
    int seller = owes_;
    owes_ = -1;
    if (seller >= 0) {
        Human& s = world.human(seller);
        if (!s.dead() && cakes > 0 && cheb(cell, s.cell) <= GREET_RADIUS) return give(s, Gift::Pay);
    }
    startIdle(0.0f, 0.0f);
}

bool Human::startFollow(World& world) {
    Human& l = world.human(leader_);
    bool seen = sight.sees(l.cell);
    // Dead out of sight, they are followed on until found or lost.
    if ((seen && (l.dead() || l.keys == 0)) || suspicious(l.id) || isFoe(l.id)) {
        if (suspicious(l.id) || isFoe(l.id))
            sayLater(talk::Moment::PartTurned, l.id);
        else if (!l.dead())
            sayLater(talk::Moment::PartNoKeys, l.id);
        leader_ = -1;
        return false;
    }
    Vec2i at = seen ? l.cell : lastSeen_[l.id];
    if (cheb(cell, at) <= FOLLOW_GAP) {
        if (!seen) {
            // Lost them.
            note("LOST @", leader_);
            sayLater(talk::Moment::PartLost, -1, leader_);
            leader_ = -1;
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
        leader_ = -1;
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
    if (auto it = snubbed_.find(o.id); it != snubbed_.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    if (isFoe(o.id)) return lieWorthy(o);
    if (!met.count(o.id)) return true;
    auto it = greetedAt_.find(o.id);
    if (it != greetedAt_.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    // Wanting a word with them: goes over.
    if (wantsWord(o.id)) return true;
    bool pay;
    return newsFor(o, pay).has_value() || o.newsFor(*this, pay).has_value() || gossipFor(o).has_value() ||
           o.gossipFor(*this).has_value();
}

// A foe with keys may be sent over spikes to a door; any foe not yet
// pointed the wrong way may be pointed at spikes as the way out.
bool Human::lieWorthy(const Human& o) const {
    if (traits.greedy < CUNNING_GREEDY || traits.brave < CUNNING_BRAVE || !isFoe(o.id)) return false;
    if (o.keys == 0 && wayTold_.count(o.id)) return false;
    auto it = greetedAt_.find(o.id);
    if (it != greetedAt_.end() && clock_ - it->second < CATCH_UP_TIME) return false;
    for (const auto& [plate, kind] : memory.traps)
        if (kind == TrapKind::Spikes && !o.memory.traps.count(plate)) return true;
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
    return !isFoe(o.id) && !suspicious(o.id) && !afraidOf(o.id) && !repelledBy(o);
}

// Someone last seen carrying cakes is gone after when nearer than any
// cake lying about. Cakes near someone this human ran from are left alone
// while it is still afraid.
bool Human::seekFood(World& world) {
    auto it = lastSeen_.find(feared_);
    bool afraid = fear_ > 0.0f && it != lastSeen_.end();
    auto p = search(world, [&](Vec2i c) {
        auto k = inReach(c, memory.cakes);
        return k && !(afraid && cheb(*k, it->second) < FLEE_DIST);
    });
    // A body with cakes on it does as well as cakes lying about.
    auto q = lootPath(world, true);
    size_t nearest = std::min(p ? p->size() : SIZE_MAX, q ? q->size() : SIZE_MAX);
    const Human* h = nearestHolder(world);
    if (h && (nearest == SIZE_MAX || cheb(cell, lastSeen_[h->id]) < int(nearest))) {
        // Not angry at them, only after their cakes.
        foe_ = h->id;
        grudge_ = 0.0f;
        if (startFight(world)) return true;
    }
    if (q && (!p || q->size() < p->size())) return startLoot(world, true);
    if (!p) return false;
    startWalking(Goal::FetchCake, *p);
    return true;
}

void Human::startEating() {
    state = State::Eating;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    timer = EAT_TIME;
}

// Snatched away mid-bite, nothing is eaten.
void Human::endEating() {
    // Eating with more cakes still in hand bears out the lesson to keep them.
    if (cakes > 1 && lessons.hoard > 0.0f) learn(lessons.hoard, false);
    if (cakes > 0) {
        cakes--;
        hunger = std::max(0.0f, hunger - CAKE_FOOD);
        takeIn(0.0f, 1.0f);
        note("ATE A CAKE");
    }
    startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
}

const Human* Human::nearestHolder(const World& world) const {
    const Human* best = nullptr;
    int bestDist = 0;
    for (auto [id, n] : seenCakes_) {
        if (fear_ > 0.0f && id == feared_) continue;
        // A suspect's cakes only tempt the starving, and so does a fight
        // at all, to the timid.
        if ((suspicious(id) || traits.brave < TIMID) && !starving()) continue;
        // A foe's cakes look nearer, a friend's farther.
        int d = cheb(cell, lastSeen_.at(id)) + int(trust(id) * TRUST_STEPS);
        // Asleep, they can't fight back.
        if (const Human& o = world.human(id); o.state == State::Sleeping && sight.sees(o.cell)) d -= SLEEPER_NEARER;
        if (!best || d < bestDist) best = &world.human(id), bestDist = d;
    }
    return best;
}

// Moved by their plea, it gives as to a friend, to the merely hungry too,
// and to the weak as well.
bool Human::wouldShare(const Human& other) const {
    bool moved = movedBy(other.id);
    return cakes > (isFriend(other.id) || moved ? 1 : spareFrom()) && !starving() &&
           (other.starving() || (moved && other.hungry())) && other.cakes == 0 &&
           (other.health >= preyHealth() || moved) && !suspicious(other.id) && !isFoe(other.id);
}

bool Human::helpStarving(World& world) {
    if (cakes <= 1 || starving()) return false;
    const Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->cakes > 0 || !sight.sees(o->cell)) continue;
        if (!o->starving() && !(movedBy(o->id) && o->hungry())) continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    if (!best) return false;
    bool moved = movedBy(best->id);
    // A foe gets beaten up however well they look; anyone else only when weak.
    if (!moved && (best->health < preyHealth() || isFoe(best->id))) {
        foe_ = best->id;
        grudge_ = grudgeFor(best->id);
        return startFight(world);
    }
    if (cakes <= spareFrom() && !isFriend(best->id) && !moved) return false;
    sharee_ = best->id;
    return startShare(world);
}

bool Human::startShare(World& world) {
    Human& o = world.human(sharee_);
    if (!wouldShare(o) || !sight.sees(o.cell)) {
        sharee_ = -1;
        return false;
    }
    if (cheb(cell, o.cell) <= 1) {
        give(o, Gift::Share);
        // Those who see it think better of the giver.
        witness(world, *this, o, TRUST_SAW_FEED, 0.0f, Episode::What::Gift, EPISODE_SAW_GIFT, int(Gift::Share));
        return true;
    }
    auto p = search(world, [&](Vec2i c) { return c != o.cell && cheb(c, o.cell) <= 1; });
    if (!p) {
        sharee_ = -1;
        return false;
    }
    startWalking(Goal::Share, *p);
    return true;
}

// The cake changes hands at once; the other eats it when next it thinks.
// Handed over under threat, it ends their fight for it, and earns no thanks.
void Human::give(Human& other, Gift why) {
    state = State::Giving;
    moving = false;
    path_.clear();
    if (other.cell.x != cell.x) facingLeft = other.cell.x < cell.x;
    timer = GIVE_TIME;
    cakes--;
    other.cakes++;
    other.rethink_ = true;
    sharee_ = -1;
    switch (why) {
    case Gift::Share:
        // Fed by the one it was after, it lets them be.
        if (other.foe_ == id) other.foe_ = -1;
        note("GAVE @ A CAKE", other.id);
        other.note("@ GAVE ME A CAKE", id);
        remember(Episode::What::Gift, EPISODE_GAVE, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_GIVEN, id, other.id, int(why));
        other.regard(id, TRUST_FED);
        // One plea, one cake.
        moved_.erase(other.id);
        sayLater(talk::Moment::Give, other.id);
        other.sayLater(talk::Moment::Thanks, id);
        break;
    case Gift::Pay:
        note("PAID @ A CAKE FOR NEWS", other.id);
        other.note("@ PAID ME A CAKE FOR NEWS", id);
        remember(Episode::What::Gift, EPISODE_PAID, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_PAID, id, other.id, int(why));
        other.regard(id, TRUST_PAID);
        sayLater(talk::Moment::Pay, other.id);
        break;
    case Gift::Yield:
        note("GAVE @ A CAKE UNDER THREAT", other.id);
        other.note("@ GAVE ME A CAKE UNDER THREAT", id);
        remember(Episode::What::Gift, EPISODE_YIELDED, id, other.id, int(why));
        other.remember(Episode::What::Gift, EPISODE_YIELDED, id, other.id, int(why));
        if (other.foe_ == id) other.foe_ = -1;
        break;
    }
}


// The fight goes on while the foe has hit this human lately, or while it
// is hungry and knows they carry cakes. It is off once they are seen dead
// or running, and a foe only remembered is looked for where they were last seen.
bool Human::startFight(World& world) {
    Human& f = world.human(foe_);
    bool seen = sight.sees(f.cell);
    bool running = seen && f.state == State::Walking && f.goal == Goal::Flee;
    bool forCakes = hungry() && seenCakes_.count(foe_) > 0;
    // Hunted, they are chased however they run, as long as they are in
    // sight or were lately.
    bool hunt = hunting() && lastSeen_.count(foe_);
    if ((seen && f.dead()) || (!hunt && (running || (grudge_ <= 0.0f && !forCakes) || (!seen && !forCakes)))) {
        foe_ = -1;
        return false;
    }
    Vec2i at = seen ? f.cell : lastSeen_[foe_];
    // Only after their cakes: a word first may be enough.
    if (!hunt && seen && grudge_ <= 0.0f && cheb(cell, at) <= THREAT_RANGE && threaten(f)) return true;
    if (cheb(cell, at) <= 1) {
        if (seen) {
            startPunch(f);
            return true;
        }
        // Got where they were; they're gone.
        seenCakes_.erase(foe_);
        if (hunting()) prey_ = -1;
        foe_ = -1;
        return false;
    }
    auto p = search(world, [&](Vec2i c) { return c != at && cheb(c, at) <= 1; });
    if (!p) {
        foe_ = -1;
        return false;
    }
    startWalking(Goal::Fight, *p);
    return true;
}

void Human::startPunch(Human& foe) {
    state = State::Punching;
    moving = false;
    path_.clear();
    punchLanded = false;
    if (foe.cell.x != cell.x) facingLeft = foe.cell.x < cell.x;
    timer = PUNCH_WINDUP;
}

// Lands only if the foe is still within reach, and snatches one of their
// cakes when there is room for it.
void Human::landPunch(World& world) {
    punchLanded = true;
    timer = PUNCH_FOLLOW;
    if (foe_ < 0) return;
    Human& f = world.human(foe_);
    // Aimed where they are seen, which is still the cell left until halfway through a step.
    Vec2i at = f.stepProgress() < 0.5f ? f.prev : f.cell;
    if (at.x != cell.x) facingLeft = at.x < cell.x;
    if (f.dead() || cheb(cell, at) > PUNCH_REACH) return;
    bool took = f.cakes > 0 && cakes < carryMax();
    if (took) {
        f.cakes--;
        cakes++;
    }
    note(took ? "HIT @ AND TOOK A CAKE" : "HIT @", f.id);
    f.note(took ? "@ HIT ME AND TOOK A CAKE" : "@ HIT ME", id);
    remember(Episode::What::Fight, EPISODE_HIT, id, f.id);
    f.remember(Episode::What::Fight, took ? EPISODE_ROBBED : EPISODE_GOT_HIT, id, f.id);
    sayLater(talk::Moment::Hit, f.id);
    // Wet blood on them comes off on the fist.
    if (f.stains.wetAny()) stain({BodyPart::FrontArm}, 1, 120, true);
    bool asleep = f.state == State::Sleeping;
    if (asleep) note("HIT @ IN THEIR SLEEP", f.id);
    f.takeHit(world, *this, rng_.range(PUNCH_DAMAGE_MIN, PUNCH_DAMAGE_MAX));
    // Set on asleep, it never stood a chance: seen, that is as bad as a killing.
    if (asleep)
        witness(world, *this, f, TRUST_SAW_SLEEPER_HIT, DREAD_SAW_SLEEPER_HIT, Episode::What::Fight, EPISODE_SAW_KILL);
    else
        witness(world, *this, f, TRUST_SAW_HIT, DREAD_SAW_HIT, Episode::What::Fight, EPISODE_SAW_FIGHT);
    observe(world);
}

void Human::creditKill(World& world, Human& by, bool witnesses) {
    killer_ = by.id;
    if (witnesses)
        for (const auto& h : world.humans())
            if (h.get() != this && h.get() != &by && !h->dead() && h->sight.sees(cell))
                h->rememberAt(cell, Episode::What::Kill, EPISODE_SAW_KILL, by.id, id);
    // Bled out out of its sight, it only learns of it at the body.
    killerSaw_ = witnesses || (!by.dead() && by.sight.sees(cell));
    if (!killerSaw_) return;
    by.note(by.hunting() ? "KILLED @ TO EAT" : "KILLED @", id);
    by.remember(Episode::What::Kill, EPISODE_KILLED, by.id, id);
    by.sayLater(talk::Moment::Killed, -1, id);
}

void Human::takeHit(World& world, Human& from, float damage) {
    if (dead()) return;
    bool asleep = state == State::Sleeping;
    health -= damage;
    hurtTime = HURT_TIME;
    regard(from.id, TRUST_HIT);
    scare(DREAD_HIT);
    if (health <= 0.0f) {
        note("KILLED BY @", from.id);
        creditKill(world, from, true);
        return die(world);
    }
    // Hit in its sleep: wakes with a start, slow to answer at first.
    if (asleep) {
        wakeUp(world, true, from.id, "WOKE UP TO @ HITTING ME");
        scare(DREAD_WOKEN);
        lift(HOPE_WOKEN_HIT);
        stunned_ = WOKEN_STUN;
        daze_ = std::max(daze_, WOKEN_DAZE);
        blameWatcher(world);
    }
    sayLater(talk::Moment::Ouch, from.id);
    // The blow that tips a beating over opens a wound as deep as the whole
    // of it; each after only deepens it.
    bool open = beating_ > BEATING_BLEED;
    beating_ += damage;
    if (beating_ > BEATING_BLEED) {
        wound(WoundKind::Blunt, BodyPart::Head, open ? damage : beating_, true, from.id);
        sprays_.push_back({float(cell.x - from.cell.x), float(cell.y - from.cell.y), damage, BodyPart::Head});
    }
    // Already trading blows with them: keep at it.
    if (state == State::Punching && foe_ == from.id && !weak(*this)) {
        grudge_ = grudgeFor(from.id);
        return;
    }
    provoked_ = true;
    attacker_ = from.id;
}

// Flying chest high, a dart takes the chest or, from the side, the arm on
// the side it came from.
void Human::hitByDart(float damage, Vec2i dir) {
    if (dead()) return;
    if (health > DART_SURVIVE_HEALTH) health = std::max(DART_SURVIVE_HEALTH, health - damage);
    hurtTime = HURT_TIME;
    note("A DART HIT ME");
    remember(Episode::What::Trap, EPISODE_DART, id, -1, int(TrapKind::Darts));
    scare(DREAD_TRAP_HIT);
    sayLater(talk::Moment::Darted);
    BodyPart part = BodyPart::Torso;
    if (dir.x != 0 && rng_.chance(0.5f)) part = (dir.x > 0) == facingLeft ? BodyPart::FrontArm : BodyPart::BackArm;
    wound(WoundKind::Pierce, part, damage, false);
    sprays_.push_back({float(dir.x), float(dir.y), damage, part});
    rethink_ = true;
}

void Human::impale(World&) {
    if (dead()) return;
    health = std::min(health, SPIKE_LEFT);
    hurtTime = HURT_TIME;
    note("STEPPED ON SPIKES");
    scare(DREAD_TRAP_HIT);
    sayLater(talk::Moment::Spiked);
    BodyPart part = rng_.chance(0.5f) ? BodyPart::Feet : BodyPart::Legs;
    float damage = rng_.range(SPIKE_WOUND_MIN, SPIKE_WOUND_MAX);
    wound(WoundKind::Pierce, part, damage).bleed *= SPIKE_BLEED_MUL;
    sprays_.push_back({0.0f, 0.0f, damage, part});
    rethink_ = true;
}

void Human::learnTrap(Vec2i cell, TrapKind kind, bool mine) {
    bool fresh = memory.traps.emplace(cell, kind).second;
    if (fresh) rethink_ = true;
    rememberAt(cell, Episode::What::Trap,
               !mine ? EPISODE_SAW_TRAP : kind == TrapKind::Spikes ? EPISODE_SPIKES : EPISODE_OWN_TRAP, mine ? id : -1,
               -1, int(kind));
    // Spikes underfoot are in the log as they go in.
    if (!mine)
        note(std::string("SAW ") + trapWord(kind) + " GO OFF");
    else if (kind == TrapKind::Darts)
        note("SET OFF DARTS");
    else if (kind == TrapKind::Slam) {
        note("SHUT IN A SLAM ROOM");
        sayLater(talk::Moment::Slammed);
    }
    // Seen going off, told of or not: a scare.
    scare(DREAD_TRAP_SEEN);
    // A trap it was told of, seen: true. A trap where it was told the way
    // was clear: whoever told it lied.
    if (auto it = memory.toldBy.find(cell); it != memory.toldBy.end()) {
        if (fresh) {
            note("@ LIED ABOUT THE WAY", it->second);
            remember(Episode::What::Lied, EPISODE_LIED, it->second, id);
            regard(it->second, TRUST_LIED);
        }
        memory.toldBy.erase(it);
    }
}

// Hit by a starving human it would feed anyway, it feeds them. Otherwise,
// not hungry or too hurt, it runs; hungry, or with nowhere to run, it
// fights back.
void Human::react(World& world) {
    provoked_ = false;
    awaiting_ = false;
    moving = false;
    observe(world);
    Human& a = world.human(attacker_);
    if (!a.dead() && wouldShare(a)) {
        if (foe_ == a.id) foe_ = -1;
        sharee_ = a.id;
        if (startShare(world)) return;
    }
    if (!a.dead()) {
        // The brave stand their ground, unless too hurt to.
        if (weak(*this) || (!hungry() && !rng_.chance(traits.brave))) {
            feared_ = a.id;
            fear_ = fearTime();
            scare(DREAD_HIT);
            if (foe_ == a.id) foe_ = -1;
            prey_ = -1;
            if (startFlee(world)) {
                note("RAN FROM @", a.id);
                sayLater(talk::Moment::Flee, a.id);
                return;
            }
        }
        foe_ = a.id;
        grudge_ = grudgeFor(a.id);
    }
    choose(world);
}

bool Human::startFlee(World& world) {
    Vec2i from = world.human(feared_).cell;
    auto p = search(world, [&](Vec2i c) { return cheb(c, from) >= FLEE_DIST; }, true);
    if (!p || p->empty()) return false;
    startWalking(Goal::Flee, *p);
    return true;
}

// Fades as fear_ runs out.
float Human::fright() const {
    return std::max(dread_, feared_ >= 0 ? std::clamp(fear_ / fearTime(), 0.0f, 1.0f) : 0.0f);
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
    if (out > p) outUrged_ = true, p = out;
    else outUrged_ = false;
    if (p <= 0.0f) return false;
    urgeWait_ = DOOR_URGE_EVERY;
    return urged_ = rng_.chance(p);
}

// The body stays where it fell, out of the way, with all it carried on it.
void Human::die(World& world) {
    for (Wound& w : wounds) w.healLeft = std::min(w.healLeft, CORPSE_BLEED_TIME);
    diedAsleep_ = state == State::Sleeping;
    state = State::Dead;
    moving = false;
    provoked_ = false;
    foe_ = -1;
    prey_ = -1;
    meal_ = -1;
    flesh = FLESH_MEALS;
    diedAt_ = clock_;
    path_.clear();
    animTime = 0.0f;
    speech_.clear();
    sayings_.clear();
    letGo(world);
    world.vacate(cell);
}

bool Human::worthLooting(int id, bool cakesOnly) const {
    auto it = bodies_.find(id);
    if (it == bodies_.end()) return false;
    bool forCakes = it->second.cakes > 0 && cakes < carryMax();
    return forCakes || (!cakesOnly && !it->second.searched);
}

// Bodies near someone this human ran from are left alone while it is
// still afraid, as cakes are.
std::optional<std::vector<Vec2i>> Human::lootPath(const World& world, bool cakesOnly, int* whose) const {
    auto it = lastSeen_.find(feared_);
    bool afraid = fear_ > 0.0f && it != lastSeen_.end();
    std::vector<std::pair<int, Vec2i>> worth;
    for (const auto& [who, b] : bodies_)
        if (worthLooting(who, cakesOnly) && !(afraid && cheb(b.cell, it->second) < FLEE_DIST)) worth.push_back({who, b.cell});
    if (worth.empty()) return std::nullopt;
    return search(world, [&](Vec2i c) {
        for (auto [who, at] : worth)
            if (cheb(c, at) <= 1) {
                if (whose) *whose = who;
                return true;
            }
        return false;
    });
}

bool Human::startLoot(World& world, bool cakesOnly) {
    robbee_ = -1;
    int whose = -1;
    auto p = lootPath(world, cakesOnly, &whose);
    if (!p) {
        corpse_ = -1;
        return false;
    }
    corpse_ = whose;
    if (!p->empty()) {
        startWalking(Goal::Loot, *p);
        return true;
    }
    // Beside it: kneels and goes through it.
    const Human& b = world.human(corpse_);
    state = State::Looting;
    moving = false;
    path_.clear();
    animTime = 0.0f;
    if (b.cell.x != cell.x) facingLeft = b.cell.x < cell.x;
    timer = LOOT_TIME;
    return true;
}

// Everything it holds of keys, and the cakes there is room for.
void Human::loot(World& world) {
    if (robbee_ >= 0) return rob(world);
    Human& b = world.human(corpse_);
    corpse_ = -1;
    int k = b.keys, c = std::clamp(carryMax() - cakes, 0, b.cakes);
    b.keys = 0;
    b.cakes -= c;
    keys += k;
    cakes += c;
    if (k > 0 && leader_ >= 0) {
        sayLater(talk::Moment::PartOwnKey, leader_);
        leader_ = -1;
    }
    auto count = [](int n, const char* one, const char* many) {
        return n == 1 ? std::string(one) : std::to_string(n) + many;
    };
    std::string took = k > 0 && c > 0 ? count(k, "A KEY", " KEYS") + " AND " + count(c, "A CAKE", " CAKES")
                       : k > 0        ? count(k, "A KEY", " KEYS")
                                      : count(c, "A CAKE", " CAKES");
    note(k + c > 0 ? "TOOK " + took + " OFF @'S BODY" : "WENT THROUGH @'S BODY, FOUND NOTHING", b.id);
    remember(Episode::What::Loot, EPISODE_LOOTED, id, b.id);
    // Gone through in plain sight: picked over, and a poor thing to do.
    for (const auto& o : world.humans())
        if (!o->dead() && (o.get() == this || o->sight.sees(b.cell))) o->bodies_[b.id] = {b.cell, b.cakes, true, b.meals(), b.spoiled()};
    witness(world, *this, b, TRUST_SAW_LOOT, 0.0f, Episode::What::Loot, EPISODE_SAW_LOOT);
    // Said aloud, and the nearest who saw it may say what they think of it.
    sayLater(talk::Moment::Loot, -1, b.id);
    Human* critic = nullptr;
    for (const auto& o : world.humans())
        if (o.get() != this && !o->dead() && o->sight.sees(cell) && cheb(o->cell, cell) <= HEAR_RADIUS &&
            !o->isFoe(b.id) && (!critic || cheb(o->cell, cell) < cheb(critic->cell, cell)))
            critic = o.get();
    if (critic) critic->sayLater(talk::Moment::Shame, id, b.id);
    startIdle(LOOK_TIME_MIN, LOOK_TIME_MAX);
    if (c > 0 && hungry()) timer = 0.0f;
}

// A plate under a table goes off as the cake is lifted; one underfoot
// already went off as it was stepped on.
void Human::takeInReach(World& world) {
    for (Vec2i d : REACH) {
        Vec2i c = cell + d;
        bool underfoot = d == Vec2i{};
        if (!underfoot && (world.tile(c) != Tile::Table || avoids(c))) continue;
        if (!world.hasKey(c)) memory.keys.erase(c);
        if (!world.hasCake(c)) memory.cakes.erase(c);
        bool key = world.hasKey(c), cake = cakes < carryMax() && world.hasCake(c);
        if (!key && !cake) continue;
        if (!underfoot) world.setOff(c, *this);
        if (key && world.takeKey(c)) {
            keys++;
            note("PICKED UP A KEY");
            memory.keys.erase(c);
            if (leader_ >= 0) sayLater(talk::Moment::PartOwnKey, leader_);
            leader_ = -1;
        }
        if (cake && world.takeCake(c)) {
            cakes++;
            note("PICKED UP A CAKE");
            memory.cakes.erase(c);
            if (hungry()) rethink_ = true;
        }
    }
}

std::optional<Vec2i> Human::inReach(Vec2i at, const std::unordered_set<Vec2i, Vec2iHash>& items) const {
    for (Vec2i d : REACH) {
        Vec2i c = at + d;
        if (items.count(c) && (d == Vec2i{} || (memory.tile(c) == Tile::Table && !avoids(c)))) return c;
    }
    return std::nullopt;
}

void Human::checkDoorsNear() {
    for (int y = -DOOR_CHECK_RANGE; y <= DOOR_CHECK_RANGE; y++)
        for (int x = -DOOR_CHECK_RANGE; x <= DOOR_CHECK_RANGE; x++) {
            Vec2i c = cell + Vec2i{x, y};
            if (memory.tile(c) != Tile::DoorLocked) continue;
            // The whole door, however wide, along its wall.
            for (Vec2i d : DIRS4)
                for (Vec2i e = c; memory.tile(e) == Tile::DoorLocked; e = e + d) doorChecked_[e] = clock_;
        }
}

bool Human::doorDue(Vec2i c) const {
    auto door = lockedDoorBeside(c);
    if (!door) return false;
    auto it = doorChecked_.find(*door);
    return it == doorChecked_.end() || clock_ - it->second >= DOOR_RECHECK_TIME;
}

bool Human::knownPassable(Vec2i c) const { return passable(memory.tile(c)); }

// The starving would rather be hurt than go without the cake on a dart plate.
bool Human::avoids(Vec2i c) const {
    auto it = memory.traps.find(c);
    return it != memory.traps.end() && !(it->second == TrapKind::Darts && starving());
}

bool Human::isFrontier(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return false;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::Void) return true;
    return false;
}

int Human::hunchCost(Vec2i c, int most) const {
    Vec2i d = c - cell;
    if (d == Vec2i{0, 0}) return 0;
    float along = (float(d.x) * std::cos(hunch_) + float(d.y) * std::sin(hunch_)) / std::hypot(float(d.x), float(d.y));
    return int(float(most) * out_ * (1.0f - along) / 2.0f);
}

std::optional<Vec2i> Human::lockedDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::DoorLocked) return c + d;
    return std::nullopt;
}

std::optional<Vec2i> Human::openDoorBeside(Vec2i c) const {
    if (memory.tile(c) != Tile::Floor) return std::nullopt;
    for (Vec2i d : DIRS4)
        if (memory.tile(c + d) == Tile::DoorOpen && memory.tile(c + d * 2) == Tile::Void) return c + d;
    return std::nullopt;
}

// Dijkstra over remembered passable cells, keeping off known traps, 8
// ways, cutting a corner only when both cells beside it are open. Ties break at random so equal
// choices don't always go the same way.
std::optional<std::vector<Vec2i>> Human::search(const World& world, const std::function<bool(Vec2i)>& done,
                                                bool avoidCrowd, const std::function<int(Vec2i)>& goalCost) const {
    return searchFrom(world, cell, done, avoidCrowd, goalCost);
}

// A done cell with a goal cost goes back in the queue as an ending, at
// what it costs to get there and end there; the first ending out wins.
std::optional<std::vector<Vec2i>> Human::searchFrom(const World& world, Vec2i start,
                                                    const std::function<bool(Vec2i)>& done, bool avoidCrowd,
                                                    const std::function<int(Vec2i)>& goalCost) const {
    struct Node {
        int cost;
        uint32_t tie;
        Vec2i at;
        bool ending = false;
        bool operator>(const Node& o) const { return cost != o.cost ? cost > o.cost : tie > o.tie; }
    };
    auto shunned = shunZone(world, start);
    auto open = [&](Vec2i c) {
        if (!knownPassable(c) || avoids(c) || (shunned && shunned(c))) return false;
        if (!avoidCrowd) return true;
        Human* o = world.occupant(c);
        return !o || o == this || !sight.sees(c);
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> frontier;
    std::unordered_map<Vec2i, int, Vec2iHash> cost;
    std::unordered_map<Vec2i, Vec2i, Vec2iHash> from;
    Rng tie(rng_.next());
    frontier.push({0, 0, start});
    cost[start] = 0;
    int expanded = 0;
    while (!frontier.empty() && expanded < SEARCH_LIMIT) {
        Node n = frontier.top();
        frontier.pop();
        int extra = 0;
        if (!n.ending) {
            if (n.cost > cost[n.at]) continue;
            if (done(n.at) && goalCost) extra = goalCost(n.at);
        }
        if (n.ending || (extra <= 0 && done(n.at))) {
            std::vector<Vec2i> path;
            for (Vec2i c = n.at; c != start; c = from[c]) path.push_back(c);
            return std::vector<Vec2i>(path.rbegin(), path.rend());
        }
        if (extra > 0) frontier.push({n.cost + extra, uint32_t(tie.next()), n.at, true});
        expanded++;
        for (Vec2i d : DIRS8) {
            Vec2i m = n.at + d;
            if (!open(m)) continue;
            bool diagonal = d.x != 0 && d.y != 0;
            if (diagonal && (!open(n.at + Vec2i{d.x, 0}) || !open(n.at + Vec2i{0, d.y}))) continue;
            int c = n.cost + (diagonal ? 14 : 10) + bloodCost(m) + messCost(world, m);
            auto it = cost.find(m);
            if (it != cost.end() && it->second <= c) continue;
            cost[m] = c;
            from[m] = n.at;
            frontier.push({c, uint32_t(tie.next()), m});
        }
    }
    return std::nullopt;
}

bool Human::sayLater(talk::Moment m, int to, int about) { return sayLater(Saying{m, to, about}); }

bool Human::sayLater(Saying s) {
    // Blows and cries only ever fill a silence, but for a trap's cry, which
    // cuts in ahead of whatever was still to be said; word to pass on
    // always waits its turn.
    if (s.moment == talk::Moment::Darted || s.moment == talk::Moment::Spiked ||
        s.moment == talk::Moment::Slammed) {
        sayings_.insert(sayings_.begin(), std::move(s));
        return true;
    }
    bool cry = talk::isCry(s.moment);
    bool word = s.tell || s.intro;
    if (!(cry ? sayings_.empty() : word || sayings_.size() <= SPEECH_QUEUE)) return false;
    sayings_.push_back(std::move(s));
    return true;
}

bool Human::hasWordFor(int to) const {
    return std::any_of(sayings_.begin(), sayings_.end(),
                       [&](const Saying& s) { return s.to == to && (s.tell || s.intro); });
}

void Human::awaitLine() {
    sayings_.back().awaited = true;
    awaiting_ = true;
    answerWait_ = timer;
}

void Human::speak(World& world, float dt) {
    using talk::Moment;
    if (!speech_.empty() && (speech_.front().shown += dt) >= speech_.front().time) {
        speech_.pop_front();
        quietSince_ = world.time();
    }
    while (!sayings_.empty()) {
        Saying& s = sayings_.front();
        // Blows and cries only fill a silence, and go unsaid if they can't
        // at once; a line to someone gone out of earshot goes unsaid.
        bool cry = talk::isCry(s.moment);
        const Human* to = s.to >= 0 ? &world.human(s.to) : nullptr;
        bool gone = to && (to->dead() || cheb(cell, to->cell) > HEAR_RADIUS || !sight.sees(to->cell));
        float patience = cry ? CRY_PATIENCE : s.tell || s.intro ? WORD_PATIENCE : SPEECH_PATIENCE;
        if (gone || s.waited > patience) {
            if (s.awaited) awaiting_ = false;
            sayings_.erase(sayings_.begin());
            continue;
        }
        if (!speech_.empty()) {
            if (cry) s.waited += dt;
            return;
        }
        if (!cry) {
            float now = world.time();
            if (now - quietSince_ < SPEECH_PAUSE) return;
            // Its turn: not over the one it talks to, nor over anyone talking
            // to it; and having just spoken, it lets them answer first.
            for (const auto& h : world.humans()) {
                if (h.get() == this || h->dead()) continue;
                const Speech* sp = h->speaking();
                bool party = h->id == s.to || (sp && sp->to == id);
                bool answer = h->id == s.to && !h->sayings_.empty() && h->sayings_.front().to == id &&
                              quietSince_ > h->quietSince_;
                if (answer || (party && (sp || now - h->quietSince_ < SPEECH_PAUSE))) {
                    s.waited += dt;
                    return;
                }
            }
        }
        Saying due = std::move(s);
        sayings_.erase(sayings_.begin());
        return say(world, due);
    }
}

void Human::say(World& world, const Saying& s) {
    Human* to = s.to >= 0 ? &world.human(s.to) : nullptr;
    if (s.awaited) {
        awaiting_ = false;
        // Said: now the answer is waited for.
        if (state == State::Idle) timer = answerWait_;
    }
    if (to && to->dead()) return;
    const Human* about = s.about >= 0 ? &world.human(s.about) : nullptr;
    auto said = s.quiet ? std::nullopt
                        : talk::pick(s.moment, *this, to, about, rng_, s.topic ? &*s.topic : nullptr, &chatLines_,
                                     world.time());
    // A topic is raised once it is said; unsaid, it can come up again.
    if (said && to && s.topic) {
        if (s.topic->turn == 1)
            raised(*s.topic, to->id);
        else if (s.topic->turn == 2)
            answered(*s.topic, to->id);
        chatOn(*to, s);
    }
    if (said) {
        speech_.push_back(
            {said->text, said->tone, 0.0f, SPEECH_TIME + SPEECH_PER_LETTER * float(said->text.size()), s.to, std::nullopt});
        // Telling the way, it turns and points it out.
        if (s.tell && s.tell->kind == Tell::Kind::Way && !s.tell->gloomy) {
            speech_.back().points = s.tell->way;
            if (std::abs(std::cos(s.tell->way)) > 0.01f) facingLeft = std::cos(s.tell->way) < 0.0f;
        }
        std::string quote = '"' + said->text + '"';
        if (to)
            note("SAID " + quote + " TO @", to->id);
        else
            note("SAID " + quote);
        for (const auto& h : world.humans())
            if (h.get() != this && !h->dead() && cheb(h->cell, cell) <= HEAR_RADIUS && h->sight.sees(cell))
                h->hear(world, *this, *said, to, about);
    }
    // Turned away out loud, it may answer back.
    if (said && to && s.moment == talk::Moment::Refuse) to->sayLater(talk::Moment::Snubbed, id);
    // Joined out loud, the other answers.
    if (said && to && s.moment == talk::Moment::Join) to->sayLater(talk::Moment::JoinReply, id);
    // Asked the way out, the other answers.
    if (said && to && s.moment == talk::Moment::AskWay) to->answerWay(*this);
    // What it tells lands now, worded or not.
    if (to && s.intro) giveName(*to);
    if (to && s.tell) deliver(*to, *s.tell);
}

void Human::hear(World&, Human& s, const talk::Said& said, Human* to, const Human* about) {
    using talk::Tone;
    if (state == State::Sleeping) return;
    bool mine = to == this;
    std::string quote = '"' + said.text + '"';
    if (mine)
        note("@ SAID " + quote, s.id);
    else if (to)
        note("HEARD @ SAY " + quote + " TO #", s.id, to->id);
    else
        note("HEARD @ SAY " + quote, s.id);
    // A name said aloud is learnt by whoever can see whose it is.
    auto learn = [&](const Human& h) {
        if (&h == this || knownName(h.id) || !sight.sees(h.cell)) return;
        memory.names[h.id] = h.name;
        note("HEARD @'S NAME", h.id);
    };
    if (said.namesMe) learn(s);
    if (said.namesThem && to) learn(*to);
    if (said.namesWho && about) learn(*about);

    switch (said.tone) {
    case Tone::Plain:
        break;
    case Tone::Warm:
        if (mine) regard(s.id, TRUST_WARM);
        break;
    case Tone::Rude:
        if (mine) regard(s.id, TRUST_RUDE);
        break;
    case Tone::Fear:
        if (trust(s.id) >= 0.0f) scare(DREAD_TALK * (1.0f + trust(s.id)));
        break;
    case Tone::Plead:
        // Moved or not, it decides once per plea.
        if (mine && cakes > 0 && !movedBy(s.id) &&
            rng_.chance(PLEA_MOVES * sway(traits.sociable) * sway(1.0f - traits.greedy) +
                        (isFriend(s.id) ? PLEA_FRIEND : 0.0f))) {
            moved_[s.id] = clock_ + MOVED_TIME;
            rethink_ = true;
        }
        break;
    case Tone::Threat: {
        if (!mine) {
            // Overheard, it reflects on the threatener, unless the threatened is a foe.
            if (!to || !isFoe(to->id)) regard(s.id, TRUST_HEARD_THREAT);
            break;
        }
        // Not while chasing or running, nor to whoever it is after.
        bool rushing = state == State::Walking && (goal == Goal::Fight || goal == Goal::Flee);
        bool free = state == State::Idle || state == State::Walking || state == State::Sitting ||
                    state == State::Rising;
        if (free && !rushing && foe_ != s.id && !provoked_ && cakes > 0 && !starving() && cheb(cell, s.cell) <= THREAT_RANGE &&
            rng_.chance(YIELD_CHANCE * (1.0f - traits.brave) * (1.0f + fright()))) {
            sayLater(talk::Moment::Yield, s.id);
            give(s, Gift::Yield);
        } else {
            regard(s.id, TRUST_THREAT);
            sayLater(talk::Moment::Defy, s.id);
        }
        break;
    }
    }
}

bool Human::movedBy(int id) const {
    auto it = moved_.find(id);
    return it != moved_.end() && it->second > clock_;
}

// Only when it knows of no cake lying about, unless starving; never a foe,
// or whoever it is afraid of.
bool Human::plead(World& world) {
    if (!hungry() || cakes > 0 || (!memory.cakes.empty() && !starving())) return false;
    Human* best = nullptr;
    for (const auto& o : world.humans()) {
        if (o.get() == this || o->dead() || o->cakes == 0 || !sight.sees(o->cell) || cheb(cell, o->cell) > HEAR_RADIUS)
            continue;
        if (isFoe(o->id) || (fear_ > 0.0f && o->id == feared_)) continue;
        if (auto it = begged_.find(o->id); it != begged_.end() && clock_ - it->second < BEG_AGAIN) continue;
        if (!best || cheb(cell, o->cell) < cheb(cell, best->cell)) best = o.get();
    }
    if (!best) return false;
    begged_[best->id] = clock_;
    float p = BEG_CHANCE * sway(1.0f - traits.brave) * sway(traits.sociable) * sway(1.0f - traits.greedy) +
              (starving() ? BEG_STARVING : 0.0f);
    if (!rng_.chance(p)) return false;
    if (!sayLater(talk::Moment::Beg, best->id)) return false;
    startIdle(BEG_WAIT_MIN, BEG_WAIT_MAX);
    awaitLine();
    if (best->cell.x != cell.x) facingLeft = best->cell.x < cell.x;
    return true;
}

bool Human::threaten(Human& f) {
    if (auto it = threatened_.find(f.id); it != threatened_.end() && clock_ - it->second < THREAT_AGAIN) return false;
    threatened_[f.id] = clock_;
    if (!rng_.chance(THREAT_BASE + THREAT_BOLD * (traits.brave + traits.greedy) / 2.0f)) return false;
    if (!sayLater(talk::Moment::Threat, f.id)) return false;
    startIdle(THREAT_WAIT_MIN, THREAT_WAIT_MAX);
    awaitLine();
    if (f.cell.x != cell.x) facingLeft = f.cell.x < cell.x;
    return true;
}
