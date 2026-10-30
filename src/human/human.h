#pragma once
#include "core/rng.h"
#include "human/body_blood.h"
#include "human/bottle.h"
#include "human/episode.h"
#include "human/human_look.h"
#include "human/life_log.h"
#include "human/memory.h"
#include "human/name.h"
#include "human/sight.h"
#include "human/talk.h"
#include "human/topic.h"
#include "human/traits.h"
#include "human/wound.h"
#include <deque>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class World;

/// One human. Its cell is its base cell, the only one that collides, and
/// no two living humans share a cell. Nobody controls it: it explores on its
/// own, knowing only what it saw, greets humans it has never met, and tags
/// along behind one holding a key when it has none. Close behind them, it
/// waves and tells them the way to a locked door they have never seen.
///
/// Keys and cakes lie on tables, or dropped on the floor; it takes them
/// from beside the table, or by stepping onto them, even just passing by.
///
/// Everything it does makes it hungrier. It picks up the cakes it finds and
/// carries a few, in plain sight, eating one whenever hungry. A hungry
/// human with none goes for the nearest cake it knows of, or for whoever it
/// last saw carrying one, and punches them: every blow that lands snatches
/// a cake. Hit back, it fights on while it is hungry and strong enough, or
/// runs away. One with cakes to spare, and not starving itself, gives one
/// to a starving human it sees, unless they are too weak to be worth it:
/// those it beats up. Starving wears health down, and at none it dies.
/// Holding a key, a hungry human with nothing to eat, or a frightened one,
/// may make for a locked door before exploring the rest: the hungrier or
/// more frightened, the likelier, and never toward what scared it.
///
/// Wounds bleed health away until they heal, faster the deeper they are,
/// and a deep one can bleed it to death. A beating opens one once enough
/// blows land in a short while; darts and spikes pierce. Nothing heals the
/// rest of it while a wound still bleeds. The dead bleed a little longer.
///
/// Traps are hidden. It may spot one on the floor beside it, and learns
/// any it sees go off; it keeps off those it knows, and leaves the cakes on
/// plates it knows alone, but a starving human braves darts for a cake.
/// Spikes leave it nearly dead and bleeding hard, and few live through
/// it; darts never kill, bleeding or not.
///
/// The dead keep what they carried. A body seen is remembered, and once the
/// fright of it passes one may kneel by it and go through it, taking the
/// keys and what cakes it has room for: the greedy and the brave sooner,
/// the shaken later, the timid only for cakes when hungry. Cakes on a body
/// are in plain sight, keys only found by searching it; anyone who sees a
/// body gone through counts it picked over, and thinks a little worse of
/// whoever did it, unless the dead was their foe.
///
/// With nothing to do it often sits down on the floor a while, which makes
/// it hungrier more slowly. Seated, it glances round now and then, and gets
/// up for anything new; nobody walks up to greet it until it is standing.
///
/// It learns from going without. Starving with no cake teaches it to keep
/// more cakes and fetch them sooner; a dead end at a locked door with no
/// key teaches it to fetch keys before exploring on. Each lesson sticks
/// harder when it pays off, and fades over half an hour.
///
/// It knows only what it saw: where cakes lay, who carried how many, and
/// where the traps are. The one exception is the way to a locked door a
/// follower told it about.
///
/// It talks as it deals with others: greeting, telling, begging, giving,
/// threatening, fighting, finding the dead. Each line is picked from
/// data/lines.txt by the moment, how it is doing and what it is like (see
/// talk.h), shows over its head a while, and is heard by everyone near
/// who can see it. What is said does something: warm and rude lines move
/// trust, a plea may move a holder to share, fear spreads, and a threat
/// may get a cake handed over, or a grudge.
class Human {
public:
    /// Waking is the first thing every human does: lying where it came to,
    /// knowing nothing, until it sits up (Sitting) and looks round.
    enum class State { Waking, Idle, Sitting, Rising, Walking, Greeting, Eating, Drinking, Relieving, Punching, Giving, Washing, Looting, Feeding, Sleeping, Sipping, Filling, Pouring, Stashing, Dead };
    /// Why the human is walking: what it does on arrival.
    enum class Goal { Explore, FetchKey, OpenDoor, PassDoor, Wander, Meet, Follow, Company, FetchCake, Drink, Relieve, Fight, Flee, Share, CheckDoor, Wash, Avoid, Escort, Escorted, Track, GiveWay, Loot, Feed, Sleep, Rob, Watch, Fill, Ask, Pick };
    /// What the human is doing, as far as hunger, thirst and tiredness go:
    /// each costs its own of all three.
    enum class Action { Rest, Sit, Walk, Run, Greet, Eat, Drink, Relieve, Punch, Give, Wash, Loot, Feed, Sleep, Watch, Fill, Pour, Stash, Count };
    /// What a human may set out to do once nothing presses. How much it
    /// wants each, by its traits and hunger, is weighed in choose(), which
    /// tries them from the most wanted down; the table of wants is in
    /// human_tuning.h. In the order they tie in.
    enum class Want { Food, Drink, Relieve, Sleep, Flesh, Hunt, Track, Escort, Wash, Fill, Meet, Rescue, UrgedDoor, Loot, Rob, Fetch, Follow, Company, Explore, OpenDoor, CheckDoor, Wander, Count };

    /// An empty name gives the one grown from seed.
    Human(int id, const HumanLook& look, Vec2i cell, uint64_t seed, std::string chosenName = {});

    void update(World& world, float dt);
    /// Looks around and remembers what it sees. True when it spots a key or
    /// a human it didn't know about.
    bool observe(const World& world);
    /// How far through the current step, 0 at prev to 1 at cell.
    float stepProgress() const;
    Action action() const;
    bool dead() const { return state == State::Dead; }
    bool hungry() const;
    bool starving() const;
    bool thirsty() const;
    bool parched() const;
    /// Tired enough to want sleep; past exhausted it walks slower, and at
    /// full tiredness it drops asleep where it stands (human_sleep.cpp).
    bool tired() const;
    bool exhausted() const;
    /// Getting up from sleep: still curled, then up.
    bool rousing() const { return sleep_.roused && state == State::Rising; }
    /// Woke from a dream lately, half a memory of before.
    bool dreamt() const;
    /// Died in its sleep: lies curled as if still asleep.
    bool diedAsleep() const { return sleep_.diedAsleep; }
    /// Kneeling by a sleeper to take a cake off them, not by a body.
    bool robbing() const { return sleep_.robbee >= 0; }
    /// The sleeper it keeps watch over; -1 none.
    int watching() const { return sleep_.watchee; }
    /// Wants to go, and can hold it no longer; see human_relief.cpp.
    bool needsToGo() const;
    bool bursting() const;
    /// Derived from its traits, 0 to 1: how much it minds being seen going,
    /// and how long it holds on.
    float modesty() const;
    /// While Relieving: squatting over feces, else standing to piss.
    bool shitting() const { return relief_.shitting; }
    /// Health below which it runs from a fight, and how long a fright
    /// lasts, by how brave it is.
    float fleeHealth() const;
    float fearTime() const;
    /// Most cakes it carries, the health below which a starving human gets
    /// beaten up instead of fed, and the cakes it keeps before sharing, by
    /// how greedy it is.
    int carryMax() const;
    float preyHealth() const;
    int spareFrom() const;
    /// Hit by someone, for this much health.
    void takeHit(World& world, Human& from, float damage);
    /// Marks by as the one who killed it, and tells by so when by sees it
    /// die. With witnesses (a last blow, always seen by the one who struck
    /// it) onlookers remember the kill too.
    void creditKill(World& world, Human& by, bool witnesses);
    /// Struck by a dart flying along dir for this much health, though never
    /// killed, not even by the bleeding.
    void hitByDart(float damage, Vec2i dir);
    /// Stepped on spikes: left nearly dead, however healthy, and bleeding hard.
    void impale(World& world);
    /// Saw the trap at the cell, or saw it go off: mine when it set it off.
    void learnTrap(Vec2i cell, TrapKind kind, bool mine = false);
    /// Found a trap at a cell it was told was clear: the teller lied.
    void trapWhereTold(Vec2i cell);
    /// Set off a slam room whose gate is at gate (human_rescue.cpp).
    void shutIn(Vec2i gate);
    /// Heard caller, shut in behind gate, cry for help through the walls:
    /// with a key to spare, it may go and let them out.
    void hearHelp(const Human& caller, Vec2i gate);

