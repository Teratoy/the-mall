# Running

Today a human runs only in a fight or away from one: `Human::move`
(`human_walk.cpp`) shortens the step to `RUN` (0.7) of a walk when the goal
is `Fight` or `Flee`, and `action()` counts it as `Action::Run` for the
hunger and thirst tables. It looks like a fast walk (`Pose::Walk`), and it
can run for as long as it likes.

This plan makes running matter: humans run when something is urgent,
when they chase or are chased, when they are afraid, and when someone else
is going for the same thing they need to live. Running costs breath, so a
long run ends winded, hands on knees, where a chaser can catch up. It has
its own look, and who runs, and how fast, comes from traits, wounds and
what is carried.

Ties into: fights and fleeing (`human_fight.cpp`), thirst, hunger and
relief (urgent needs), blame and blood (fear of a killer, running to wash),
cannibalism (hunting prey), sleep (robbers), dread (`scare()`), bottles
(spilling), the per-action tables, and the sprites.

## User's decisions (2026-10-31)

- **When**: urgent needs, chasing, fear, and racing for things that are
  essential for survival (cake and water; not keys).
- **Stamina**: a breath meter. Running drains it; empty, the human is
  winded: it stops, hands on knees, panting, and can't run again until it
  has some breath back. Tiredness rises faster while running.
- **Look**: a run pose of its own (leaning forward, longer stride, arms
  pumping) and a winded pose.
- **Who**: traits change who runs (the timid run from fear sooner, the bold
  seldom; the greedy race for cakes). The hurt and weak run slower or can't
  run (limping). Carrying slows the run, and may spill water or drop a
  cake. Every new reason to run gets a log line and a voice moment.

### Answers to the open questions (2026-10-31)

1. **Speed**: a running step takes 0.6 of a walking step (`RUN` from 0.7).
2. **Breath**: 7–9 s of running from full breath (rolled per human, by
   `rng_`), then 2–3 s winded.
3. **Costs**: running makes it thirstier and more tired than walking.
   Being winded costs the same as standing idle (`Action::Rest`).
4. **Dropping**: nothing spills. Bottles and cakes drop whole, onto the
   floor, one at a time, and only while running in a chase (chaser or
   chased). Only the stack carried in the back hand drops: cakes, and the
   bottle on top of them. At 2 things or fewer in all the chance is very
   low; it rises exponentially with each one more, and at 5 it is very high.
5. **Racing for water**: never for a basin, as basins don't run out. Not
   for a vending machine either, unless it has only one thing left.
   Racing is for cakes, bottles, and a machine's last one.
6. **Breath shows** in the stats menu (`WorldScene::drawStats`).

## Steps

### 1. Running as its own thing

- A `run_` struct on Human: `bool on` (wants to run this walk), `why`
  (an enum of reasons, for the log and lines), `breath` (0 to 1).
- `Human::running()` replaces the `Fight || Flee` test in `move()` and
  `action()`: true when `run_.on`, it has breath, and it isn't winded or
  too hurt (step 4).
- `startRunning(Goal, target, why)`: like `startWalking`, setting `run_`.
  Every `startWalking` clears it, so a run never outlasts its reason.
- Fight and Flee go through it, so today's behaviour is the first case.
- `Action::Run` already has hunger and thirst rows (thirst higher than
  walking). Make sure its `TIRED_PER_SECOND` row in `human_sleep.cpp` is
  higher than walking's too.
- `RUN` goes from 0.7 to 0.6.
- Pure refactor apart from that: `make check` should pass unchanged
  except for the tiredness change.


#### Built (2026-10-31)

`run_` (`Human::Run`) and `human_run.cpp`. `startRunning(goal, path, why)`
sets the walk running; `startWalking` clears it. Fight and Flee start
through it (`RunWhy::Fight`, `RunWhy::Flee`); `running()` drives the step
time, `Action::Run` and the pose. `RUN` is 0.6. Run's tiredness row was
already 4x walking's, so it stays.

Lungs (7–9 s) are rolled from the human's seed, as traits are, not from
`rng_`: an extra roll there shifted every later die and changed unrelated
scenarios.

The faster run changed the timing of three scenarios that rely on a human
asking or moving a stash before a chaser gets there: bottle-sell,
hard-no-sell (holder seed search now starts at 13100) and stash-robbed
(18100). Those scenarios are fragile: about a third of nearby seeds pass.

### 2. Breath and being winded

- Breath drains while running (7–9 s of running from full, the length
  rolled once per human), refills while walking slowly and faster while standing or sitting
  (`BREATH_REST`).
- At 0: `State::Winded` for `WINDED_TIME` (~2–3 s): standing still, bent
  over. It can't run again until breath is back over `BREATH_AGAIN` (~0.4);
  until then it walks, even fleeing.
- Fleeing winded is the danger: a chaser with more breath catches up.
- Winded counts as `Action::Rest` for the tables, like idling.
- Breath shows in the stats menu, beside the other needs.
- Log: "Out of breath." Moment: `Winded`.


#### Built (2026-10-31)

