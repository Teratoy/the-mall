# Inspecting discoveries plan

Today a human remarks on the first door, key, toilet or machine it ever
sees while walking straight past it: the bubble comes up and its feet don't
stop. The user wants a discovery seen while exploring or wandering to pull
it over: it walks up close, stands and looks, says its line there, then goes
on with what it was doing.

## Every first-sight comment today

Said once per life, "one a look" (at most one of these per `observe()`), and
never while still lying in the waking sit.

| Moment | Seen | Where | Log line |
|---|---|---|---|
| `FirstKey` | a key on a table or the floor | `observe()`, human.cpp | none |
| `FirstCake` | a cake | `observe()` | none |
| `FirstDoor` | a door, locked or open | `observe()` | none |
| `FirstWater` | drinkable water | `observe()` | none |
| `FirstHuman` | another living human | `observe()` | none (meeting has its own) |
| `FirstPrints` | someone else's shitty footprints | `observe()` | none |
| `FirstToilet` | a toilet | `observe()` | none |
| `FirstVending` | a vending machine | `seeMachine()`, human_vending.cpp | FOUND A MACHINE THAT TAKES KEYS |
| `FirstServerRoom` | a rack, so a server room | `seeServerRoom()`, human_server.cpp | FOUND A ROOM OF HUMMING MACHINES |

Said only while still dazed, alone and quiet (`fogSight`), so the human is
near-still already:

| Moment | Seen |
|---|---|
| `FirstStatue` | a giant statue (the line can name it) |
| `FirstTable` | a table |
| `EmptyRoom`, `Blank` | nobody about; nothing remembered (not things in sight) |

First sight of each one, not once per life, and already a scare or a
reaction of their own:

| Moment | Seen | Where |
|---|---|---|
| `FoundBody` | a body, the first time it sees that one | `observe()` |
| `Gnawed` | a half-eaten body | `seeRemains()`, human_flesh.cpp |
| `Pool` / `OwnPool` / `Spilled` | a fresh pool of blood | `noticeBlood()`, human_blood.cpp |
| `SeeBottle` | a bottle lying, while it has none | `spotBottle()` |
| (no line) | a hidden trap spotted beside it: SPOTTED HIDDEN ... | `move()`, human_walk.cpp |

## Proposal

- A first sight worth a look (see the questions) no longer says its line at
  once. It is kept as "a discovery": the moment, the cell, who it is about.
- If the human is walking on `Goal::Explore` or `Goal::Wander` (or standing
  idle between them) and nothing presses, `choose()` takes a new reflex,
  just after the pressing ones: walk to a free cell next to the discovery
  (`Goal::Inspect`), turn to face it, stand still for a while
  (`State::Idle`, so `Action::Rest`, no new Action and no new rows), and say
  the line on arriving. Then `choose()` as usual.
- "Back to its path" is a fresh `choose()`, not the old path replayed:
  Explore already re-picks the nearest frontier every step it sees
  something new, and the old target may be stale. This can't loop, because
  each discovery is inspected at most once, whether or not it got there.
- Given up when: the cell can't be reached, it's farther than a limit, an
  urgent need or a fight comes up on the way, or it takes too long. Given
  up, the line is said where it stands, as today, so no comment is lost.
- Log line on arriving: `STOPPED TO LOOK AT A <THING>` (key, cake, door,
  water, toilet, machine, room of humming machines, footprints).
- No new talk moment needed: the existing first-sight lines are said there.
- Scenario `inspect`: one curious human, a key on a table across the spawn
  room, nothing pressing; check its log has `STOPPED TO LOOK AT A KEY`
  before any `TOOK A KEY`, and that it is idle beside the table when it
  says the line.

## Open questions

1. Which discoveries are worth a detour?
2. How close does it walk, and how long does it stand?
3. What skips it (urgent needs, fear, a fight, running, escorting, leading
   or following company)?
4. Dangerous things: are traps, bodies and blood inspected, from a distance,
   or not at all?
5. Several at once: queue them, or only the nearest?
6. A companion: does it wait?
7. Does the human pick up a key or cake it walked up to inspect?

## User's decisions (2026-10-31)

1. Worth a detour: first key, cake, bottle lying (`SeeBottle`), door, water,
   toilet, vending machine, server room, someone else's footprints, the
   first statue, and the first corpse. Not the table (stays a dazed
   remark) nor the first human (meeting covers it).
2. Walks to a cell beside it, by curiosity: curious humans go farther for it
   and stand longer (up to about 4 s), incurious ones glance.
3. Only from exploring or wandering, and not when hungry, thirsty, tired,
   needing to go, scared, fighting, running, escorting or escorted. With
   company it inspects and they carry on; it catches up.
4. Grim and dangerous first sights (the first body, the first fresh pool of
   blood, the first trap spotted): brave or curious humans walk up beside
   it (never onto a trap or into a pool's shunned ring); others stop where
   they are, face it and stand a moment. Their scare and today's reaction
   are unchanged.
5. Several at once queue, nearest first, one stop each; the queue is left
   the moment something presses.
6. A key or cake looked at is not picked up because of it: taking stays up
   to the usual wants.
7. The first statue ever seen, dazed or not, is inspected and remarked on
   once per life.

## What was built

- `src/human/human_inspect.cpp`, state in `Human::inspect_` (a queue of
  `Find`s, the one being walked to, and once-flags for the first body, pool,
  trap and bottle). New `Goal::Inspect`; the standing is `State::Idle`
  (`Action::Rest`), so no new Action or tuning rows.
- `discover()` keeps a find when `freeToInspect()`: goal Explore, Wander or
  Inspect; not hungry, thirsty, tired, needing to go or running; no foe, no
  fear of anyone, not leading or led to water; and, unless grim, not scared
  by blood. Otherwise the line is said at once, as before.
- `choose()`: `settleFinds()` first (taken up with something else, or 15 s
  on its way: the kept finds are remarked on where it stands); then
  `inspectNext(grimOnly)` just before `keepAway`, so it stares at a pool
  before backing off; then `inspectNext` for the rest after the company
  wait. Nearest first. Reach is 8 steps, 4 to 12 by curiosity; a find
  farther or unreachable is remarked on at once.
- `lookAt()`: faces it, logs `WENT CLOSER TO LOOK AT <THING>` (walked) or
  `STOPPED TO LOOK AT <THING>` (already there, or grim and neither brave
  nor curious, 0.6), stands 0.6 s to 4 s by curiosity after its line, which
  goes to the front of what it still meant to say.
- On the last step up to a find, it doesn't take what is in reach until it
  has looked (`choose()` takes it right after). A key or cake seen taken
  before it gets there is remarked on, not looked at.
- New moment `SpotTrap` (`[spot_trap]` in lines.txt), the first hidden trap
  spotted. The first statue is now a once-per-life find, dazed or not.
- Back on its way is a fresh `choose()`: each find is looked at once or
  remarked on, so it can't loop.
- Scenarios `inspect`, `inspect-hungry`, `inspect-body-timid`,
  `inspect-body-brave`. Scenarios about other things whose humans got
  pulled aside (the water deals, `lie-water`, `waking-alone`) start with
  `SoakProbe::seenItAll`. `saidAround` now finds a line within 4 s even with
  a log line between.
- Soak after it: 21 humans still 30 s (was 16), most of them long
  greetings; one loop, a human keeping company torn between a fresh pool it
  shuns and its company (pool fear vs company, not inspecting).
