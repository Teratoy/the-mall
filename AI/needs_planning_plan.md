# Planning for needs

Many humans loop through their needs badly. They can't plan ahead or do
two things at once, so they change their minds partway through a task, and
because the mall is big, every change of mind costs a long walk (user,
2026-11-01). A human should look after its body the way a person on a long
walk does: drink when it passes water, turn back before it is too late,
look for water when it knows of none, and finish the errand it set out on.

## What the probe shows

`make trips` (`tools/trips.cpp`) runs the soak's 6 worlds for 15 minutes,
6 humans each, and tracks every walk set off for a need. Measured on
2026-11-01 at b009eec:

- **Humans walk 59% of the time.** Only 13% of that walking is for a need.
- **Water and cake trips are often dropped.** Of 100 walks to water, 30
  were dropped on the way. The dropped ones were long (106 steps on
  average; finished ones were 28). Of 121 walks to cake, 67 were dropped.
  Of all the dropped walks, 23 to water and 38 to cake were set off again
  within half a minute: back and forth. Relief (14 trips) and sleep (16)
  trips were never dropped.
- **What it drops a cake trip for:** greeting someone new (Meet 11),
  exploring (9), keeping away from blood or a suspect (Avoid 7), wandering
  (5), opening a door (3), a fight (3). Seed 3 #1, thirst 0.71 to 0.91 and
  not hungry, goes FetchCake → Avoid → FetchCake → Avoid → FetchCake →
  Wander, and sits down twice.
- **Half of all thirst spells start with no water known:** 68 of 144.
  Thirsty and knowing no water, it spends only 37% of the time exploring.
  23% goes to standing or sitting, and the rest to fetching cake, company,
  picking things up and fights, all while there is still unexplored ground
  it could reach. Being boxed in with nothing left to try (sitting by design,
  [parched_lost_plan.md](parched_lost_plan.md)) is only 10%.
- **It walks past water just before it needs it.** 46 of the 144 had stood
  beside water in the minute before getting thirsty and walked on without
  drinking, because it drinks only from thirst 0.5.