    const int id;
    /// Its first name, grown from its seed; see name.h.
    const std::string name;
    HumanLook look;
    /// Rolled at birth; see traits.h.
    const Traits traits;
    /// What it taught itself from going without, 0 to 1 each. Unlike its
    /// traits these grow with every hard lesson, a little more each time it
    /// pays off, and are slowly forgotten. Like its traits they only weigh
    /// what it wants, never letting it do what it couldn't do before.
    struct Lessons {
        /// Went starving with no cake: carries more cakes, and goes for
        /// those it knows of before exploring further.
        float hoard = 0.0f;
        /// Ran out of places to go with a locked door before it and no key:
        /// goes for the keys it knows of before exploring further.
        float keysFirst = 0.0f;
        /// Went parched with no water it could get to: keeps near the
        /// water it last drank at (human_thirst.cpp).
        float nearWater = 0.0f;
        /// Dropped asleep worn out wherever it stood: plans ahead harder,
        /// making for where it last slept safely sooner (human_sleep.cpp).
        float safeSleep = 0.0f;
    };
    Lessons lessons;
    Vec2i cell, prev;
    State state = State::Waking;
    Goal goal = Goal::Explore;
    /// Stepping between two cells right now.
    bool moving = false;
    bool facingLeft = false;
    float timer = 0.0f;
    float stepTime = 0.0f;
    /// Restarts at death, so the fall plays from its start, and likewise on
    /// sitting down and getting up.
    float animTime = 0.0f;
    /// 0 is full, 1 is starving.
    float hunger = 0.0f;
    /// 0 is quenched, 1 is parched to death. Only water quenches it: the
    /// water where it lies, or what it carries in a bottle.
    float thirst = 0.0f;
    /// 0 rested, 1 drops asleep where it stands. Everything it does tires
    /// it, and only sleep rests it.
    float tiredness = 0.0f;
    /// 0 empty, 1 can't hold it: filled by what it drinks and eats, and a
    /// little by the hour, emptied by going.
    float bladder = 0.0f;
    float bowels = 0.0f;
    /// 1 is unhurt, 0 is dead.
    float health = 1.0f;
    /// Counts down after a hit, while the human shows it.
    float hurtTime = 0.0f;
    /// Open wounds, healed ones dropped.
    std::vector<Wound> wounds;
    /// Health lost per second to all wounds together.
    float bleeding() const;
    /// The blood on it, its own or anyone's.
    BodyBlood stains;
    /// Feces on it, as blood is, on the feet from stepping in it: brown
    /// while fresh, darker dry. Washing takes it off (relief step 3).
    BodyBlood dirt;
    /// Piss on it, from wetting itself: yellow while wet, faint dry.
    BodyBlood soaked;
    /// How filthy it looks: the feces on its feet and legs and the piss
    /// on it, 0 to 1.
    float filth() const;
    /// While Punching: the fist is out and the blow has landed.
    bool punchLanded = false;
    int keys = 0;
    /// Cakes carried, the one being eaten included.
    int cakes = 0;
    /// The water bottles it carries, in a back pocket, in plain sight: one
    /// to wake with, full; no more than BOTTLE_MAX, taking another as it
    /// takes a cake, while there is room (human_bottle.cpp).
    static constexpr int BOTTLE_MAX = 2;
    std::vector<Bottle> bottles;
    /// All the water in them, 0 to the number of bottles.
    float bottleWater() const;
    /// The bottle it shows: the one in its hand while drinking, filling,
    /// pouring or handing water over, else the fullest; null if it has none.
    const Bottle* shownBottle() const;
    /// Whether that bottle is in its hand, and, Giving, whether it hands
    /// water over rather than a cake.
    bool bottleInHand() const;
    bool givingWater() const { return state == State::Giving && water_.givingWater; }
    /// Kneeling to put things down, not to pick them up.
    bool putting() const { return state == State::Stashing && stash_.putting; }
    Memory memory;
    Sight sight;
    /// World seconds when it was made, and what it has been through since.
    float born = 0.0f;
    LifeLog log;
    /// The notable things it went through or saw, to talk about
    /// (human_episode.cpp); how much each still matters to it, 0 to 1; and
    /// its own memory of the same thing as e, if it has one.
    const std::vector<Episode>& episodes() const { return episodes_; }
    float salience(const Episode& e) const;
    const Episode* shares(const Episode& e) const;
    /// What it would bring up with to right now, the topic that matters
    /// most to it (human_topic.cpp); nothing if it has nothing to say to
    /// them. The last one it brought up.
    std::optional<Topic> pickTopic(const World& world, const Human& to) const;
    std::optional<Topic> lastTopic;
    /// Keeps what just happened as an episode, here or at a cell, with
    /// whoever it has in sight as having been there.
    void remember(Episode::What what, float strength, int who = -1, int whom = -1, int detail = 0);
    void rememberAt(Vec2i at, Episode::What what, float strength, int who = -1, int whom = -1, int detail = 0);
    /// Everyone this human has greeted.
    std::unordered_set<int> met;
    /// The other's name as this human knows it, or nothing if never told.
    const std::string* knownName(int id) const;
    /// Whom it is dealing with right now: greeting, following, fighting,
    /// running from, feeding, escorting or escorted by. -1 if nobody.
    int counterpart() const;
    /// How many it counts friends, and foes, from what it saw and was told.
    int friendCount() const;
    int foeCount() const;
    /// Whom it keeps company with, -1 if nobody; and whether it is standing
    /// waiting for its group to catch up.
    int companion() const { return social_.companion; }
    bool waitingForCompany() const { return social_.waitedFor > 0.0f && state == State::Idle; }
    /// Greeting a friend, and swapping word of others while at it.
    bool gossiping = false;
    /// What it thinks of another human, -1 (a foe) to 1 (a friend), 0 for
    /// anyone it knows nothing about. Built only from what it saw: who hit
    /// it or was seen hitting someone, who fed it or was seen feeding
    /// someone, who greeted it, led it to water, told it a door, or was
    /// seen covered in someone else's blood. Fades slowly toward 0.
    float trust(int id) const;
    /// Thinks of someone this much better (or worse, below 0) for what it
    /// just saw them do.
    void regard(int id, float delta);
    /// A piece of what one human knows, passed to another at a greeting:
    /// where a cake or key lies, a trap, or a route (the cells of a walk
    /// ending at a locked door, told with the tiles round them). What is
    /// told lands in the listener's memory as if seen, marked as told. A
    /// lie is a route laid over a spike plate the listener doesn't know of.
    /// Between those who trust each other there is also word of people: the names of others,
    /// and what the teller thinks of them, a warning off a foe or a pointer
    /// to an ally, which the listener believes as far as it trusts them.
    struct Tell {
        enum class Kind { Cake, Key, Trap, Route, People, Way, Water };
        Tell(Kind kind, Vec2i cell, TrapKind trap = TrapKind::Spikes) : kind(kind), cell(cell), trap(trap) {}
        Kind kind;
        Vec2i cell;
        TrapKind trap;
        std::vector<Vec2i> route;
        /// For People: one entry per human spoken of, with their name when
        /// the listener doesn't know it and the teller does, and the
        /// teller's opinion of them when firm enough to pass on: what it
        /// says it thinks, which from the cunning may be a lie.
        struct Word {
            int about;
            std::string name;
            std::optional<float> opinion;
            bool lie = false;
        };
        std::vector<Word> people;
        /// For Way: which way the teller says out lies (an angle, as
        /// hunch()), how sure it sounds, 0 to 1, or that there is no way
        /// out at all (gloomy), sure by how much of the place it has seen.
        float way = 0.0f, sure = 0.0f;
        bool gloomy = false;
        /// A route laid over a trap (trap) on purpose; a way pointed wrong on
        /// purpose, at a trap (cell) when it knows one; a cake on a rigged
        /// table; or, nothing, a cake or key where it knows there is none.
        bool lie = false, nothing = false;
        /// A lie for the liar's own good, not out of spite: told to anyone
        /// but friends, foes and those it walks with.
        bool gain = false;
        /// Told for a cake, paid once the greeting ends.
        bool sold = false;
    };
    /// Says the tell to other, in turn; it lands in their memory as it is
    /// said (see deliver), and is lost if it never gets said.
    void tell(Human& other, const Tell& t);
    /// Beyond liking or past bearing: it shares with and follows a friend
    /// sooner, and won't share with, follow, greet, escort or go near a
    /// foe, steals from them first, and holds a grudge against them longer.
    bool isFriend(int id) const;
    bool isFoe(int id) const;
    /// Cunning: greedy and bold enough to lie, to sell what it knows, and
    /// to keep a straight face. It never lets a foe hear it in its words,
    /// unless it is fighting them or afraid of them (masks).
    bool cunning() const;
    bool masks(int id) const;
    /// A slow fear, 0 to 1, that every scare adds to, the timid more, and
    /// that fades over minutes, faster for the brave and while sitting.
    /// Scares: being hit, running from a fight, a trap going off on it or
    /// in sight, seeing a punch land, a fresh pool of blood, a bloodied
    /// suspect, and a corpse seen for the first time. Shaken, a human
    /// explores less, strolls shorter and sits more, keeping to known
    /// ground, runs from a fight sooner, and, with a key, is likelier to
    /// put a door between it and all that. fright() folds it in.
    float dread() const { return dread_; }
    void scare(float how);
    /// How frightened it is, 0 to 1: the fright of whoever it ran from
    /// while that lasts, or its dread, whichever is more.
    float fright() const;
    /// How dazed it still is from waking up, 1 just come to, 0 clear-headed:
    /// it sees less far and walks slower while it lasts.
    float dazed() const { return escape_.daze; }
    /// Thinks them a killer for the blood on them; still afraid of them since they hit it.
    bool suspects(int id) const { return suspicious(id); }
    bool afraidOf(int id) const { return fight_.fear > 0.0f && fight_.feared == id; }
    /// Has seen id dead, and knew them for who they were.
    bool knowsDead(int id) const { return deaths_.seenDead.count(id) > 0 && !deaths_.nameless.count(id); }
    /// Can tell whose body id is: not bones it never saw as more.
    bool knowsWhose(int id) const { return !deaths_.nameless.count(id); }
    /// How badly it wants out of here, 0 to 1: everyone wakes wanting out,
    /// and settles to a steadier want after a while. It is why humans
    /// explore, and what sends a key holder to doors early.
    float outUrge() const { return escape_.out; }
    /// Which way it feels the way out lies, as an angle (0 east, π/2 south).
    /// Exploring heads that way more than others.
    float hunch() const { return escape_.hunch; }
    float hunchSure() const { return escape_.hunchSure; }
    /// Whether it still believes there is a way out, 0 to 1; see
    /// human_hope.cpp. New places and opened doors lift it; dead ends,
    /// bodies, starving and more of the same wear it down. Without hope it
    /// settles: gives up looking for a way out and makes a home of a place.
    float hope() const { return escape_.hope; }
    /// How much of its hope is gone below HOPE_LOW, 0 to 1: it sits more,
    /// strays less, and its mind starts to give.
    float gloom() const;
    bool settled() const { return escape_.settled; }
    Vec2i home() const { return escape_.home; }
    /// A line being said: what, in what tone, how long it has shown and
    /// how long it shows, and to whom (-1 for nobody).
    struct Speech {
        std::string text;
        talk::Tone tone;
        float shown, time;
        int to = -1;
        /// Telling the way out: the way it points while it says it.
        std::optional<float> points;
    };
    /// The line over its head right now, if any.
    const Speech* speaking() const { return voice_.speech.empty() ? nullptr : &voice_.speech.front(); }
    /// Chats it has started (human_chat.cpp).
    int chats = 0;

