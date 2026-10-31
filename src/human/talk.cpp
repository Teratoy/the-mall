#include "human/talk.h"
#include "core/files.h"
#include "human/human.h"
#include "world/statue.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace talk {
namespace {

/// The sections' names in the file, in the order of Moment.
constexpr const char* MOMENTS[] = {
    "meet", "catch_up", "introduce", "tell_cake", "tell_key", "tell_trap", "tell_door", "sell_trap", "lie", "warn",
    "praise", "thank_tip", "refuse", "snubbed", "beg", "give", "thanks", "pay", "threat", "yield", "defy", "hit", "ouch", "flee", "killed",
    "darted", "spiked", "slammed",
    "help", "help_coming", "freed",
    "found_body", "loot", "shame", "pool", "own_pool", "escort", "escorted", "accuse",
    "learn_cakes", "learn_keys",
    "snap", "hunt", "feed", "sick", "saw_eat", "gnawed", "haunted", "craving",
    "wake",
    "first_key", "first_cake", "first_door", "first_water", "first_human", "first_prints", "first_toilet",
    "despair", "hope", "settle", "guard",
    "ask_way", "tell_way", "no_way_out", "lie_way", "no_idea",
    "join", "join_reply", "follow",
    "part_lost", "part_turned", "part_filthy", "part_no_keys", "part_own_key",
    "chat_here", "chat_shared", "chat_story", "chat_someone", "chat_feeling", "chat_way", "chat_group", "chat_before",
    "reply_agree", "reply_add", "reply_disagree", "reply_unheard", "reply_brush", "chat_follow", "chat_close",
    "parched", "foul_water", "need_to_go", "accident", "stepped_in", "seen_going", "into_water", "mock", "privacy",
    "toilet_wait", "bloody_water", "fouled", "my_water", "water_free", "water_price", "water_cheat",
    "tell_water", "sell_water", "lie_water", "ask_water", "lead_drink", "led_drink", "thirsty", "drink", "learn_water", "toilet_drink", "toilet_mock",
    "tired", "exhausted", "sleep", "woke", "dream", "robbed_sleep", "cake_gone", "watch_me", "watch_yes", "watch_cake",
    "watch_no", "wake_up", "your_watch", "failed_watch", "wake_them",
    "spilled", "blame",
    "tell_shut_in", "call_back", "heard_back", "key_for_shut_in", "lost_shut_in",
    "sip", "bottle_dry", "fill_bottle", "pour_out", "ask_drink", "give_drink", "sell_drink", "bottle_lie",
    "caught_bottle_lie", "refuse_drink", "bottle_taken", "put_down", "pick_up", "stash", "from_stash", "saw_stash", "stash_robbed", "steal_stash", "caught_stealing", "resent",
    "tell_stash", "sell_stash", "lie_stash", "stash_told", "ask_stash", "own_stash", "deny_stash", "caught_denying",
    "got_dirty", "washed_clean", "stink", "smell_death", "follow_stench", "put_off",
};
static_assert(std::size(MOMENTS) == size_t(Moment::Count));

/// The tones' words, in the order of Tone; plain needs none.
constexpr const char* TONES[] = {"plain", "warm", "rude", "plead", "fear", "threat"};

/// What a line may ask of the speaker, and of the one it talks to.
enum class Cond {
    Hungry, Starving, Scared, Hurt, Bleeding, Cakes, Keys,
    Brave, Timid, Greedy, Generous, Curious, Homebody, Sociable, Loner,
    Erratic, Guilty, Craving, Dazed, Hopeless, Settled, Sure,
    Thirsty, Parched, NeedToGo, Bursting, Filthy, Tired, Exhausted, Dreamt,
    Friend, Foe, Liked, Disliked, Suspect, Afraid,
    TheyHungry, TheyStarving, TheyScared, TheyHurt, TheyBleeding, TheyCakes, TheyKeys, TheyDazed,
    TheyThirsty, TheyParched, TheyFilthy, TheyAsleep,
    // How far gone the body of the one spoken of is.
    BodyFresh, BodyRotting, BodyBones,
    // About the topic brought up in a chat; never true otherwise.
    HereStatue, HereWater, HereBasin, HereBody, HereBlood, HereMess, Foul, Feces,
    EpFight, EpKill, EpBody, EpFlesh, EpGnawed, EpTrap, EpSpotted, EpGift, EpDoor, EpPlace, EpPool, EpEscort,
    EpSnapped, EpFriend, EpFoe, EpLied, EpStarving, EpParched, EpLoot, EpFoul, EpDespair, EpSettled, EpFreed,
    ByMe, ToMe, ByYou, ToYou, Dark, Strong, Recent,
    Spikes, Darts, Slam, GiftShare, GiftPay, GiftYield,
    AboutDead, AboutLiked, AboutDisliked, AboutFriend, AboutFoe, TheyKnow,
    FeelPeckish, FeelDry, FeelFootsore, FeelUneasy, FeelGloomy, FeelHopeful,
    ILead, YouLead,
    // What the topic is (for answers and follow-ups), and how it was answered.
    IsHere, IsShared, IsStory, IsSomeone, IsFeeling, IsWay, IsGroup, IsBefore,
    Agreed, Added, Disagreed, Unheard,
    StatueMug, StatueDuck, StatueChair, StatueTeapot, StatueToilet, StatueToaster, StatueLamp, StatueClock,
    StatueSpoon, StatueBulb,
    Count
};
constexpr const char* CONDS[] = {
    "hungry", "starving", "scared", "hurt", "bleeding", "cakes", "keys",
    "brave", "timid", "greedy", "generous", "curious", "homebody", "sociable", "loner",
    "erratic", "guilty", "craving", "dazed", "hopeless", "settled", "sure",
    "thirsty", "parched", "need.to.go", "bursting", "filthy", "tired", "exhausted", "dreamt",
    "friend", "foe", "liked", "disliked", "suspect", "afraid",
    "they.hungry", "they.starving", "they.scared", "they.hurt", "they.bleeding", "they.cakes", "they.keys",
    "they.dazed", "they.thirsty", "they.parched", "they.filthy", "they.asleep",
    "body.fresh", "body.rotting", "body.bones",
    "here.statue", "here.water", "here.basin", "here.body", "here.blood", "here.mess", "foul", "feces",
    "ep.fight", "ep.kill", "ep.body", "ep.flesh", "ep.gnawed", "ep.trap", "ep.spotted", "ep.gift", "ep.door",
    "ep.place", "ep.pool", "ep.escort", "ep.snapped", "ep.friend", "ep.foe", "ep.lied", "ep.starving", "ep.parched",
    "ep.loot", "ep.foul", "ep.despair", "ep.settled", "ep.freed",
    "by.me", "to.me", "by.you", "to.you", "dark", "strong", "recent",
    "spikes", "darts", "slam", "gift.share", "gift.pay", "gift.yield",
    "about.dead", "about.liked", "about.disliked", "about.friend", "about.foe", "they.know",
    "feel.peckish", "feel.dry", "feel.footsore", "feel.uneasy", "feel.gloomy", "feel.hopeful",
    "i.lead", "you.lead",
    "is.here", "is.shared", "is.story", "is.someone", "is.feeling", "is.way", "is.group", "is.before",
    "agreed", "added", "disagreed", "unheard",
    "statue.mug", "statue.duck", "statue.chair", "statue.teapot", "statue.toilet", "statue.toaster", "statue.lamp",
    "statue.clock", "statue.spoon", "statue.bulb",
};
static_assert(std::size(CONDS) == size_t(Cond::Count));

/// A trait counts from here up, and its opposite from TRAIT_LOW down:
/// about a third of humans either side.
constexpr float TRAIT_HIGH = 0.6f, TRAIT_LOW = 0.4f;
/// Frightened from this much fright, hurt below this much health, and
/// liked or disliked from this much trust either way.
constexpr float SCARED = 0.35f;
constexpr float HURT = 0.5f;
constexpr float LIKED = 0.15f;
/// Guilty, or craving flesh, from this much of either.
constexpr float GUILTY = 0.2f;
constexpr float CRAVES = 0.3f;
/// Still dazed from waking up while this much of the daze is left.
constexpr float DAZED = 0.3f;
/// Sure of its hunch of the way out from this much.
constexpr float SURE = 0.5f;
/// About to burst from this full; filthy from this much on it.
constexpr float BURSTING = 0.9f;
constexpr float FILTHY = 0.15f;
/// An episode struck hard from this much, and is recent this many seconds on.
constexpr float STRONG = 0.8f;
constexpr float RECENT = 60.0f;
/// A chat line said this many seconds ago is passed over for another that
/// fits, if there is one.
constexpr float CHAT_REPEAT = 300.0f;

/// The words for a statue, as {THING}: its material, then what it is.
constexpr const char* STATUE_WORDS[] = {"MUG",     "RUBBER DUCK", "CHAIR",       "TEAPOT", "TOILET",
                                        "TOASTER", "DESK LAMP",   "ALARM CLOCK", "SPOON",  "LIGHT BULB"};
constexpr const char* MATERIAL_WORDS[] = {"MARBLE", "BRONZE", "STONE", "PAINTED"};
static_assert(std::size(STATUE_WORDS) == size_t(StatueKind::Count));
static_assert(std::size(MATERIAL_WORDS) == size_t(StatueMaterial::Count));

struct Line {
    std::string text;
    Tone tone = Tone::Plain;
    /// Each condition, and whether it must not hold instead.
    std::vector<std::pair<Cond, bool>> conds;
};
struct Section {
    float chance = 1.0f;
    std::vector<Line> lines;
};
using Book = std::array<Section, size_t(Moment::Count)>;

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

/// Where the file may be: named by THEMALL_LINES, beside where the game
/// was started, or beside the game itself.
std::vector<std::filesystem::path> places() {
    std::vector<std::filesystem::path> p;
    if (const char* env = std::getenv("THEMALL_LINES")) p.emplace_back(env);
    p.emplace_back("data/lines.txt");
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        p.push_back(exe.parent_path() / "../data/lines.txt");
        p.push_back(exe.parent_path() / "data/lines.txt");
    }
    return p;
}

