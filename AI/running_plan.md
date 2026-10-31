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
- `Action::Run` already has hunger and thirst rows. Add its row to
  `TIRED_PER_SECOND` in `human_sleep.cpp` if it's still the walk's (check),
  higher than walking.
- Pure refactor apart from that: `make check` should pass unchanged
  except for the tiredness change.

### 2. Breath and being winded

- Breath drains while running (`BREATH_RUN`, about 8–10 s of running from
  full), refills while walking slowly and faster while standing or sitting
  (`BREATH_REST`).
- At 0: `State::Winded` for `WINDED_TIME` (~2–3 s): standing still, bent
  over. It can't run again until breath is back over `BREATH_AGAIN` (~0.4);
  until then it walks, even fleeing.
- Fleeing winded is the danger: a chaser with more breath catches up.
- Winded counts as `Action::Rest` for the tables (or a new
  `Action::Pant`, with rows in all three, if the user wants it to cost more).
- Log: "Out of breath." Moment: `Winded`.

### 3. The run and winded poses

- `Pose::Run`: its own frames in `human_sprites.cpp`, body leaning
  forward a pixel, legs further apart than walking, arms swinging wide;
  cakes and bottle held as when walking. Played at the run's step rate.
- `Pose::Winded`: hands on knees, head down, chest rising fast.
- `world_view.cpp` picks Run when `h.running()` and moving.
- Add both to `make bottlesheet` and `make dirtysheet`, and the create
  scene's pose cycle if it fits.

### 4. Who runs, and how fast

- **Traits**: courage sets how frightened it must be to run from fear
  (step 6); greed lowers the bar for racing for cakes (step 7).
- **Hurt and weak**: below `RUN_HEALTH` health, or bleeding hard, or
  starving or parched, it can't run, only walk; between that and full
  health the run step lengthens. A deep wound makes it limp (slower
  walking too, and a limp in the walk frames if cheap).
- **Carrying**: each cake and a bottle's water lengthen the run step a
  little (`RUN_LOAD`). Running with an open bottle may spill some water
  (a small puddle, `Spill` log line); a cake may drop now and then when
  carrying many (`RUN_DROP_CHANCE`), left where it fell.
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

- When a human going for a cake, a bottle or water it needs sees another
  going for the same one (closer than it, or as close), and neither trusts
  the other much, it runs for it. The other, seeing it run, may run too.
- Humans only know what they see: the race starts from seeing the other
  head for it, not from knowing their goal.
- The greedy race sooner; the generous, trusting, or not that hungry let it
  go.
- Whoever gets there first takes it; the loser may beg, fight, or walk off
  as today.

Log: "Raced Hut for a cake, and won/lost." Moments: `Race`, `RaceWon`,
`RaceLost`.

### 9. Scenario and coverage

A scenario per reason the soak seldom reaches: winded while fleeing and
caught; chasing a thief; a race for one cake; spilling from a bottle. Check
`make coverage` lists none of the new lines.

## Build order

1, 2, 3 first (running, breath, look), then 4, then the reasons 5–8 one at
a time, each with its lines in `data/lines.txt` and its scenario.

## Open questions

1. **Speed**: keep the run at 0.7 of a walk step (about 1.4x as fast), or
   faster? Recommendation: 0.6.
2. **Breath length**: about 8–10 s of running from full, 2–3 s winded?
3. **Winded cost**: just resting, or a new `Pant` action that costs more
   thirst?
4. **Spilling and dropping**: how often? Recommendation: a spill only from
   a bottle over half full, a sip's worth now and then; a cake dropped only
   when carrying 3 or more, rarely.
5. **Racing water**: water basins don't run out, so is a race only for
   cakes and bottles, or also for a basin someone guards or a vending
   machine?
6. **The player's human**: anything different, for example a line in the
   stats menu showing breath?