- **It goes too far.** Seed 1, Taglie (#8): no water known, exploring and
  opening doors further out from thirst 0.5 to 0.8. Parched, it sets off for
  the one toilet it knows, 483 steps back (about 100 s of walking; a step
  costs 1/1500 thirst), and dies 28 steps short.

## Why

1. **Needs count only past a line.** `choose()` drinks at thirst ≥ 0.5 and
   eats at hunger ≥ 0.5. Below that, water or a cake right beside it is
   worth nothing, so it walks past them and then later walks a long way back.
2. **No sense of range.** Nothing weighs how far the water it knows is
   against how thirsty it is. `waterTether()` does that only after the "keep
   near water" lesson, so a human who hasn't learned it explores without
   limit.
3. **A need with no known source doesn't make it search.** When Want::Drink
   fails (no water known), choose() falls through to the next want: Fetch
   (0.45), Company, Wander. Exploring is wanted no more than when it is
   comfortable (0.25 + curious).
4. **No sticking to a plan.** While walking, anything new it sees
   (`move()` → `rethink_`) runs choose() again from scratch. A reflex
   (keepAway, a greeting, a pile to pick up) wins, and when it is over the
   human starts again from a new plan, which is often a different one.
5. **One errand at a time.** It never does two things on one trip (drink on
   the way to the cakes, take a cake on the way back from water).

## Steps (proposed)

1. **Topping up.** Beside water it would drink, or within a few steps of
   it, it drinks once thirst is past TOP_UP (say 0.25), even though it is
   not thirsty yet. Beside a cake table with room to carry, it takes one,
   as Fetch already does (no change). Log: "DRANK WHILE I COULD"; moment
   `TopUp`.
2. **A sense of range.** Each time it rethinks, it knows how many steps away
   the nearest water it knows is. When getting there would take thirst
   past PARCHED, wanting to explore and open doors falls off with distance,
   and it wants to go back to drink before it is thirsty. Log: "TOO FAR
   FROM WATER. TURNING BACK"; moment `TurnBack`. The "keep near water"
   lesson makes that range shorter.
3. **Looking for water.** Thirsty with no water known, Explore takes the
   thirst drive (as Drink would). Wander and sitting are allowed only once
   there is nothing left to explore. Hungry with no cake known: the same,
   for Explore after cakes. Log: "LOOKING FOR WATER"; moment `SeekWater`.
4. **Sticking to it.** A walk set off for a need is kept until it gets
   there unless something new outscores it by COMMIT_MARGIN. Reflexes that
   interrupt it (keeping away, fleeing, a fight, someone asking it
   something) put it back on the same errand afterwards, rather than
   choosing again from scratch. Greeting someone new waits until the errand
   is done, unless it is parched and might ask them for water.
5. **Two errands at once (maybe).** When it sets off for one need and
   another is close to its line, it picks a source of the second within a
   short detour of the way and does both.
6. **Measured.** `make trips` before and after each step: fewer dropped
   trips and back-and-forths, less time standing about while thirsty and
   not boxed in, and fewer deaths of thirst in the soak. A scenario each
   for topping up, turning back and looking for water.

## User's decisions (2026-11-01)

1. **Topping up:** only once learned, by traits. A human tops up only
   after it has learned to keep near water (`lessons.nearWater`, learned
   when it gets parched), and the stronger the lesson, the sooner. Traits
   move the line: the timid top up earlier, the brave and the curious later.
   New humans still make the mistake.
2. **Range:** only once learned, by traits. The sense of range grows out of
   the same lesson (today's `waterTether()`, made into a real
   turn-back-in-time rule). The brave and the curious push further.
3. **Sticking to it:** danger and greeting someone new may interrupt a walk
   for water or cake (fights, fleeing, keeping away from blood or a suspect,
   being asked for something, and a first greeting). Afterwards it goes back
   to the same errand. Spotting a key, a door or anything else on the way
   doesn't turn it off.
4. **Two errands at once:** build step 5 now, with steps 1 to 4.

Steps 3 (looking for water) and 4 (sticking to it) apply to everyone, from
the start.

## Built (2026-11-01)

All in `src/human/human_errand.cpp` (state in `Human::Errand`, `errand_`),
with hooks in `choose()`, `pursue()`, `arrive()` and `startDrinking()`.

- **Step 4, sticking to it.** A walk for Food, Drink, Fetch or Fill is an
  errand. While it lasts, choose() leaves out every want but the needs
  (food, drink, relief, sleep, flesh, hunt, track), a first greeting
  (Meet) and the errand's own; the reflexes for danger stay first, and
  looking at something new or waiting for company are skipped. After
  being turned aside it goes back to the same errand: "WENT BACK FOR THE
  WATER" / "... THE CAKE" / "WENT BACK TO FILL MY BOTTLE", moment
  `errand_back`. Ends on arrival, when the want goes, when it can't be
  pursued, or after ERRAND_KEEP (90 s). Turned aside more than
  ERRAND_ASIDES (2) times, it stops keeping to it, and that want waits
  ERRAND_REST (30 s) unless thirst or hunger press. Without this rest, a
  human went back and forth between filling its bottle and keeping away
  from blood beside the water.
- **Step 3, looking for water and food.** Thirsty and knowing no water at
  all (no water, basin or dispenser seen), with nobody in sight it could
  ask for a drink, Explore takes the thirst drive, scored SEEK_BELOW (0.2)
  under drinking; hungry and knowing no cake, the same. This is for the
  settled too. It searches around whoever stands in the way rather than
  waiting on them. "LOOKING FOR WATER" / "LOOKING FOR FOOD", moments
  `seek_water`, `seek_food`. (Seeking when known water was only out of
  reach for a moment made a human step back and forth by water beside a
  suspect, so it is for no water known at all.)
- **Step 1, topping up.** `topUpLine()`: only with the near-water lesson;
  THIRSTY − TOP_UP_EARLY (0.35) × lesson, sooner for the timid, later for
  the brave and curious (TOP_UP_BOLD 0.5), never below 0.15. Past it,
  beside water or within TOP_UP_REACH (6) cells, it drinks: "DRANK WHILE
  I COULD", moment `top_up`.
- **Step 2, range.** `rangeUsed()`: only with the lesson and a last drink
  known. The walk back to where it last drank (1.5 × the straight-line
  cells) against the steps of thirst left before parched, bottles counted,
  times RANGE_REACH (0.6), further for the bold (RANGE_BOLD 0.4), shorter by
  the lesson (RANGE_LESSON 0.4). `waterTether()` now grows with it; at 1 it
  turns back to the nearest water before it is thirsty: "TOO FAR FROM
  WATER. TURNING BACK", moment `turn_back`.
- **Step 5, two errands.** Built as "while here" rather than planned from
  the start of a trip: within CHAIN_TIME (30 s) of a drink, a cake within
  CHAIN_REACH (15) cells is fetched if it has none, or has room for one and
  is CHAIN_HUNGER (0.25) hungry; after taking a cake, water that near, by
  those who would top up. "FETCHING A CAKE WHILE I'M HERE" / "DRINKING
  WHILE I'M HERE", moment `while_here`.
- **Fixed on the way.** Red water out of sight was taken as drinkable,
  since only seen water was judged; it is now remembered (`water_.red`).
  The water search tries again through a crowd before it gives up.
- **Scenarios:** top-up, top-up-unlearned, turn-back, while-here,
  seek-water, seek-food.

### Measured

`make trips`, before → after:

| | before | after |
|---|---|---|
| walks to water dropped (of ~100) | 30 | 22 |
| steps lost on dropped water walks | 2424 | 518 |
| walks to cake dropped | 67 of 121 | 34 of 86 |
| cake walks set off again within 30 s | 38 | 13 |
| thirsty, no water known, not boxed in: standing about | 23% | 9% |

Soak (`tests/golden/soak.txt`): died of thirst 11 → 5, starved 3 → 1,
alive at the end 27 of 64 → 45 of 77; unmoved 30 s 22 → 19, loops 2 → 0.

### Still open

- A suspect by the water makes the way there open and shut step by step
  (`shunZone` is measured from where the search starts, and both move), so
  a human near that water steps back and forth for up to 15 s. Older than
  this plan.
- In a world with almost no cake, looking for food walks off hunger
  faster than sitting would (seed 4 of an earlier run starved 6). Fine
  for now; worth watching.
- Rescue (a cry from someone shut in) no longer turns a human off an
  errand, per decision 3. Say if it should.