// Anything it can't make out is reported and left out, the rest kept.
Book read(std::istream& in, const std::string& file) {
    Book book;
    Section* section = nullptr;
    std::string row;
    for (int n = 1; std::getline(in, row); n++) {
        auto complain = [&](const std::string& what) {
            std::fprintf(stderr, "%s:%d: %s\n", file.c_str(), n, what.c_str());
        };
        if (size_t hash = row.find('#'); hash != std::string::npos) row.erase(hash);
        row = trim(row);
        if (row.empty()) continue;
        if (row.front() == '[') {
            std::istringstream head(row.substr(1, row.find(']') - 1));
            std::string name;
            head >> name;
            section = nullptr;
            for (size_t i = 0; i < std::size(MOMENTS); i++)
                if (name == MOMENTS[i]) section = &book[i];
            if (!section) {
                complain("no moment called '" + name + "'");
                continue;
            }
            float chance;
            if (head >> chance) section->chance = chance;
            continue;
        }
        if (!section) continue;
        Line line;
        std::string words, text = row;
        if (size_t bar = row.find('|'); bar != std::string::npos) {
            words = row.substr(0, bar);
            text = row.substr(bar + 1);
        }
        bool ok = true;
        std::istringstream ws(words);
        for (std::string w; ws >> w;) {
            bool tone = false;
            for (size_t i = 0; i < std::size(TONES); i++)
                if (w == TONES[i]) line.tone = Tone(i), tone = true;
            if (tone) continue;
            bool negate = w.front() == '!';
            if (negate) w.erase(0, 1);
            auto it = std::find(std::begin(CONDS), std::end(CONDS), w);
            if (it == std::end(CONDS)) {
                complain("no condition or tone called '" + w + "'");
                ok = false;
                continue;
            }
            line.conds.push_back({Cond(it - std::begin(CONDS)), negate});
        }
        // Capitals throughout, as the pixel font is drawn; @ and # are the log's.
        for (char c : trim(text))
            if (c != '@' && c != '#') line.text += char(std::toupper(static_cast<unsigned char>(c)));
        for (size_t at = line.text.find('{'); ok && at != std::string::npos; at = line.text.find('{', at + 1)) {
            std::string slot = line.text.substr(at, line.text.find('}', at) - at + 1);
            if (slot != "{NAME}" && slot != "{ME}" && slot != "{WHO}" && slot != "{THING}" && slot != "{N}") {
                complain("no name called " + slot + ": use {NAME}, {ME}, {WHO}, {THING} or {N}");
                ok = false;
            }
        }
        if (ok && !line.text.empty()) section->lines.push_back(std::move(line));
    }
    return book;
}