    /// The mind, 0 to 1 each; see human_flesh.cpp. Erratic grows from
    /// starving, deep dread, half-eaten bodies and a taste for flesh, and
    /// fades while fed and calm; from ERRATIC on the human is erratic, and
    /// eats, and hunts, the living when merely hungry. Eating flesh
    /// leaves guilt in a steady mind and appetite in an erratic one: guilt
    /// haunts, keeps it apart and holds it back from doing it again, until
    /// too much of it breaks the mind; appetite asks for more.
    float erratic() const { return mind_.erratic; }
    bool isErratic() const;
    float guilt() const { return mind_.guilt; }
    float appetite() const { return mind_.appetite; }
    /// Hunting someone to eat them: a fight that only ends with a death.
    bool hunting() const { return mind_.prey >= 0 && fight_.foe == mind_.prey; }
    /// Dead: the meals of flesh left on the body, seconds since death, and
    /// how far gone it is, 0 fresh to 1 bare bones. Past spoiled() it
    /// makes whoever eats it sick.
    static constexpr int FLESH_MEALS = 3;
    int flesh = 0;
    float deadFor() const { return dead() ? clock_ - deaths_.diedAt : 0.0f; }
    /// Where the bodies it knows of lay when it last saw them.
    std::vector<Vec2i> bodyCells() const {
        std::vector<Vec2i> cells;
        for (const auto& [id, b] : deaths_.bodies) cells.push_back(b.cell);
        return cells;
    }
    float decay() const;
    bool spoiled() const;
    bool bones() const { return decay() >= 1.0f; }
    /// Meals of flesh it can still give: none once down to the bones.
    int meals() const;

private:
    /// tests/stuck_soak.cpp reads how walks are going.
    friend struct SoakProbe;
    // Each system's state is a struct (voice_, chat_, sleep_, ...) placed
    // beside that system's methods; what the core loop uses everywhere
    // (rng_, clock_, path_, rethink_, dread_, ...) stays loose.
    mutable Rng rng_;
    /// Something to say, to whom and about whom (-1 for nobody), said in
    /// turn at a later update, where the world is at hand; and how long it
    /// has waited for others to finish.
    /// A saying may carry what it tells (word, or the speaker's name),
    /// passed on only as it is said; a quiet one is passed on without a
    /// line. Awaited: the speaker stands waiting until it is said.
    struct Saying {
        Saying(talk::Moment moment, int to = -1, int about = -1) : moment(moment), to(to), about(about) {}
        talk::Moment moment;
        int to, about;
        float waited = 0.0f;
        std::optional<Tell> tell;
        /// A topic brought up in a chat; kept as raised once said.
        std::optional<Topic> topic;
        bool intro = false, quiet = false, awaited = false;
    };
    /// What it says, in turn (human_speech.cpp).
    struct Voice {
        /// The line being said, if any; one at a time.
        std::deque<Speech> speech;
        /// World time its last line ended.
        float quietSince = -1e9f;
        std::vector<Saying> sayings;
        float answerWait = 0.0f;
        /// The chat lines it has said, and when (world time), so it words
        /// things differently next time.
        std::unordered_map<std::string, float> chatLines;
        /// When it last begged each human, and last threatened them; and whose
        /// pleas moved it to share, until when.
        std::unordered_map<int, float> begged, threatened, moved;
    };
    Voice voice_;
    /// False if the saying didn't fit in the queue and goes unsaid.
    bool sayLater(talk::Moment m, int to = -1, int about = -1);
    bool sayLater(Saying s);
    void chatOn(Human& to, const Saying& s);
    void answered(const Topic& t, int to);
    /// Word for `to` still waiting to be said.
    bool hasWordFor(int to) const;
    /// What a tell said, and a name given, do to the one told.
    void deliver(Human& other, const Tell& t);
    void giveName(Human& other);
    /// Waiting for its threat or plea to be said; then voice_.answerWait more for an answer.
    bool awaiting_ = false;
    /// Marks the saying just queued as awaited, the wait set by startIdle
    /// kept for after it is said.
    void awaitLine();
    /// Saying something, or about to.
    bool talking() const { return !voice_.speech.empty() || !voice_.sayings.empty(); }
    /// Chit-chat (human_chat.cpp, human_topic.cpp).
    struct Chat {
        /// The urge to talk to each human it is together with, 0 to 1, grown
        /// while both are calm and quiet; a full one starts a chat.
        std::unordered_map<int, float> urge;
        float tick = 0.0f;
        /// By listener: what was raised with them, and when (for Someone, the
        /// opinion it had then).
        std::unordered_map<int, std::unordered_map<uint64_t, float>> talked;
        /// When it last sat down.
        float restedAt = 0.0f;
    };
    Chat chat_;
    /// Chit-chat (human_chat.cpp): calm enough to chat with someone, and
    /// together with them.
    bool calmForChat(const Human& with) const;
    bool togetherWith(const Human& o) const;
    /// Nothing pressing on it: not hungry, thirsty, needing to go, bleeding,
    /// frightened or just hurt. Someone it has met, not a foe or suspect,
    /// alive, in sight and this close.
    bool easyInMind() const;
    bool knowsInSight(const Human& o, int range) const;
    /// Its urge to chat with o has come full.
    bool wantsWord(int o) const {
        auto it = chat_.urge.find(o);
        return it != chat_.urge.end() && it->second >= 1.0f;
    }
    void chatUrges(World& world, float dt);
    /// Slam rooms: shut in, and letting others out (human_rescue.cpp).
    struct Rescue {
        /// The gate of the slam room it was shut in, while it stays shut, and
        /// when it next cries for help.
        std::optional<Vec2i> slamGate;
        float helpTimer = 0.0f;
        /// Who it means to let out of a slam room, and where the gate is; whose
        /// cries for help it has heard, each a scare once.
        int rescuee = -1;
        Vec2i gate{};
        std::unordered_set<int> heardHelp;
    };
    Rescue rescue_;
    /// Shut in a slam room with no key, it cries for help now and then,
    /// loud enough to carry through walls (human_rescue.cpp).
    void callForHelp(World& world, float dt);
    /// Nowhere to go but a few cells: walled in.
    bool walledIn(const World& world) const;
    /// Makes for the gate of whoever it means to let out; false, and gives
    /// up on them, if it can't.
    bool startRescue(World& world);
    /// Has just opened a gate: whoever was shut in behind it is let out,
    /// meant or not. True if anyone was.
    bool letOut(World& world, Vec2i gate);
    void startChat(World& world, Human& o);
    /// A line passed between the two: the silence starts over for both.
    void quiet(Human& o);
    /// How it would answer sp bringing up t, from its own memory, opinion
    /// and feelings; the next line of a chat after s, said by to; and,
    /// having answered, keeping the topic as talked of with them.
    Topic::Reply replyTo(const Human& sp, const Topic& t) const;
    /// Raised: kept, so it isn't brought up with them again until something
    /// changes. Owns: it would talk of e at all (the steady keep quiet
    /// about killing, eating or robbing the dead themselves).
    void raised(const Topic& t, int to);
    bool owns(const Episode& e) const;
    /// Moves the line showing along, and says the next one when its turn
    /// comes: after a pause, and not over whoever it talks to, nor over
    /// anyone talking to it. Cries don't wait their turn.
    void speak(World& world, float dt);
    /// Picks a line for the saying, shows it, logs it, and lets everyone in
    /// earshot who can see it hear it, all at once, so what it does to them
    /// comes as it shows.
    void say(World& world, const Saying& s);
    /// Hears a line the speaker said to `to` (nobody in particular if null),
    /// and takes it as its tone says.
    void hear(World& world, Human& speaker, const talk::Said& said, Human* to, const Human* about);
    bool movedBy(int id) const;
    /// Hungry with no cakes: begs a holder close by, unless it begged them
    /// lately, and waits a moment to see. False if it doesn't.
    bool plead(World& world);
    /// Fighting for cakes and close: threatens the holder first, unless it
    /// did lately, and waits a moment to see. False if it doesn't.
    bool threaten(Human& f);
    /// Puts what just happened in the log, at the world's time.
    void note(std::string what, int who = -1, int whom = -1);
    std::vector<Episode> episodes_;
    /// The living others it saw at its last look round.
    std::vector<int> inSight_;
    /// Cells still to walk to, in order, and the last of them.
    std::deque<Vec2i> path_;
    Vec2i target_;
    /// Something changed; decide again before the next step.
    bool rethink_ = false;
    /// Steps in a row someone stood in the way.
    int stuck_ = 0;
    /// At a dead end with no key, already taken to heart: one lesson per
    /// dead end, however long it stays there.
    bool deadEnd_ = false;
    /// Learns from going without (hard) or from a lesson paying off.
    void learn(float& lesson, bool hard);
    /// How long it keeps fighting back whoever just hit it: longer for a foe.
    float grudgeFor(int id) const;
    float dread_ = 0.0f;
    /// Which of the first key, cake, door, water, human and footprints it
    /// has remarked on, a bit each in the order of talk::Moment from FirstKey.
    unsigned firstSights_ = 0;
    /// A body it has seen, by whose it is: where it lies, the cakes last
    /// seen on it, and whether it saw it gone through, by itself or another,
    /// so nothing more is to be found on it than those cakes; and the meals
    /// of flesh last seen on it, and whether it had spoiled.
    struct Body {
        Vec2i cell;
        int cakes = 0;
        bool searched = false;
        int meals = 0;
        bool spoiled = false;
        /// The bottles last seen on it, and the water in them.
        int bottles = 0;
        float water = 0.0f;
    };
    /// The dead: those it saw, the bodies it knows, and its own death.
    struct Deaths {
        /// Corpses it has seen, each a scare once, and who beat it to death.
        std::unordered_set<int> seenDead;
        /// Bones it first saw as bones: someone's, it can't tell whose.
        std::unordered_set<int> nameless;
        int killer = -1;
        /// Whether the killer saw it die; if not, it learns it on finding the body.
        bool killerSaw = false;
        std::unordered_map<int, Body> bodies;
        /// The body it is going to, or kneeling by, to go through.
        int corpse = -1;
        /// Clock time of death.
        float diedAt = 0.0f;
    };
    Deaths deaths_;
    /// The mind and flesh (human_flesh.cpp).
    struct Mind {
        float erratic = 0.0f, guilt = 0.0f, appetite = 0.0f;
        /// Clock times it last said it was haunted, or craving, so it doesn't keep on.
        float hauntedAt = -1e9f, cravedAt = -1e9f;
        /// Eating something rotten: no healing until this runs out.
        float sick = 0.0f;
        /// Who it is hunting, or killed to eat and has yet to eat. -1 if nobody.
        int prey = -1;
        /// The body it is going to, or kneeling by, to eat from.
        int meal = -1;
        /// Bodies it has eaten from; humans it has seen eat a body; bodies it
        /// has seen half eaten.
        std::unordered_set<int> ateFrom, eaters, gnawedSeen;
    };
    Mind mind_;
    /// Keeps the mind moving: erratic, guilt and appetite growing and fading.
    void mind(float dt);
    /// How hungry it must be before it eats flesh, or hunts for it; above 1
    /// never.
    float fleshBar() const;
    float huntBar() const;
    /// Whether it would eat from this body, as it remembers it.
    bool wouldEat(int id) const;
    /// The way to the nearest body it would eat from, and whose it is.
    std::optional<std::vector<Vec2i>> fleshPath(const World& world, int* whose = nullptr) const;
    /// Walks to the nearest body it would eat from, or kneels to eat once
    /// beside it; only from body when given. False if there is none.
    bool startFeed(World& world, int body = -1);
    /// One meal off mind_.meal, and what eating it does to the mind. Everyone
    /// who sees it knows what it is now.
    void feed(World& world);
    /// Someone in sight it would kill to eat: the weakest, then the nearest.
    Human* preyInSight(const World& world) const;
    bool startHunt(World& world);
    /// Saw eater eat from body.
    void sawEat(Human& eater, const Human& body);
    /// What it sees of the dead: half eaten, or one it ate from.
    void seeRemains(const Human& body);
    /// Whether the body is worth going to: never gone through, so it may
    /// hold keys, or with cakes on it and room to carry them. With cakesOnly,
    /// only the cakes count.
    bool worthLooting(int id, bool cakesOnly = false) const;
    /// The way to the nearest body worth going through, and whose it is.
    std::optional<std::vector<Vec2i>> lootPath(const World& world, bool cakesOnly, int* whose = nullptr) const;
    /// Walks to the nearest body worth going through, or kneels by it once
    /// beside it. False if there is none it can get to.
    bool startLoot(World& world, bool cakesOnly = false);
    /// Takes the keys off deaths_.corpse and the cakes it has room for. Everyone
    /// who sees it knows the body was gone through.
    void loot(World& world);
    /// Making for a locked door out of hunger or fright, ahead of exploring,
    /// and how long before it weighs that again.
    bool urged_ = false;
    float urgeWait_ = 0.0f;
    /// Seated: until the next look round.
    float glance_ = 0.0f;
    /// Waking, wanting out, hope and settling (human_hope.cpp, AI/escape_plan.md).
    struct Escape {
        /// See dazed(). Sitting up after waking, it stays sat for a moment
        /// whatever it sees (escape_.comingTo).
        float daze = 1.0f;
        bool comingTo = false;
        /// See outUrge() and hunch().
        float out = 1.0f;
        float hunch = 0.0f;
        /// The last door urge came from wanting out, and that was noted.
        bool outUrged = false, urgedNoted = false;
        /// See hope(). Chunks it has set foot in, by id; when it last saw
        /// somewhere it had never seen. Settled, it keeps to home, and an
        /// intruder found there at intrudedAt is driven off if it stays.
        float hope = 0.7f;
        std::unordered_set<int> chunksIn;
        float lastFound = 0.0f;
        bool settled = false;
        Vec2i home;
        int intruder = -1;
        float intrudedAt = 0.0f;
        std::unordered_set<int> askedWay;
        /// How sure it is of its hunch, 0 to 1: a vague feeling on waking,
        /// firmer as others say the same, shakier as they don't. What it last
        /// told each human of the way: the angle, or below -TAU none at all.
        float hunchSure = 0.3f;
        std::unordered_map<int, float> wayTold;
    };
    Escape escape_;
    /// Hope moved by how: lost faster by the timid. Notes and mutters
    /// when it sinks below HOPE_LOW or comes back up.
    void lift(float how);
    /// Once a step: lifted or worn down by the chunk it is in.
    void enterChunk(const World& world);
    /// Once a tick: hope worn down by finding nothing new for long, and
    /// settling or setting off again.
    void brood(const World& world);
    /// Wanting out as it weighs now: the urge times how much it still
    /// hopes; nothing once settled.
    float outDrive() const;
    /// Settled: driving off whoever comes into its place, if it is the
    /// kind to. True when it started something.
    bool guardHome(World& world);
    /// Whether this human keeps its place against others: by its traits,
    /// and always once erratic.
    bool territorial() const;

