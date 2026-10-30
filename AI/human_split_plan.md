# Splitting Human into systems

After the 2026-10-30 refactor, Human's state is grouped per system
(`sleep_`, `water_`, `relief_`, ...) and human.cpp is split by theme. But
it is still one class: about 250 methods in a 1,287-line header, and any
method in any file can read or change any system's state. The compiler
doesn't stop sleep code from changing thirst state, and the header has
to be read whole to find anything.

The goal: each system is a class of its own that holds its state and its
methods. Human keeps the body, the needs, the main loop and a small set
of **services** that every system uses. A system can reach another only
through that system's public methods.

## What the code says (measured 2026-10-30)

Calls from each file into methods defined in other files:

| file | calls out | mostly into |
|---|---|---|
| human (loop, choose) | 174 | walk 36, speech 20, relief 18, sleep 18 |
| fight | 120 | speech 37, human 33, episode 14 |
| sleep | 105 | speech 33, social 21, human 17, walk 15, relief 13 |
| thirst | 97 | speech 31, human 22, social 13, walk 10 |
| social | 68 | speech 31, walk 17 |
| blood | 59 | human 18, walk 16, speech 11 |
| walk | 52 | human 19, speech 7 |
| relief | 50 | flesh 18, speech 18 |
| flesh | 48 | speech 17, human 10 |
| speech, company, chat, rescue, hope, topic | 22–32 each | social, speech, walk |

Two things stand out:

1. **Everyone calls speech, walk and social.** `sayLater`/`note`/
   `remember`, `search`/`startWalking`/`startIdle`, and `trust`/`regard`/
   `isFriend` are services, not systems. They stay on Human and become its
   public interface for systems.
2. **The real systems are leaves.** Rescue, relief, flesh, hope, thirst,
   blood, sleep and conversation mostly call the services and rarely each
   other. Most of the cross-system calls are a few queries, such as
   `isErratic()`, `settled()`, `watching()` and `escorting`. Those become
   small public methods.

## The pattern

Each system becomes a class with its state private, taking the human it
belongs to as an argument:

```cpp
// human/sleep.h
class Sleep {
public:
    // Read by others (today: inline accessors on Human).
    bool robbing() const { return robbee_ >= 0; }
    int watching() const { return watchee_; }
    // Hooks Human calls from update/live/choose/pursue.
    void tire(Human& h, float dt);
    void tick(Human& h, World& w, float dt);     // the Sleeping state
    bool reflex(Human& h, World& w);             // keepWatch in choose()
    float want(const Human& h, const World& w) const;
    bool pursue(Human& h, World& w);
private:
    int robbee_ = -1, watchee_ = -1, watchedBy_ = -1;
    // ... the rest of today's Human::Sleep struct, and its private helpers
};
```

- Human owns one of each (`Sleep sleep;`) and gives read access through
  `const Sleep& sleep() const`. Writes from outside go through methods,
  e.g. `o.sleep().wakeUp(o, world, ...)`.
- System code reaches its own human as `h.` and other humans as `o.`:
  `h.note(...)`, `h.sayLater(...)`, `h.search(...)`, `h.memory`, `h.rng()`.
  That makes system code a little longer. In exchange, every reach outside
  the system is visible where it happens.
- **Constructors and the RNG.** Systems keep no Rng of their own and use
  `h.rng()`, so every die roll stays in the same order.
- **Tuning stays with its system:** each system's constants move into its
  .cpp, as most already are.
- **The choose() order stays written out.** choose() keeps calling the
  systems' hooks one by one, in today's order. It does not loop over a
  list of systems. The order of reflexes and wants decides behaviour (and
  the order of dice rolls), so it should stay visible in one place. The
  WANTING table and the Want enum stay in human.cpp.

### The services Human keeps (and makes public)

- **Speech:** sayLater, note, remember, rememberAt, quiet, talking
- **Walking:** search, searchFrom, startWalking, startIdle, rest,
  inReach, avoids, lockedDoorBeside, hunchCost, shunZone
- **Social:** trust, regard, isFriend, isFoe, knownName, inGroupWith,
  counterpart
- **Mood:** scare, lift, fright, gloom, learn
- **Body:** wound, takeHit, die, startEating
- Public fields that already exist: cell, state, goal, hunger, thirst,
  health, cakes, keys, memory, sight, traits, log

A first job of step 0 is to go through these and drop the ones no system
needs.

## Build order

Each step is one commit, and for each step:
- `build/stuck_soak 6 15 1 6` output is **byte-identical** to before (the
  sim is deterministic, so any slip shows as a diff);
- the build has zero warnings;
- no other session edits human files during the step (it touches all of
  them).

0. **Services.** Move the service methods into a public "for systems"
   section of human.h with their docs, and add `rng()`. No behaviour
   change. Small.
1. **Rescue (pilot).** 154 lines, 5 fields, 6 methods, and it touches
   other systems in only 3 places (walk opening a gate, world.cpp calling
   shutIn, choose/pursue). This step proves the pattern: if it reads
   badly here, we stop and rethink before going further.
2. **Hope / escape** (168 lines): out-urge, hunch, hope, settling, home.
   Many systems read `settled()` and `home()`, so this step gives them
   their accessors.
3. **Flesh / mind** (372 lines): erratic, guilt, appetite, hunting,
   feeding. Relief calls it 18 times (`isErratic`), so it comes before
   relief.
4. **Relief** (534 lines).
5. **Blood + escort** (634 lines): gore, suspects, tracking, washing,
   escorting. Thirst shares escorting, so escorting moves here first.
6. **Thirst** (506 lines): water, guarded water, water tips.
7. **Sleep** (692 lines): the largest. It leans on social (21 calls) for
   groups and watch.
8. **Conversation** (chat + topic + episode, about 575 lines): urges,
   topics and replies. Episodes are a service that several systems
   record, so `remember` stays on Human and the episode store becomes
   Conversation's.

After step 8, Human would hold the loop, needs, choose/pursue, and the
walk, social, fight and speech services: about half of today's header.

**Later, only if it's worth it:** turn walk, speech and social into
components too (a `Walker`, a `Voice`). They are used everywhere, so it
costs more and gains less. Decide after step 8.

## Risks

- **Clashes with other sessions.** Every step edits most human files.
  Run steps when nothing else is editing Human, and commit each step
  straight away.
- **Behaviour slipping.** This is covered by the byte-identical soak. A
  step that can't stay identical gets stopped and looked at, not tuned
  to fit.
- **Probes in other sessions' scratch folders** (shot.cpp, talk_pace.cpp,
  probe.cpp) read Human internals and will stop compiling. They are
  throwaway, but any you still use need updating.
- **Effort.** Steps 1–2 are about one sitting together. Steps 5–7 are
  one sitting each.

## Open questions

1. **Pattern:** system classes taking `Human&` (this plan), or stop at
   today's grouped structs and only move docs into per-system headers?
2. **Pilot gate:** look at the rescue pilot (step 1) yourself before
   steps 2–8 go ahead, or go straight through as long as each soak stays
   identical?
3. **Walk/speech/social as components later:** decide after step 8, or
   rule it out now?