Book& book() {
    static Book b;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        reload();
    }
    return b;
}

// Conditions about the topic being brought up.
bool holdsTopic(Cond c, const Human& s, const Human* to, const Topic& t, float now) {
    using T = Topic::Thing;
    using W = Episode::What;
    using F = Topic::Feel;
    bool here = t.kind == Topic::Kind::Here;
    bool ep = t.kind == Topic::Kind::Shared || t.kind == Topic::Kind::Story;
    const Episode& e = t.episode;
    auto what = [&](W w) { return ep && e.what == w; };
    auto feel = [&](F f) { return t.kind == Topic::Kind::Feeling && t.feel == f; };
    int toId = to ? to->id : -2;
    switch (c) {
    case Cond::HereStatue: return here && t.thing == T::Statue;
    case Cond::HereWater: return here && t.thing == T::Water;
    case Cond::HereBasin: return here && t.thing == T::Basin;
    case Cond::HereBody: return here && t.thing == T::Body;
    case Cond::HereBlood: return here && t.thing == T::Blood;
    case Cond::HereMess: return here && t.thing == T::Mess;
    case Cond::Foul: return here && (t.thing == T::Water || t.thing == T::Basin) && t.detail == 1;
    case Cond::Feces: return here && t.thing == T::Mess && t.detail == 1;
    case Cond::EpFight: return what(W::Fight);
    case Cond::EpKill: return what(W::Kill);
    case Cond::EpBody: return what(W::Body);
    case Cond::EpFlesh: return what(W::Flesh);
    case Cond::EpGnawed: return what(W::Gnawed);
    case Cond::EpTrap: return what(W::Trap);
    case Cond::EpSpotted: return what(W::Spotted);
    case Cond::EpGift: return what(W::Gift);
    case Cond::EpDoor: return what(W::Door);
    case Cond::EpPlace: return what(W::NewPlace);
    case Cond::EpPool: return what(W::Pool);
    case Cond::EpEscort: return what(W::Escort);
    case Cond::EpSnapped: return what(W::Snapped);
    case Cond::EpFriend: return what(W::Friend);
    case Cond::EpFoe: return what(W::Foe);
    case Cond::EpLied: return what(W::Lied);
    case Cond::EpStarving: return what(W::Starving);
    case Cond::EpParched: return what(W::Parched);
    case Cond::EpLoot: return what(W::Loot);
    case Cond::EpFoul: return what(W::FoulWater);
    case Cond::EpDespair: return what(W::Despair);
    case Cond::EpSettled: return what(W::Settled);
    case Cond::EpFreed: return what(W::Freed);
    case Cond::ByMe: return ep && e.who == s.id;
    case Cond::ToMe: return ep && e.whom == s.id;
    case Cond::ByYou: return ep && e.who == toId;
    case Cond::ToYou: return ep && e.whom == toId;
    case Cond::Dark: return ep && (e.what == W::Snapped || e.what == W::Despair) && e.detail == 1;
    case Cond::Strong: return ep && e.strength >= STRONG;
    case Cond::Recent: return ep && now - e.when <= RECENT;
    case Cond::Spikes: return (what(W::Trap) || what(W::Spotted)) && e.detail == int(TrapKind::Spikes);
    case Cond::Darts: return (what(W::Trap) || what(W::Spotted)) && e.detail == int(TrapKind::Darts);
    case Cond::Slam: return (what(W::Trap) || what(W::Spotted)) && e.detail == int(TrapKind::Slam);
    // As Human::Gift: shared, paid, handed over under threat.
    case Cond::GiftShare: return what(W::Gift) && e.detail == 0;
    case Cond::GiftPay: return what(W::Gift) && e.detail == 1;
    case Cond::GiftYield: return what(W::Gift) && e.detail == 2;
    case Cond::AboutDead: return t.who >= 0 && s.knowsDead(t.who);
    case Cond::AboutLiked: return t.kind == Topic::Kind::Someone && t.opinion > 0.0f;
    case Cond::AboutDisliked: return t.kind == Topic::Kind::Someone && t.opinion < 0.0f;
    case Cond::AboutFriend: return t.who >= 0 && s.isFriend(t.who);
    case Cond::AboutFoe: return t.who >= 0 && s.isFoe(t.who);
    case Cond::TheyKnow: return t.who >= 0 && to && to->met.count(t.who);
    case Cond::FeelPeckish: return feel(F::Peckish);
    case Cond::FeelDry: return feel(F::Dry);
    case Cond::FeelFootsore: return feel(F::Footsore);
    case Cond::FeelUneasy: return feel(F::Uneasy);
    case Cond::FeelGloomy: return feel(F::Gloomy);
    case Cond::FeelHopeful: return feel(F::Hopeful);
    case Cond::ILead: return t.kind == Topic::Kind::Group && t.who == s.id;
    case Cond::YouLead: return t.kind == Topic::Kind::Group && t.who == toId;
    default: break;
    }
    if (c >= Cond::IsHere && c <= Cond::IsBefore) return int(t.kind) == int(c) - int(Cond::IsHere);
    if (c >= Cond::Agreed && c <= Cond::Unheard)
        return int(t.reply) == int(Topic::Reply::Agree) + int(c) - int(Cond::Agreed);
    if (c >= Cond::StatueMug && c <= Cond::StatueBulb)
        return here && t.thing == T::Statue && t.detail / 16 == int(c) - int(Cond::StatueMug);
    return false;
}