    /// The extra a walk to c costs for leading away from the hunch, up to
    /// most right against it, the more the more it wants out: none straight
    /// ahead.
    int hunchCost(Vec2i c, int most) const;
    /// Lying there done: it sits up, mutters and starts looking round.
    void sitUp();
    /// Blood thrown by a hit that wounded, the way the blow went (zero all
    /// round), and how hard: thrown at the next update, where the world is.
    struct Spray {
        float dx, dy, damage;
        BodyPart part;
    };
    /// Those seen with blood on them but no wound to show for it: until when
    /// it keeps away from them, and how much their blood, by freshness and
    /// amount, weighed last time (0 to 1).
    struct Suspect {
        float until;
        float weight;
    };
    /// Blood: its own spilling, on the floor and on others (human_blood.cpp).
    struct Gore {
        /// Drops of blood owed, a whole one falling at each 1.
        float drip = 0.0f;
        std::vector<Spray> sprays;
        /// The cell last checked for blood underfoot, and the steps left that
        /// still print a bloody footprint.
        Vec2i trod;
        int bloodySteps = 0;
        /// Whose blood the feet print, from the stain last stepped in.
        int feetBlood = -1;
        /// Seconds until wet blood on the body next runs down a pixel.
        float run = 0.0f;
        /// The water cell being washed in.
        Vec2i washAt;
        /// A fresh pool of blood that scared it, and until when it stays away.
        Vec2i scare;
        float scaredUntil = 0.0f;
        std::unordered_map<int, Suspect> suspects;
        /// The blood each trail step led to, and when, so it doesn't pace back
        /// and forth at a trail's end.
        std::unordered_map<Vec2i, float, Vec2iHash> tracked;
    };
    Gore gore_;
    /// What the blood on a cell in sight tells it: a trap beside it, or a
    /// pool. True if that is news.
    bool noticeBlood(const World& world, Vec2i c);
    /// Whether someone in sight looks suspicious. True if newly so.
    bool judge(const Human& o);
    bool suspicious(int id) const;
    /// Extra cost of walking over a cell, for the blood it remembers there.
    int bloodCost(Vec2i c) const;
    /// Walks off from a fresh pool that scared it, or from a suspect too
    /// close, unless starving and they carry cakes. False if neither.
    bool keepAway(World& world);
    /// Whether o, seen, is someone it keeps away from: a suspect or a foe,
    /// unless starving and they carry cakes, or it is fighting them. Its
    /// walks stay out of their way too, so it doesn't walk back up to them.
    bool shuns(const Human& o) const;
    /// For a walk from start: whether a cell comes too near someone it
    /// shuns. Empty when it shuns nobody in sight.
    std::function<bool(Vec2i)> shunZone(const World& world, Vec2i start) const;
    /// Taking someone to water, or being taken (human_blood.cpp, human_thirst.cpp).
    struct Escorting {
        /// The wounded human this one is taking to water, the floor beside the
        /// water it takes them to, and whether it got there; and the one taking
        /// this human to water.
        int escortee = -1;
        Vec2i to;
        bool arrived = false;
        int by = -1;
        /// Leading escortee to drink, not to wash (human_thirst.cpp).
        bool drink = false;
        /// Whom it last led to drink, and when: not again straight away.
        std::unordered_map<int, float> ledToDrink;
    };
    Escorting escorting_;
    /// It would lead o, a parched (or thirsty and not going to water)
    /// friend or one it walks with, to water to drink.
    bool wouldLeadToDrink(const Human& o) const;
    /// How much more it wants to escort for someone in sight it would lead
    /// to drink: LEAD_PARCHED if anyone, else nothing.
    float parchedToLead(const World& world) const;
    /// Leads the nearest such human to the nearest water it would drink.
    bool offerDrinkEscort(World& world);
    /// Drinking and water (human_thirst.cpp).
    struct Water {
        /// The water cell being drunk from.
        Vec2i drinkAt;
        /// Where it last drank, and whether it ever has; when it last got
        /// thirsty; when new water last lifted its hope.
        Vec2i last;
        bool drankYet = false;
        float thirstyAt = 0.0f, hopeAt = -1e9f;
        /// When it last gave offence washing.
        float offendedAt = -1e9f;
        /// Guarding water by its home: whom it lets drink there, until when.
        std::unordered_map<int, float> letDrink;
        /// The guard it owes a cake for water, -1 none; paid (or not) once it
        /// has drunk its fill.
        int owes = -1;
        /// Homes whose settler turned it away from their water, and when.
        std::vector<std::pair<Vec2i, float>> refused;
        float askedAt = -1e9f;
        /// The bottle in hand (an index into bottles) while drinking from
        /// it, filling or pouring it out; -1 none.
        int inHand = -1;
        /// Handing water over, not a cake.
        bool givingWater = false;
        /// When it last asked each human for a drink; and whom it has made up
        /// its mind to ask, and is walking up to (-1 nobody).
        std::unordered_map<int, float> askedDrink;
        int askee = -1;
    };
    Water water_;
    /// Getting thirsty, and parched (with what that may teach it).
    void gotThirsty();
    void gotParched(const World& world);
    /// Thirsty, new water seen: a little hope, now and then.
    void foundWater();
    /// Where it strolls from: home if settled; else, as far as it has
    /// learnt to keep near water, the water it last drank at.
    Vec2i strollFrom();
    /// How much less it wants to explore for being far from that water.
    float waterTether() const;
    /// Healthy, fed and with a cake, it would take o, wounded and bloody,
    /// to water.
    bool wouldEscort(const Human& o) const;
    /// Takes the nearest such human in reach to the nearest water either of
    /// them knows. False if there is none, or no water.
    bool offerEscort(World& world);
    /// Leads escorting_.escortee on, waits for them to catch up, or waits by the water
    /// while they wash. False once the escort is over.
    bool leadEscort(World& world);
    /// Keeps close behind escorting_.by. False once the escort is over.
    bool followEscort(World& world);
    /// Hungry with no cakes: walks to the freshest blood in sight, fresher
    /// than any underfoot, toward whoever is bleeding. False if none.
    /// Blood it already walked up to is left alone a while.
    bool track(World& world);
    /// Puts ink of blood on pixels of the body, one at a time, picked from
    /// the given parts; with low, from the lowest of them, as on a hand.
    void stain(std::initializer_list<BodyPart> parts, int pixels, int ink, bool low = false);
    /// Wet blood on the body runs down and dries.
    void dryStains(float dt);
    /// Bloody, not hungry and not fighting or running: walks to the nearest
    /// water it knows, or starts washing when beside it. False if it won't.
    bool startWash(World& world);
    /// Washes a bit more blood off into the water beside it, or stops once
    /// clean or something else comes first.
    void wash(World& world);
    /// Seconds lived, and when it last stood by each locked door cell it
    /// knows, to tell whether it is worth going back to see if it opened.
    float clock_ = 0.0f;
    std::unordered_map<Vec2i, float, Vec2iHash> doorChecked_;

