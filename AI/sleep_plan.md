# Sleep

Hunger made cakes matter and thirst made water matter. Sleep should make
**safety** matter. A sleeping human sees nothing, can't run, and holds
whatever it carries where anyone can take it. So the question every
tired human has to answer is *where can I shut my eyes, and next to
whom?* Its answer should tie into everything that's already there:
trust and groups (keeping watch), cunning and greed (robbing sleepers),
appetite (sleepers make easy prey), settling (a home is where you sleep),
the dead-end spots relief already finds, and the waking daze (every
human's first memory is waking up somewhere it doesn't know).

The animations are already built (2026-10-29): Pose::Sleep (crouch,
nodding off seated, then curled on its side breathing, with z's) and
Pose::WakeUp (still curled, blinking, then up), in
render/human_sprites.cpp. No state uses them yet.

## User's decisions (2026-10-30)

- **Rhythm:** tiredness from activity only, each human on its own
  clock. No world clock, no lights-out.
- **How often:** rarer than eating: about once every 15–20 min of play,
  a sleep lasting ~90 s.
- **Danger:** sleepers can be **robbed**, **attacked**, and can **die in
  their sleep**. They are **not** singled out to be eaten: those craving
  flesh get no pull towards sleepers (a sleeper killed some other way is
  a body like any other).
- **Extras in:** keeping watch, dreams, exhaustion collapse.
- **Out:** accidents while asleep (no wetting or soiling in sleep;
  answers relief_plan.md's open question 1).

## 1. Tiredness itself

- `Human::tiredness` 0..1 next to hunger and thirst, starting at 0–0.2
  (wakers have just slept, after all). Grows by its own per-action table,
  TIRED_PER_SECOND, beside the hunger and thirst tables, so every action
  has a tiredness cost too (the every-action-costs rule), and the new
  Sleep action has a cost in all three tables (hunger and thirst slow
  right down while asleep, e.g. Rest ÷ 3).
- Hard actions tire more: Run and Punch about 4× Walk; Sit and Rest the
  least, but never 0. Bleeding adds to it (blood loss is exhausting),
  and so does dread (fear wears you out).
- TIRED 0.5, EXHAUSTED 0.8, the counterparts of HUNGRY/STARVING. At 1 it
  doesn't kill: the human **drops asleep where it stands**, whatever it
  was doing (unless fighting, then the moment it stops). That's the
  danger: a collapse in the open.
- Exhausted effects, small: walks a little slower (step time × 1.15),
  punches miss more often, and it notices less (lower chance to spot a
  hidden trap per step, a shorter sight range for spotting small things).
- Log: "EXHAUSTED", "FELL ASLEEP", "WOKE UP", "COLLAPSED FROM
  EXHAUSTION". HUD: a fourth bar, TIRED, beside HP / HUNGER / THIRST;
  status "SLEEPING", "LOOKING FOR A PLACE TO SLEEP".

### Built (2026-10-30, build step 1)

Code in src/human/human_sleep.cpp. `Human::tiredness` (start 0–0.2),
TIRED_PER_SECOND per action (Walk 1/1800, Run 1/450, Punch 1/300, Rest
1/3600, Sit 1/5400, the rest between), + TIRED_BLEED 0.3 × bleeding +
TIRED_DREAD 1/1800 × dread. TIRED 0.5, EXHAUSTED 0.8 ("EXHAUSTED",
steps × 1.15). Action::Sleep costs hunger and thirst 1/1800 each. At 1,
choose() drops it asleep where it stands right after the fight line
("COLLAPSED FROM EXHAUSTION", straight to the curl, no nodding off).
State::Sleeping (Pose::Sleep), Goal::Sleep, Want::Sleep (base 2 +
tiredDrive). seekSleep: a Floor cell within 12 cells (3 when exhausted),
no door or water in the 8 round it, no known trap, not occupied, and
never a choke point (open cells round it in more than one bunch: a
corridor or gap); costs 3 per open cell round it, so dead ends beat
corners beat open floor ("FELL ASLEEP"). Asleep: sight.blind() (so it
witnesses nothing and watches nobody go), hears nothing, says nothing,
heals × 2 (SLEEP_HEAL), tiredness −1/170 a second. Wakes at RESTED 0.05
(State::Rising with roused_ → Pose::WakeUp, ROUSE_TIME + STAND_UP_TIME),
at a blow ("WOKE UP HURT", straight up to react), once below 0.3 if
starving, parched or bladder/bowels ≥ 0.9, and when full right up
whatever (no accidents asleep). Daze 0.25 on waking. HUD: a fourth bar,
TIRED; status "SLEEPING", "WAKING UP", "LOOKING FOR A PLACE TO SLEEP".
LIE_DOWN_TIME and ROUSE_TIME moved to human_look.h.

Soak (30 × 20 min): first try 524 sleeps of ~7 s: waking for any need
as soon as under TIRED put it straight back to sleep; with the 0.3 /
0.9 rule, 69 sleeps of ~60 s, about one per 21 min awake, asleep 4.8 %
of the time, 0 collapses, 0 exhausted spells; deaths and stuck/loop
flags unchanged (parched 43, thirst 20, starved 22, bled 15).

## 2. Sleeping

- `State::Sleeping`, `Action::Sleep`, `Goal::Sleep`, `Want::Sleep`
  (base 2 + tiredness drive, like Food and Drink, so being very tired
  can outweigh mild hunger but not starving or parched).
- Settling down follows the animation: crouch, nod off seated for
  LIE_DOWN_TIME (2.6 s), then asleep curled up. Someone coming up during
  the nodding-off part makes it lift its head and stay up (not asleep
  yet).
- Asleep, tiredness falls by SLEEP_RECOVERY per second until it's under
  0.05; one full sleep from exhausted takes ~90 s of game time (tuned
  from play). Wounds heal faster while asleep (HEAL_RATE × 2), if not
  hungry or thirsty, as now.
- Asleep it **sees nothing new**: observe() is skipped. It still hears
  (see step 4 for what wakes it).
- Waking is Pose::WakeUp: curled with eyes open for ROUSE_TIME, then
  up. A short daze after waking (reuse `daze_`, much milder than the
  first waking): what it last knew may have changed.

## 3. Where to sleep

A sleeping place is scored a bit like a relief spot (human_relief.cpp,
which already finds dead ends and corners), but for safety, not privacy:

- **Out of the way:** dead ends and corners score well, open plazas and
  doorways badly. Never on a known trap, never next to water (it blocks
  the way and others come there).
- **Away from danger it knows:** far from foes, suspects, the feared,
  bodies and fresh blood.
- **Near company it trusts:** next to a friend or someone it walks with
  is the best place of all (see step 5); a loner (sociable < 0.3)
  prefers being alone.
- **Home:** a settler sleeps at home. A settled place is where it slept
  safely before; a new lesson `safeSleep` remembers good spots.
- **Planning ahead** (user, 2026-10-30): from DROWSY (~0.35) tiredness,
  before it is tired, a human drifts towards a remembered safe spot or
  home, the way the nearWater lesson keeps it near water: strolls start
  from there, Explore is wanted less the farther away it is, and a
  group's leader brings the group along. So tiredness rarely catches it
  out in the open.
- Not too far: only within SLEEP_REACH cells; the more tired, the closer
  it settles for. Exhausted, it takes the best spot within a few cells.
- Watched: a modest or timid human won't go to sleep while a stranger it
  doesn't trust is in sight (uses watchedAt-style sight checks); the
  brave and the erratic don't care.

### Built (2026-10-30, build step 2)

In human_sleep.cpp. seekSleep's spot cost (in tenths of a cell, like the
walk itself: CELL_COST 10) is now 3 per open cell round it, plus 3 per
cell nearer than 10 to where it last saw anyone it fears, distrusts or
suspects, up to 15 × (1 − brave) for a body it knows within 4, blood it
knows within 2 (20 a cell's worth, fresh counting triple), 10 ×
sociable for sleeping beyond 3 cells of every friend or group member in
sight (if any), 8 for a loner (< 0.3) sleeping within 3 of anyone, and
25 for anywhere but where it means to sleep (sleepBase: home if
settled, else where it last slept and woke unharmed, within 2 cells;
walked to from up to 30 cells unless exhausted). The timid (brave <
0.4) and modest (≥ 0.55) never lie down where someone not a friend, not
in its group and trusted under 0.1 would see (sleepWatched), unless
erratic or exhausted. Woken by a blow there, the place is forgotten.
Lesson `safeSleep` (SAFE SLEEP row in the stats menu): learnt hard on
collapsing ("LEARNT TO FIND A SAFE PLACE TO SLEEP IN TIME"), paid off by
waking rested where it meant to sleep.
Planning ahead: drowsy() rises from 0.35 tiredness to 1 at tired, ×
(0.6 + 0.4 × safeSleep). Drowsy with a sleep place more than 12 cells
off, Explore and OpenDoor are wanted up to 0.6 less, Want::Sleep is
wanted up to 1.5 to walk back to within 2 cells of it
(returnToSleepPlace, as a Wander), and strolls start from it.
Keeping company with someone who falls asleep, it sits down beside them
(human_company.cpp), rather than standing there a minute and more.
Probe (scratchpad probe.cpp, 20 worlds): with a stranger in view the
timid never chose a spot it could see (bold: 11 of 20); slept within 3
of a friend 20 of 20 (stranger 6); twice as far from a foe; went back
to where it slept before 11 of 20 (8–25 cells off); drowsy for 4 min it
kept 19 cells from its sleep place on average, against 38 without one.
Soak (30 × 20 min): 66 sleeps, 0 collapses, asleep 4.9 %, nothing stuck
or looping; deaths unchanged.

## 4. Danger: what can happen to a sleeper

- **Waking up:** a hit always wakes. A fight, a scream or a death in
  sight within a few cells wakes the light sleeper (timid, high dread);
  someone walking right next to it might. Hunger or thirst past
  STARVING/PARCHED wakes it. Waking in a fright (woken by a hit) gives
  dread and a log line "WOKE UP TO @ HITTING ME".
- **Robbing:** a greedy, brave-enough human who sees a sleeper carrying
  cakes (or a key) may sneak up and take one. The thief rolls to stay
  unnoticed (lower for the light sleeper); if the sleeper wakes, it
  knows who did it (foe, −0.5 trust, log "@ ROBBED ME IN MY SLEEP"). If
  it doesn't wake, it only finds the cake missing: suspicion falls on
  whoever it last saw near it (a natural seed of wrong accusations).
- **Attack:** a hungry or erratic human who wants the sleeper's cake
  prefers sleepers as targets (not those craving flesh, user): a free first punch, a
  dazed defender for a moment. Witnesses who see it take it very badly
  (like seeing a killing).
- **Dying in one's sleep:** a sleeper bleeding out, starving or parched
  can die asleep. Drawn as the curl going still (z's stop); others who
  find it can't tell at first and may try to wake it (a [wake_them] line
  and a nudge) before they realise.

### Built (2026-10-30, build step 3)

- Waking (human_sleep.cpp): lightSleep() = 0.2 + 0.6 × (1 − brave) + 0.5 ×
  dread. Lighter than 0.3, a fight, someone just hurt, or a death in its
  first second within 5 cells wakes it at up to 1.5/s, someone walking
  right past at 0.3/s, by how much lighter ("WOKE UP TO A NOISE", dread
  +0.05). Hit asleep (takeHit): "WOKE UP TO @ HITTING ME", dread +0.2,
  daze 0.6, and it can't answer the blow for 0.8 s (stunned_); hurt
  otherwise (darts, spikes): "WOKE UP HURT". Set on (hit or robbed), the
  place it slept is forgotten. wakeUp now looks round (observe) itself.
- Robbing: Want::Rob (WANTING 0.3 + 0.2 brave + 0.6 greedy − 0.2
  sociable, + 0.4 with a sleeper in sight, + 0.8 × hunger drive). Only
  the greedy ≥ 0.5 and brave ≥ 0.35, with room to carry, from a sleeper
  in sight holding a cake, not a friend, not in its group, trusted under
  0.2, not feared. Goal::Rob walks beside them, then kneels 1.5 s
  (State::Looting with robbee_, Pose::Search, "ROBBING @"). Takes one
  cake; they wake with chance 0.3 × (0.5 + lightSleep): "@ WOKE UP AS I
  ROBBED THEM" / "WOKE UP TO @ ROBBING ME", "@ ROBBED ME IN MY SLEEP",
  −0.5 trust, and they answer it as a blow (react: fight or flee).
  Unseen: "ROBBED @ IN THEIR SLEEP". Anyone who sees it −0.3.
- Missing cakes: on waking with fewer cakes than it lay down with,
  "SOMEONE TOOK MY CAKE WHILE I SLEPT", and it blames the nearest it
  sees within 6 (friends last), else whoever was nearest in sight as it
  fell asleep: "@ MUST HAVE TAKEN IT", −0.3 trust. Often the wrong one.
- Attacking: a sleeping cake holder in sight looks 6 cells nearer to the
  hungry (nearestHolder), so they go for it first. "HIT @ IN THEIR
  SLEEP"; witnesses −0.5 trust, dread +0.3, remembered as strongly as a
  killing. Those craving flesh get no pull to sleepers (user).
- Dying asleep: die() keeps diedAsleep_; the body is drawn curled and
  still (Pose::DiedAsleep: no breath, no z's), eaten and rotting on the
  curl (HumanSprites::remains curled). From farther than 2 cells it is
  taken for a sleeper (not a body: no FOUND, no looting); close up,
  "FOUND @ DEAD. I THOUGHT THEY WERE ASLEEP". The nudge and [wake_them]
  line wait for talk (step 5).
Probe (scratchpad probe3.cpp, 30 worlds each): a greedy bold thief by a
sleeper with 2 cakes robbed unseen 22 times and was caught 18 times
(each caught one woke to it); a hungry human went for a sleeping cake
holder 23 of 30, hit it asleep 18 (all woke to the blow), and 10
witnesses took it as badly as a killing; a starving sleeper died asleep
30 of 30, found at once from 1 cell (30, "thought asleep"), and from 4–7
cells only 2. Soak (30 × 20 min): 63 sleeps, 3 died asleep, 1 hit
asleep, no robberies (it takes a greedy, bold human coming on a sleeper
with cakes); nothing stuck, deaths unchanged (the one 30 s stand was a
greeting then keeping company, not sleep).

## 5. Sleeping in company and keeping watch

This is where trust pays off.

- Friends and group members (inGroupWith) like to sleep near each other.
  A group whose leader goes to sleep settles round the leader.
- **Keeping watch:** in a group, one who is not too tired may stay up
  while the others sleep (sociable and brave make it more likely). The
  watcher sits by them, wakes them if a foe or someone suspected comes
  close (a [wake_up] line), and gets turns: when it's tired, it wakes the
  most rested one to take over (a [your_watch] line).
- A watcher who falls asleep on watch (tiredness 1) collapses like
  anyone; the group may learn to trust it less if harm came of it.
- Cunning betrayal (fits the trading/cunning ideas in memory.h): a
  watcher with low trust in a sleeper and greed may rob it instead of
  watching. Rare, and it ruins the group if seen.
- Asking: a tired human with a friend nearby may ask them to watch
  ([watch_me]); sociable friends agree, greedy ones might ask for a cake.

### Built (2026-10-30, build step 4)

In human_sleep.cpp unless said. Group = inGroupWith or a friend.
- Settling round a sleeper: drowsy (≥ 0.35) with one of its group asleep
  in sight within 6, it lies down near them too (sleepsWithGroup →
  Want::Sleep → seekSleep, whose company cost keeps it close), unless on
  watch or asked to watch. Followers sit by a sleeping leader
  (startFollow), as companions already did.
- Keeping watch (choose → keepWatch, after givePrivacy): not hungry,
  thirsty, needing to go or afraid; of its own accord only if sociable ≥
  0.3, tiredness < 0.4, and (sociable + brave) / 2 ≥ 0.45; over the
  nearest of its group asleep within 8 that nobody else watches
  (watchee_; the sleeper's watchedBy_). Walks within 2 (Goal::Watch) and
  sits ("KEEPING WATCH OVER @"); new Action::Watch (hunger and thirst
  1/1500, tiredness 1/1200: staying awake on purpose). Each glance while
  sat (watchCalls) gets it up if the sleeper woke, a threat came, or it
  is tired or in need.
- Alarm: a foe, suspect, whoever it fears, or anyone sneaking up to rob
  (robbee_) within 6 of the sleeper: it wakes every sleeper of its group
  within 8 ("WOKE THE OTHERS: @ WAS COMING" / "@ WOKE ME. SOMEONE WAS
  COMING"). Thieves won't rob a sleeper someone sits watch over within 3
  (wouldRob).
- Turns: tired on watch, it wakes the most rested of its group asleep
  (tiredness < 0.25) to take over ("WOKE @ TO TAKE A TURN ON WATCH"),
  who then watches it, whatever its traits (askedWatch_); then it goes
  to sleep itself. If nobody is rested enough it watches on until it
  collapses: "FELL ASLEEP ON WATCH".
- Asking: lying down sleeping lightly (lightSleep ≥ 0.45), it asks the
  nearest friend or group member awake within 6, not tired or needy
  ("ASKED @ TO KEEP WATCH"): a friend or the sociable (≥ 0.5) agree; the
  greedy (≥ 0.6) for a cake if it has one; else "WOULDN'T KEEP WATCH
  OVER @".
- Failing: harmed asleep (hit, or a cake gone) while its watcher slept
  or was more than 8 away: "@ WAS MEANT TO KEEP WATCH", −0.3 trust.
- Betrayal: a watcher greedy ≥ 0.67 and brave ≥ 0.35, not fond of the
  sleeper (not a friend, trust < 0.3), may rob them instead (15 % each
  time it thinks again; goRob). Caught: "@ ROBBED ME ON THEIR WATCH",
  −0.8, and the sleeper leaves its company or following; unseen, "ROBBED
  @ ON MY WATCH", and the sleeper likely blames whoever is nearest on
  waking: the watcher.
- Status lines: "KEEPING WATCH OVER @", "GOING TO KEEP WATCH OVER @".
Probe (scratchpad probe4.cpp, 25 pairs each): a rested, willing
companion kept watch 25 (a timid one 0); woke the sleeper for a foe 25;
a nervous sleeper asked a friend who wouldn't volunteer 25, who then
watched 25; a tired watcher handed over 25, the woken one took the watch
25 and the first slept 25; a greedy watcher not fond of the sleeper
robbed them 5 of 25 within 40 s; a thief robbed a watched sleeper 0,
an unwatched one 25; a drowsy companion lay down by the sleeper 25.
Soak (30 × 20 min): no watches (groups are rare), nothing new stuck;
crowded soak (12 × 20 min, 6 extra humans): 1 watch, 2 robbed, 4 hit
asleep, nothing stuck from sleep.

## 6. Talk, hope and memory

- Talk lines: [tired], [exhausted], [sleep] (yawn, said to nobody,
  muttering allowed), [woke], [robbed_sleep], [wake_up], [your_watch],
  [watch_me], [wake_them]; conditions tired / exhausted / they.asleep.
- Hope: a good night's sleep lifts hope a little (+0.05); waking robbed
  or hit takes a lot (−0.2). Sleeping at home counts towards settling.
- Dread falls a little with each full sleep (rest calms).
- Waking from sleep echoes the first waking: a small chance of a
  [dream] line on waking, half-memories of "before" (escape plan: humans
  remember nothing from before). These could be the misleading clues of
  escape step 5: every dream line is vague and some contradict each
  other, so dreamers' theories feed the hunches told at greetings.

### Built (2026-10-30, build step 5)

- Talk (data/lines.txt, Sleep section; talk.h/talk.cpp): moments tired,
  exhausted, sleep (lying down, unless it asked someone to watch), woke,
  dream, robbed_sleep (to the thief), cake_gone (about whom it blames),
  watch_me / watch_yes / watch_cake / watch_no, wake_up (alarm, to the
  sleeper), your_watch (handing over), failed_watch (to a watcher who
  slept or went), wake_them (finding a body it took for asleep, about
  them, before found_body). Conditions tired, exhausted, dreamt,
  they.asleep. Asleep it says nothing once lying down: lines still to say
  are dropped, a bubble begun while nodding off runs out.
- Hope and dread: slept right through (woke rested): hope +0.05, dread
  −0.1; woken robbed or hit: hope −0.2; a cake found gone: hope −0.1.
- Home: slept safely in the same place 2 times running, and no water
  near, that place is where it makes its home on settling ("GAVE UP ON A
  WAY OUT. MADE A HOME WHERE I SLEEP", human_hope.cpp), ahead of a table.
- Dreams: waking in its own time, 30 % of the time it dreamt ("DREAMT OF
  BEFORE, I THINK", [dream]): its hunch of the way out turns 35 % of the
  way toward a random direction and it grows 0.05 surer, so dreamers grow
  sure of ways that disagree and tell them at greetings (escape plan step
  5's misleading clues). For 10 minutes after, before comes back to mind
  in chat (+0.3 to the Before topic; chat_before has dreamt lines).
Probe (scratchpad probe5.cpp): all 15 new moments load and fit; dreamt
70 of 200 wakings, hunch swinging 0.53 rad on average; no new line once
lying down. Probes 3 and 4 unchanged. Soak (30 × 20 min): 68 sleeps, 0
collapses, deaths unchanged; the two 30 s stands were long greetings
(chit-chat), not sleep.

## 7. Relief while asleep

Out (user, 2026-10-30): no accidents in sleep. Relief urgency grows
while asleep at the Sleep rate like any action, and a human who wakes
needing to go simply goes.

## 8. Soak and tuning

- tests/stuck_soak.cpp: count sleeps, collapses, robbed sleepers, deaths
  in sleep, and time asleep. 30 seeds × 20 min.
- Targets: most sleeps chosen, not collapses; humans awake most of the
  time (asleep under ~20 % of the time); no new stuck/loop flags (a
  sleeper lies on a cell; GiveWay must walk round sleepers, never wait
  for them to move).

## Build order

1. Tiredness, costs, collapse, Sleep state/goal/want, HUD bar, the poses
   hooked up in poseOf. Simple spot choice (nearest dead end/corner
   within reach). Soak.
2. Spot scoring (danger, company, home, watched) and the safeSleep
   lesson.
3. Danger: waking rules, robbing sleepers, attacking sleepers, dying
   asleep.
4. Sleeping in company and keeping watch.
5. Talk, hope, dread, dreams.

## Open questions

None now (answered 2026-10-30); all five build steps built the same day. Timings (TIRED_PER_SECOND, SLEEP_RECOVERY)
get tuned from play to hit ~15–20 min awake, ~90 s asleep.