bool holds(Cond c, const Human& s, const Human* to, const Human* about, const Topic* topic, float now) {
    if (c >= Cond::HereStatue) return topic && holdsTopic(c, s, to, *topic, now);
    const Traits& t = s.traits;
    switch (c) {
    case Cond::Hungry: return s.hungry();
    case Cond::Starving: return s.starving();
    case Cond::Scared: return s.fright() >= SCARED;
    case Cond::Hurt: return s.health < HURT;
    case Cond::Bleeding: return !s.wounds.empty();
    case Cond::Cakes: return s.cakes > 0;
    case Cond::Keys: return s.keys > 0;
    case Cond::Brave: return t.brave >= TRAIT_HIGH;
    case Cond::Timid: return t.brave <= TRAIT_LOW;
    case Cond::Greedy: return t.greedy >= TRAIT_HIGH;
    case Cond::Generous: return t.greedy <= TRAIT_LOW;
    case Cond::Curious: return t.curious >= TRAIT_HIGH;
    case Cond::Homebody: return t.curious <= TRAIT_LOW;
    case Cond::Sociable: return t.sociable >= TRAIT_HIGH;
    case Cond::Loner: return t.sociable <= TRAIT_LOW;
    case Cond::Erratic: return s.isErratic();
    case Cond::Guilty: return s.guilt() >= GUILTY;
    case Cond::Craving: return s.appetite() >= CRAVES;
    case Cond::Dazed: return s.dazed() >= DAZED;
    case Cond::Hopeless: return s.gloom() > 0.0f;
    case Cond::Settled: return s.settled();
    case Cond::Sure: return s.hunchSure() >= SURE;
    case Cond::Thirsty: return s.thirsty();
    case Cond::Parched: return s.parched();
    case Cond::Tired: return s.tired();
    case Cond::Exhausted: return s.exhausted();
    case Cond::Dreamt: return s.dreamt();
    case Cond::NeedToGo: return s.needsToGo();
    case Cond::Bursting: return std::max(s.bladder, s.bowels) >= BURSTING;
    case Cond::Filthy: return s.filth() >= FILTHY;
    default: break;
    }
    // About the one spoken of, if dead: never true of the living or nobody.
    if (c >= Cond::BodyFresh && c <= Cond::BodyBones) {
        if (!about || !about->dead()) return false;
        if (c == Cond::BodyBones) return about->bones();
        return about->spoiled() == (c == Cond::BodyRotting) && !about->bones();
    }
    // The rest are about the one spoken to: never true of nobody.
    if (!to) return false;
    switch (c) {
    case Cond::Friend: return s.isFriend(to->id);
    // The cunning keep a straight face with a foe (Human::masks).
    case Cond::Foe: return s.isFoe(to->id) && !s.masks(to->id);
    case Cond::Liked: return s.trust(to->id) >= LIKED;
    case Cond::Disliked: return s.trust(to->id) <= -LIKED && !s.masks(to->id);
    case Cond::Suspect: return s.suspects(to->id);
    case Cond::Afraid: return s.afraidOf(to->id);
    case Cond::TheyHungry: return to->hungry();
    case Cond::TheyStarving: return to->starving();
    case Cond::TheyScared: return to->fright() >= SCARED;
    case Cond::TheyHurt: return to->health < HURT;
    case Cond::TheyBleeding: return !to->wounds.empty();
    case Cond::TheyCakes: return to->cakes > 0;
    case Cond::TheyKeys: return to->keys > 0;
    case Cond::TheyDazed: return to->dazed() >= DAZED;
    case Cond::TheyThirsty: return to->thirsty();
    case Cond::TheyParched: return to->parched();
    case Cond::TheyAsleep: return to->state == Human::State::Sleeping;
    case Cond::TheyFilthy: return to->filth() >= FILTHY;
    default: return false;
    }
}