    /// Hunger, thirst and health over dt, for whatever the human is doing.
    void live(World& world, float dt);
    /// Heals the wounds over dt and returns the health they bled. lethal
    /// is what came from wounds that can kill, out of it.
    float bleedOut(float dt, float& lethal);
    /// Drips blood from the wounds over dt, and throws the spray of the hits
    /// taken since. In human_blood.cpp.
    void spill(World& world, float dt);
    /// Opens a wound, or deepens the one of that kind still open there.
    Wound& wound(WoundKind kind, BodyPart part, float damage, bool lethal = true, int by = -1);
    void choose(World& world);
    /// Starts on the want. False if it can't be done right now.
    bool pursue(World& world, Want w);
    void step(World& world);
    /// Face to face with o in a way too narrow to pass, or o standing about
    /// in it: whichever of the two has the shorter step out of the other's
    /// way takes it and waits there a moment. False if neither can.
    bool makeWay(World& world, Human& o);
    /// The way to the nearest free cell off passer's path, if any.
    std::optional<std::vector<Vec2i>> asideFrom(const World& world, const Human& passer) const;
    void arrive(World& world);
    void move(World& world, Vec2i next);
    void startIdle(float lo, float hi);
    /// Idles a while, more often than not sitting down for longer.
    void rest();
    void sitDown();
    void startRising();
    void startWalking(Goal g, std::vector<Vec2i> path);
    void startPassDoor(Vec2i door);