Breath comes back at 1/20 a second walking, 1/6 standing, sitting or
winded (`BREATH_WALK`, `BREATH_REST` in `human_run.cpp`). Winded is not a
state: `step()` stands it still while `run_.winded` lasts, keeping its goal
and path, so it walks on after (spent until `BREATH_AGAIN`, 0.4). Standing
still it counts as `Action::Rest`. Log "OUT OF BREATH", moment `winded`
with lines in `data/lines.txt`. The HUD says "CATCHING BREATH". Breath is
under BODY in the stats menu. Scenario `winded` (short lungs set through
`SoakProbe::setLungs`). Nobody runs out of breath in the soak yet: fights
and flights are short.

### 3. The run and winded poses

- `Pose::Run`: its own frames in `human_sprites.cpp`, body leaning
  forward a pixel, legs further apart than walking, arms swinging wide;
  cakes and bottle held as when walking. Played at the run's step rate.
- `Pose::Winded`: hands on knees, head down, chest rising fast.
- `world_view.cpp` picks Run when `h.running()` and moving.
- Add both to `make bottlesheet` and `make dirtysheet`, and the create
  scene's pose cycle if it fits.


#### Built (2026-10-31)

`HumanLook::run(frame)` (strides 2, 1, -2, -1, head a pixel ahead, arms
bent: the hand swung ahead comes up, the one swung back trails behind) and
`HumanLook::winded(frame)` (legs a row shorter, head hung and ahead, arms
straight down to the knees, breathing in on frame 1). `Pose::Run` at 14
fps, `Pose::Winded` at 4. In `make dirtysheet` and `make bottlesheet`.

### 4. Who runs, and how fast

- **Traits**: courage sets how frightened it must be to run from fear
  (step 6); greed lowers the bar for racing for cakes (step 7).
- **Hurt and weak**: below `RUN_HEALTH` health, or bleeding hard, or
  starving or parched, it can't run, only walk; between that and full
  health the run step lengthens. A deep wound makes it limp (slower
  walking too, and a limp in the walk frames if cheap).
- **Carrying**: each cake and a bottle lengthen the run step a little
  (`RUN_LOAD`).
- **Dropping in a chase**: while running in a chase (step 7), chaser or
  chased, the stack in the back hand may drop one thing at a time, the top
  first (the bottle, which rides on the cakes, then cakes). Nothing spills:
  a dropped bottle keeps its water. It lies on the floor like any dropped
  cake or bottle, for anyone to pick up. The chance per second grows
  exponentially with how many things it carries: `DROP_BASE * DROP_GROW^(n-1)`,
  tuned so 1–2 is very low and 5 is very high. A bottle held in the front
  hand never drops. Log: "Dropped a cake running." Moment: `Dropped`.
- All dice through `rng_`.

### 5. Urgent needs

Runs to the place it is going when:
- Bladder or bowels about to go (over `RUN_RELIEF`), to the spot it chose.
- Parched or starving and going to water or a cake it knows.
- Bleeding and going to wash (blood plan), when the bleeding is bad.

Moments: `RunNeed` (lines by need: water, cake, relief).

### 6. Fear

Runs instead of walking (or starts walking away, running) when:
- It sees a fresh body, a trap go off in sight, or a punch land, and its
  fright passes a bar set by courage (the bold seldom do).
- It avoids a blamed killer or someone it suspects (`Goal::Avoid` from
  `human_blood.cpp`), and they are close.
- A strong smell drives it off (`human_smell.cpp`), only the timid.
- A slam room opens and it gets out.

Moment: `RunFear`.

### 7. Chasing and being chased

- The robbed chases the thief: a cake or bottle snatched off it, or a
  sleeper waking robbed with the robber in sight, runs after them
  (`Goal::Fight` or a new chase goal), and the thief, seeing it come, runs.
- A hunter runs at prey it means to kill (`hunting()`), and prey that sees
  it coming runs.
- Whoever sees someone running at it who has hit it or is a foe runs too.
- A chase ends when the chaser loses sight, is winded, or catches up.

Log: "Ran after Hut." / "Hut ran after me." Moments: `Chase`, `Chased`.

### 8. Racing for what keeps it alive

- When a human going for a cake or a bottle it needs, or for a vending
  machine it saw had one thing left, sees another
  going for the same one (closer than it, or as close), and neither trusts
  the other much, it runs for it. The other, seeing it run, may run too.
- Humans only know what they see: the race starts from seeing the other
  head for it, not from knowing their goal.
- The greedy race sooner; the generous, trusting, or not that hungry let it
  go.
- Never for a water basin, which doesn't run out, or a machine with more
  than one left.
- Whoever gets there first takes it; the loser may beg, fight, or walk off
  as today.

Log: "Raced Hut for a cake, and won/lost." Moments: `Race`, `RaceWon`,
`RaceLost`.

### 9. Scenario and coverage

A scenario per reason the soak seldom reaches: winded while fleeing and
caught; chasing a thief; a race for one cake; a chased human with 5 things
dropping some. Check
`make coverage` lists none of the new lines.

## Build order

1, 2, 3 first (running, breath, look), then 4, then the reasons 5–8 one at
a time, each with its lines in `data/lines.txt` and its scenario.

## Open questions

All six were answered on 2026-10-31 (see "Answers to the open questions"
under the user's decisions). New ones go here as they come up.