std::string caps(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void fill(std::string& text, const char* slot, const std::string& name, bool& said) {
    for (size_t at = text.find(slot); at != std::string::npos; at = text.find(slot, at + name.size())) {
        text.replace(at, std::char_traits<char>::length(slot), name);
        said = true;
    }
}

} // namespace

bool reload() {
    if (auto in = files::asset("data/lines.txt")) {
        book() = read(*in, "data/lines.txt");
        return true;
    }
    for (const auto& path : places()) {
        std::ifstream in(path);
        if (!in) continue;
        book() = read(in, path.string());
        return true;
    }
    std::fprintf(stderr, "no data/lines.txt found: humans will say nothing\n");
    book() = {};
    return false;
}

std::optional<Said> pick(Moment m, const Human& speaker, const Human* to, const Human* about, Rng& rng,
                         const Topic* topic, std::unordered_map<std::string, float>* used, float now) {
    const Section& s = book()[size_t(m)];
    // Bringing up a topic, nothing is left to chance (see the header).
    bool fixed = topic != nullptr;
    if (s.lines.empty() || (!fixed && !rng.chance(s.chance))) return std::nullopt;
    const std::string* toName = to ? speaker.knownName(to->id) : nullptr;
    // Bones it never knew for whose go unnamed.
    const std::string* aboutName =
        about && speaker.knowsWhose(about->id) ? speaker.knownName(about->id) : nullptr;
    std::vector<float> weights;
    float total = 0.0f;
    for (const Line& line : s.lines) {
        bool statue = topic && topic->kind == Topic::Kind::Here && topic->thing == Topic::Thing::Statue;
        bool group = topic && topic->kind == Topic::Kind::Group;
        bool fits = (toName || line.text.find("{NAME}") == std::string::npos) &&
                    (statue || line.text.find("{THING}") == std::string::npos) &&
                    (group || line.text.find("{N}") == std::string::npos) &&
                    (aboutName || line.text.find("{WHO}") == std::string::npos);
        for (auto [c, negate] : line.conds) fits = fits && holds(c, speaker, to, about, topic, now) != negate;
        weights.push_back(fits ? 1.0f + float(line.conds.size()) : 0.0f);
        total += weights.back();
    }
    if (total <= 0.0f) return std::nullopt;
    size_t chosen = 0;
    if (fixed) {
        // Not one it said just now, if it has another; then the most
        // conditions; then the one said longest ago, or never.
        auto saidAt = [&](size_t i) {
            if (!used) return -1e9f;
            auto it = used->find(s.lines[i].text);
            return it == used->end() ? -1e9f : it->second;
        };
        auto stale = [&](size_t i) { return now - saidAt(i) < CHAT_REPEAT; };
        // Last of all, each human's own way of putting things: a liking for
        // some wordings over others, fixed for it.
        auto taste = [&](size_t i) {
            uint64_t z = std::hash<std::string>{}(s.lines[i].text) + uint64_t(speaker.id) * 0x9e3779b97f4a7c15ull;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            return z ^ (z >> 31);
        };
        bool any = false;
        for (size_t i = 0; i < s.lines.size(); i++) {
            if (weights[i] <= 0.0f) continue;
            bool better =
                !any || stale(chosen) > stale(i) ||
                (stale(chosen) == stale(i) &&
                 (weights[i] > weights[chosen] ||
                  (weights[i] == weights[chosen] &&
                   (saidAt(i) < saidAt(chosen) || (saidAt(i) == saidAt(chosen) && taste(i) < taste(chosen))))));
            if (better) chosen = i;
            any = true;
        }
        if (used) (*used)[s.lines[chosen].text] = now;
    } else {
        chosen = size_t(rng.weighted(weights));
    }
    const Line& line = s.lines[chosen];
    Said said{line.text, line.tone};
    if (topic && topic->kind == Topic::Kind::Here && topic->thing == Topic::Thing::Statue) {
        bool unused = false;
        fill(said.text, "{THING}",
             std::string(MATERIAL_WORDS[topic->detail % 16]) + " " + STATUE_WORDS[topic->detail / 16], unused);
    }
    if (topic) {
        bool unused = false;
        fill(said.text, "{N}", std::to_string(topic->count), unused);
    }
    if (toName) fill(said.text, "{NAME}", caps(*toName), said.namesThem);
    if (aboutName) fill(said.text, "{WHO}", caps(*aboutName), said.namesWho);
    fill(said.text, "{ME}", caps(speaker.name), said.namesMe);
    return said;
}

} // namespace talk