    /// Others: whom it deals with, keeps with, trusts and owes (human_social.cpp, human_company.cpp).
    struct Social {
        /// The human being walked up to or greeted, and the one being followed.
        int partner = -1;
        int leader = -1;
        /// Where each human was when last in sight.
        std::unordered_map<int, Vec2i> lastSeen;
        std::unordered_map<int, float> trust;
        /// How many cakes each human carried when last in sight; none are left out.
        std::unordered_map<int, int> seenCakes;
        /// How much water each human carried in bottles when last in sight;
        /// none are left out.
        std::unordered_map<int, float> seenWater;
        /// Who this human is walking up to give a cake.
        int sharee = -1;
        /// Whoever it owes a cake for a tip, paid as the greeting ends.
        int owes = -1;
        int companion = -1;
        /// Seconds waited in a row for the group, back to 0 once they are close.
        float waitedFor = 0.0f;
        /// When it last greeted each human, and when it set out for partner,
        /// to tell a greeting done since from one still owed.
        std::unordered_map<int, float> greetedAt;
        float partnerSince = 0.0f;
        /// When each human last wouldn't greet it back; not tried again for a while.
        std::unordered_map<int, float> snubbed;
        /// When each human was last too busy to be greeted; let be a while.
        std::unordered_map<int, float> busy;
    };
    Social social_;
    /// Walks toward social_.partner, or greets them once close. False if they're lost.
    bool startMeet(World& world);
    void tryGreet(World& world, Human& other);
    void greet(Human& other);
    /// Both tell each other something at a greeting: a lie to a foe, or
    /// honest news, which the greedy sell for a cake paid once the greeting
    /// is over.
    void exchange(World& world, Human& other);
    /// What it would honestly tell other: a cake when they are hungry and
    /// have none, a trap they don't know of, a key when they have none,
    /// in that order; nothing if it has none, or keeps them back. The
    /// greedy keep cake tips from all but friends, and sell trap tips to
    /// all but friends, to those with a cake to spare: forPay says so.
    std::optional<Tell> newsFor(const Human& other, bool& forPay) const;
    /// Cunning, and facing a foe: hungry with no cake, a cake on a rigged
    /// table it doesn't know is rigged; holding keys, a route to a locked
    /// door the foe has never seen that crosses a trap it doesn't know of;
    /// else a false tip, a cake (to the hungry) or a key (to the keyless)
    /// on a table far off that the foe has never seen, where the liar
    /// knows there is none. Nothing if it isn't that sort, or has none.
    std::optional<Tell> lieFor(const World& world, const Human& foe) const;
    /// A trap it knows of at c that can be walked into, and goes off
    /// again and again: spikes, or darts in the floor. Slam rooms shut
    /// once for good, so they never are.
    bool bait(Vec2i c) const;
    /// The nearest cake it knows of on a table it knows is rigged with
    /// darts, that o knows of neither: what it would send o, hungry, to.
    std::optional<Vec2i> riggedCakeFor(const Human& o) const;
    /// The nearest table it knows has no cake on it (no key, forKey) and no
    /// trap, at least FALSE_TIP_FAR from o and never seen by o: where a
    /// false tip sends o.
    std::optional<Vec2i> bareTableFor(const World& world, const Human& o, bool forKey) const;
    /// Cunning and greedy past GAIN_LIE_GREEDY, to anyone but a friend, a
    /// foe or one it walks with: a lie for its own good. To a hungry
    /// rival near a cake it wants, a cake on a bare table far from it, so
    /// it gets there first; to one with cakes to spare, spikes that aren't
    /// there, for a cake (forPay), when it has no real trap to sell.
    std::optional<Tell> gainLieFor(const World& world, const Human& o, bool& forPay) const;
    /// The route lie of lieFor, for a foe holding keys.
    std::optional<Tell> routeLie(const World& world, const Human& foe) const;
    /// A told cell seen for what it is: nothing there, and the teller is
    /// thought worse of, or as told, and the telling is forgotten.
    void checkTold(Vec2i c, bool asTold);
    /// Whether, greeting other, it gives its name: likelier the more
    /// sociable, when they have just given theirs, and always to a friend.
    /// Never when they know it already.
    bool wouldIntroduce(const Human& other, bool theyDid) const;
    /// Tells other its name.
    void introduce(Human& other);
    /// Between humans who trust each other well both ways (TRUST_CONFIDE in
    /// human_tuning.h): word of people for other, the names it knows that they
    /// don't, and its firm opinions of others that changed since it last
    /// told them, warning them off its foes and pointing out likely allies.
    /// Nothing if the trust isn't there both ways, or nothing new.
    std::optional<Tell> gossipFor(const Human& other) const;
    /// What it thinks of the way out, for other: its hunch, or that there
    /// is none once it has lost hope or settled; to a foe, if cunning, the
    /// wrong way, at a spike plate the foe doesn't know when it can. Not
    /// said again to the same human unless it changed its mind.
    std::optional<Tell> wayFor(const Human& other) const;
    /// Whether, greeting other, it might ask them the way out: only while
    /// it still wants out, never a foe, once per human (escape_.askedWay), and
    /// then seldom (ASK_WAY_CHANCE).
    bool wouldAskWay(const Human& other) const;
    /// Asked the way out by asker, out loud: says what it thinks
    /// (wayFor), or that it has no idea when too unsure or unwilling.
    void answerWay(Human& asker);
    void endGreet(World& world);
    /// Walks after social_.leader, or waits near them. False once it stops following.
    bool startFollow(World& world);
    /// Tells the leader the way to a locked door it knows and they don't, if any.
    void tellLockedDoor(World& world, Human& leader);
    /// Groups (human_company.cpp). Whom it walks after, for a key (social_.leader)
    /// or for company (social_.companion), -1 if nobody.
    int follows() const { return social_.leader >= 0 ? social_.leader : social_.companion; }
    /// How well it must think of someone met to keep with them, by how
    /// sociable it is.
    float companyTrust() const;
    /// Whether id's chain of following leads back to this human.
    bool followedBy(const World& world, int id) const;
    /// Leaving social_.companion: says why, if it can tell them (human_company.cpp).
    void partCompany(const World& world);
    /// The one in sight it would most like to keep with, -1 if none will do.
    int pickCompany(const World& world) const;
    /// Whether it still keeps with social_.companion.
    bool stillCompany(const World& world) const;
    /// Walks after social_.companion (choosing one if it has none), or waits, or
    /// sits, near them. False once it keeps company with nobody.
    bool keepCompany(World& world);
    /// Leading a group: someone in sight keeping with it has fallen behind
    /// and it hasn't waited too long already, so it waits a moment.
    bool waitForCompany(const World& world);
    /// The nearest human in sight worth greeting: a stranger, or someone
    /// not greeted for a while who it has news for or who may have news
    /// for it, or a foe it means to lie to.
    Human* unmetInSight(const World& world) const;
    bool wouldGreet(const Human& o) const;
    /// Cunning, and o is a foe who is hungry with no cake or has no key (a
    /// false tip may do), or doesn't know a trap it could be baited with
    /// and either holds keys or was never pointed the wrong way: worth a
    /// lie. The cheap half of lieFor and wayFor.
    bool lieWorthy(const Human& o) const;
    /// Nothing presses it (hunger, thirst, needing to go): it would go up to a foe to lie.
    bool freeToLie() const { return !hungry() && !thirsty() && !needsToGo(); }
    /// Free to be greeted: standing about, or walking somewhere nothing
    /// presses: not fighting, running, stepping aside, passing a door,
    /// escorting or escorted, keeping away, going to food or a body, nor
    /// waiting on a threat or plea it made.
    bool approachable() const;
    /// Greeted by o, it greets back: anyone but a foe, a suspect, or
    /// whoever it is afraid of.
    bool welcomes(const Human& o) const;

    /// The nearest known cake, or a fight with the nearest human last seen
    /// carrying one, whichever is closer. False if there is no food to be had.
    bool seekFood(World& world);
    void startEating();
    void endEating();
    /// Thirsty: walks to the nearest water it knows with a free cell beside
    /// it, or starts drinking when beside it. False if it knows of none. In
    /// human_thirst.cpp, like the rest of drinking.
    bool seekWater(World& world);
    /// No water to be had, parched or erratic: a toilet it knows will do.
    bool seekToilet(World& world);
    /// Water it can drink from beside the cell, as it remembers it.
    std::optional<Vec2i> waterBeside(Vec2i c) const;
    void startDrinking(Vec2i water);
    /// Bottles (human_bottle.cpp). Whether it would drink what is in b, which
    /// it sees through the glass: as for water, not too foul or red, unless
    /// parched, erratic or craving.
    bool wouldSip(const Bottle& b) const;
    /// The bottle it would drink from, the fullest of those; -1 none.
    int sippable() const;
    /// Drinks from that bottle where it stands. False if there is none.
    bool startSipping();
    /// Another gulp from the bottle, or done.
    void sip(World& world);
    /// What a gulp of water this foul and red does to it, wherever it came
    /// from; at is the water cell drunk from, if any, to remember it by.
    void swallow(World& world, float foul, float red, std::optional<Vec2i> at, bool fromBottle);
    /// Beside water it would drink (or a toilet, parched or erratic), with a
    /// bottle to fill or to pour out first: kneels and does it. False if
    /// there is nothing to do there.
    bool startFilling(World& world, Vec2i water);
    /// Fills a share more, or pours, or done.
    void fill(World& world);
    void pourOut(World& world);
    /// A bottle it would pour out at this water rather than keep: one it
    /// wouldn't drink, or clearly dirtier than this water looks. -1 none.
    int toPour(const World& world, Vec2i water) const;
    /// How foul and red the water at a cell truly is (a toilet's as foul as
    /// it is dirty), for what goes into a bottle.
    void waterHere(const World& world, Vec2i water, float& foul, float& red) const;
    /// How much more than WANTING's base it wants to go and fill up: below
    /// 0 when it doesn't.
    float fillDrive() const;
    /// Some bottle wants filling, or pouring out and filling, at this water.
    bool wantsFill(const World& world, Vec2i water) const;
    /// Low on water and not thirsty: walks to water to fill up.
    bool seekFill(World& world);
    /// Seen a human carrying water, and how much, as it last saw it.
    const Human* nearestWaterHolder(const World& world) const;
    /// Thirsty with nothing to drink: asks someone it sees with water, close
    /// by, walking up to them first. False if it doesn't.
    bool askForDrink(World& world);
    /// Asked for a drink by asker, out loud: pours them some, sells it for
    /// a cake, lies that it has none, or refuses.
    void answerDrink(World& world, Human& asker);
    /// Pours amount of its water into o's bottle, or hands o a spare
    /// bottle when o has none; for a cake when sold. False if it can't.
    bool giveWater(World& world, Human& o, bool sold);
    /// Putting things down on the floor and picking them up (human_stash.cpp).
    struct Stash {
        /// The cell it kneels at, and whether it puts things down there
        /// (else picks them up); what it puts down: cakes, and which of its
        /// bottles.
        Vec2i at;
        bool putting = false;
        int cakes = 0;
        std::vector<size_t> bottles;
    };
    Stash stash_;
    /// What it would take from the pile on a cell, as it sees it: cakes
    /// while it has room, a bottle with water it would drink (any bottle
    /// when it has none) while it has room, or water poured into its own.
    bool wantsFromPile(const Pile& p) const;
    /// A pile it wants something from, on its cell or beside it: kneels and
    /// takes it. False if there is none.
    bool takeFromPiles(World& world);
    /// Kneels and puts down cakes and bottles (by index) where it stands.
    void startPutting(int cakes, std::vector<size_t> bottles);
    /// Done kneeling: puts down or picks up.
    void finishStash(World& world);
    /// The way to the nearest pile it remembers with what it wants: cakes
    /// (food), water (water).
    std::optional<std::vector<Vec2i>> pilePath(const World& world, bool food, bool water) const;
    /// Sleep, robbing sleepers and keeping watch (human_sleep.cpp).
    struct Sleep {
        /// The sleeper it is robbing, or going to; -1 none.
        int robbee = -1;
        /// Cakes it held, and who was nearest in sight, on falling asleep.
        int cakesAsleep = 0, nearAsleep = -1;
        /// Woken by a blow, it is slow to answer it for this long.
        float stunned = 0.0f;
        bool diedAsleep = false;
        /// When it last woke from a dream; how many times in a row it slept
        /// safely where it last slept.
        float dreamtAt = -1e9f;
        int safeSleeps = 0;
        /// Keeping watch over a sleeper of its group: whom it watches, and, on
        /// the sleeper, who watches over it. Asked by someone to watch, or
        /// woken to take a turn, it watches whatever it is like.
        int watchee = -1, watchedBy = -1, askedWatch = -1;
        /// Robbing the one it keeps watch over.
        bool betraying = false;
        /// Got up from sleep, not from sitting.
        bool roused = false;
        /// Where it last slept and woke unharmed, if anywhere yet; and whether
        /// it lay down there (or at home) this time.
        Vec2i sleptAt;
        bool sleptYet = false, atSleepSpot = false;
    };
    Sleep sleep_;
    /// Tired: walks to the nearest out-of-the-way cell within reach, the
    /// fewer open cells round it the better, and sleeps there; the more
    /// tired, the nearer it settles for. In human_sleep.cpp, like the rest
    /// of sleep.
    bool seekSleep(World& world);
    /// Lies down to sleep where it stands, or drops there exhausted.
    void startSleeping(World& world, bool collapsed);
    /// Asleep: sees nothing, wakes rested, hurt, or once well rested
    /// enough to mind starving, thirst or nearly bursting.
    void sleep(World& world, float dt);
    /// Wakes: abruptly (hit, robbed, a noise), up at once; else slowly,
    /// curled a moment before getting up. by did it, what is the log line
    /// if not the usual. Looks round, and finds what went missing.
    void wakeUp(World& world, bool abrupt, int by = -1, const char* what = nullptr);
    /// How lightly it sleeps, 0 to 1: the timid and frightened lightest.
    float lightSleep() const;
    /// Asleep, it may hear a fight, a cry or a death close by, or someone
    /// walking right past, and wake.
    bool hearsInSleep(const World& world, float dt);
    /// Woke with fewer cakes than it lay down with: blames whoever it sees
    /// close by, else whoever was near when it fell asleep.
    bool missedWhileAsleep();
    /// Greedy and bold enough, it would sneak a cake off o asleep.
    bool wouldRob(const World& world, const Human& o) const;
    /// Goes to the nearest sleeper it would rob, or kneels by them.
    bool startRob(World& world);
    /// Takes the cake, unless they woke; then they know who.
    void rob(World& world);
    /// Waking in its own time: now and then from a dream of before, which
    /// pulls its hunch of the way out some way, and makes it surer of it.
    void dream();
    /// Someone of its group, or a friend, it would watch over: o asleep in
    /// sight, near, and nobody else already watching them.
    bool wouldWatch(const World& world, const Human& o) const;
    /// Drowsy, with someone of its group or a friend asleep close by: it
    /// lies down near them too, unless keeping watch.
    bool sleepsWithGroup(const World& world) const;
    /// Sat on watch: true when something needs it up: the sleeper awake or
    /// gone, a threat coming, or itself tired or in need.
    bool watchCalls(const World& world) const;
    /// Someone coming at the sleeper it watches: a foe, a suspect, whoever
    /// it fears, or someone sneaking up to rob them; nobody.
    const Human* watchThreat(const World& world, const Human& sleeper) const;
    /// Keeps watch: sits by the sleeper, wakes them if a foe or suspect comes
    /// close, hands over to the most rested once tired, and may rob them
    /// instead if greedy and not fond of them. False once there is no one
    /// to watch, or it has more pressing needs.
    bool keepWatch(World& world);
    /// Tired on watch: wakes the most rested sleeper of the group to take
    /// over, if any is rested enough.
    bool handOverWatch(World& world);
    /// Nervous, about to sleep: asks a friend or one of its group awake
    /// nearby to keep watch; they may want a cake for it. True if it asked.
    bool askToWatch(World& world);
    /// Harmed asleep: thinks the worse of whoever was meant to keep watch.
    void blameWatcher(World& world);
    /// Goes to o asleep, or kneels by them, to take a cake.
    bool goRob(World& world, Human& o);
    /// Where it means to sleep: home if settled, else where it last slept
    /// safely; nothing if it has no such place yet.
    std::optional<Vec2i> sleepBase() const;
    /// Getting drowsy, 0 at DROWSY to 1 at tired, weighed by safeSleep.
    float drowsy() const;
    /// Drowsy and far from where it means to sleep: how much less it wants
    /// to explore.
    float sleepTether() const;
    /// Drowsy and far from where it means to sleep: how much it wants to
    /// go back there; and going.
    float headBack() const;
    bool returnToSleepPlace(World& world);
    /// A stranger it doesn't trust would see it sleep at c, and it minds.
    bool sleepWatched(const World& world, Vec2i c) const;
    /// Tiredness over dt: up by what it is doing, bleeding and dread; down
    /// asleep. How much more than tired it is, 0 to 1.
    void tire(float dt);
    float tiredDrive() const;
    /// Another gulp, or done once quenched or something else comes first.
    void drink(World& world);
    /// How foul it sees the water is, as far as it minds: nothing to the
    /// parched, the erratic or those craving flesh, nor where it can't see.
    float seenFoul(const World& world, Vec2i water) const;
    /// Would drink this water from that cell: not too foul or red, as far
    /// as it can see, no filth round the spot too much for its modesty, and
    /// no body close by, unless parched, craving flesh (the red, the body,
    /// the blood round it) or (for the body) brave.
    bool wouldDrink(const World& world, Vec2i from, Vec2i water) const;
    /// The disgust of the piss, feces and (but for butBlood) blood it knows
    /// of on and round a cell.
    float filthAround(const World& world, Vec2i at, bool butBlood) const;
    /// Washing at the water: whoever sees it and drinks there, or lives by
    /// it, thinks the worse of it.
    void offendDrinkers(World& world, Vec2i water);
    /// Someone it sees has made a home by this water.
    bool othersWater(const World& world, Vec2i water) const;
    bool mayDrinkHere(int who) const;
    /// Someone thirsty came into its place, which has water: lets them
    /// drink (true), maybe for a cake, or turns them away (false, refused,
    /// having told them so). In human_thirst.cpp.
    bool askedToDrink(Human& o, bool& refused);
    void payForWater(World& world);
    /// Water to tell o of at a greeting, if thirsty: the way to the water
    /// nearest it that o has never seen, free to friends and those it walks
    /// with, sold by the grasping, kept back by the stingy; to a foe, a way
    /// over spikes from the cunning, else nothing. In human_thirst.cpp.
    std::optional<Tell> waterFor(const World& world, const Human& o, bool& forPay) const;
    /// The way from here to the nearest water o has never seen, or over a
    /// spike plate o doesn't know (lie) on the way to water.
    std::optional<Tell> waterRoute(const World& world, const Human& o, bool lie) const;
    /// Thirsty and knowing no water it can get to: asks those it walks
    /// with in sight, now and then, and one who knows tells it the way.
    void askGroupForWater(World& world);
    /// Pissing, shitting and mess (human_relief.cpp).
    struct Relief {
        /// Going, for feces as well, or only to piss.
        bool shitting = false;
        float drunk = 0.0f, eaten = 0.0f;
        /// Whom it is looking away from, giving privacy; -1 none.
        int privacyFor = -1;
        /// A fright big enough to wet itself came, to be seen to by tread().
        bool frightWet = false;
        Vec2i messTrod;
        int messSteps = 0;
        bool ignoreMess = false;
        std::unordered_set<int> filthNoted;
    };
    Relief relief_;
    /// Bladder and bowels filling over dt; human_relief.cpp, like the rest
    /// of going.
    void press(float dt);
    /// What goes in comes out: this much water drunk and food eaten, as
    /// shares of a full gulp and a whole cake, held (relief_.drunk, relief_.eaten) and
    /// passed on to the bladder and bowels over the next minutes.
    void takeIn(float water, float food);
    /// Wants to go from here, held on longer by the modest.
    float needBar() const;
    /// Needing to go: walks to the nearest spot that feels out of the way,
    /// where nobody it sees could see it, never far, or goes where it
    /// stands once it can't hold on. False if there is nowhere yet.
    bool seekRelief(World& world);
    /// Goes where it stands, facing a wall if there is one beside it, or
    /// into the toilet it stands before, unless it misses.
    void startRelieving(World& world);
    /// The nearest toilet it knows within reach is in use: waits its turn a
    /// few cells off, if modest enough for how urgent it is. False if it
    /// won't, or needn't.
    bool waitForToilet(World& world, int reach, float urgent);
    /// o and it walk together: one keeps with the other, or both with the
    /// same one.
    bool inGroupWith(const Human& o) const;
    /// Someone it walks with is off to go, or going, in sight: it stands
    /// back and looks away. The erratic don't bother.
    bool givePrivacy(World& world);
    /// Couldn't hold on, or (fright) frightened out of it: goes in its
    /// clothes where it stands, piss and maybe feces on its legs and feet,
    /// ashamed unless erratic. Doesn't stop what it is doing.
    void accident(World& world, bool fright);
    /// Dead: what it held lets go.
    void letGo(World& world);
    /// Everyone else in sight of it going (accident or not; into water or
    /// not) thinks what they think of that; it is ashamed of being seen.
    void beSeenGoing(World& world, bool accident, bool intoWater);
    /// The toilet it knows of that the cell is in front of, if any.
    std::optional<Vec2i> toiletBefore(const World& world, Vec2i c) const;
    void endRelieving();
    /// Someone it sees could see the cell. The erratic never care.
    bool watchedAt(const World& world, Vec2i c) const;
    /// Stepping onto fresh feces dirties the feet, which print it on for
    /// some steps after.
    void tread(World& world);
    /// What the mess it knows of on a cell adds to walking over it: nothing
    /// to the erratic, nor to its own while it looks for a spot to go.
    int messCost(const World& world, Vec2i c) const;
    /// Saw o filthy: thinks a little less of them, once while they stay so.
    void noticeFilth(const Human& o);
    /// Too filthy for it to greet.
    bool repelledBy(const Human& o) const;
    /// Feces on it it would wash off: none of the erratic's business.
    bool wantsWashing() const;
    /// Washes a little more feces off into the water at the cell, fouling
    /// it. False once there is none left.
    bool washDirt(World& world, Vec2i water);
    /// Fouls the water at the cell this much, which seeps on from there.
    static void foulWater(World& world, Vec2i water, float amount);
    /// The nearest human remembered carrying cakes, and not to be feared.
    const Human* nearestHolder(const World& world) const;
    /// With cakes to spare: a starving human in sight to feed, or to beat up
    /// when too weak. False if there is none.
    bool helpStarving(World& world);
    /// Whether this human would give the other a cake.
    bool wouldShare(const Human& other) const;
    /// Walks up to social_.sharee and hands them a cake. False if they're lost.
    bool startShare(World& world);
    /// Why a cake is handed over: to the starving, for a tip, or under threat.
    enum class Gift { Share, Pay, Yield };
    /// Hands other a cake, who thinks better of it for a gift or a payment.
    void give(Human& other, Gift why);
    /// Fighting and running away (human_fight.cpp).
    struct Fight {
        /// Who this human is fighting, and for how long more without being hit.
        int foe = -1;
        float grudge = 0.0f;
        /// Who it ran from, and for how long it keeps away from them.
        int feared = -1;
        float fear = 0.0f;
        /// Hit by attacker and not yet reacted: it does once off its step.
        bool provoked = false;
        int attacker = -1;
        /// Damage of the blows taken lately, draining away with time. Past a
        /// point a beating starts to bleed.
        float beating = 0.0f;
    };
    Fight fight_;
    /// Punches fight_.foe if close, else walks up to them, or to where they were
    /// last seen. False once the fight is off.
    bool startFight(World& world);
    void startPunch(Human& foe);
    void landPunch(World& world);
    /// Called back from takeHit once the human can act: fights or runs.
    void react(World& world);
    /// Runs from fight_.feared. False when there is nowhere to run.
    bool startFlee(World& world);

    /// Holding a key, whether hunger, fright or wanting out sends it to a
    /// locked door now rather than after exploring. Weighed only every so often.
    bool doorUrge();
    /// Falls where it stands, keeping whatever it carried.
    void die(World& world);

    /// Takes the keys underfoot or on tables beside it, and cakes while
    /// there is room, leaving those on plates it knows of.
    void takeInReach(World& world);
    /// One of items, as remembered, it could take standing at cell at.
    std::optional<Vec2i> inReach(Vec2i at, const std::unordered_set<Vec2i, Vec2iHash>& items) const;
    bool knownPassable(Vec2i c) const;
    /// A known trap it won't step on.
    bool avoids(Vec2i c) const;
    /// Known floor next to unknown cells: where exploring leads.
    bool isFrontier(Vec2i c) const;
    /// The locked door cell next to known floor c, if any.
    std::optional<Vec2i> lockedDoorBeside(Vec2i c) const;
    /// An open door next to known floor c with nothing known past it: someone
    /// opened it, or the human gave up going through.
    std::optional<Vec2i> openDoorBeside(Vec2i c) const;
    /// Notes that it is looking at, close up, every locked door near it.
    void checkDoorsNear();
    /// A locked door next to known floor c not looked at close up lately.
    bool doorDue(Vec2i c) const;
    /// Cheapest walk over remembered ground to the nearest cell that is
    /// done, which may be where it stands. Nothing if none is reachable.
    /// With avoidCrowd, cells others are seen standing on are kept out of the way.
    /// With goalCost, each done cell costs that much more to end at, so a
    /// farther one may win over a nearer.
    std::optional<std::vector<Vec2i>> search(const World& world, const std::function<bool(Vec2i)>& done,
                                             bool avoidCrowd = false,
                                             const std::function<int(Vec2i)>& goalCost = {}) const;
    /// The same walk from any cell, which need not be passable itself.
    std::optional<std::vector<Vec2i>> searchFrom(const World& world, Vec2i start,
                                                 const std::function<bool(Vec2i)>& done, bool avoidCrowd,
                                                 const std::function<int(Vec2i)>& goalCost = {}) const;
};
