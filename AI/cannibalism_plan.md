# Cannibalism plan (2026-10-29)

**Built 2026-10-29**, all 8 steps. The code is in `src/human/human_flesh.cpp` (mind,
feeding, hunting, witnesses), hooks in `human.cpp`, sprites in
`src/render/remains_art.cpp`, talk sections `snap hunt feed sick saw_eat
gnawed haunted craving` (+ conds `erratic guilty craving`), and the HUD row
`MIND n  GUILT n  CRAVE n` (red once erratic). Changes from the plan below:
erratic does nothing beyond flesh yet (answer 2); hunt lines are rude, not
threats, since a threat makes the prey hand over a cake and ends the hunt;
the timid never eat flesh, even when erratic. Soak 30 x 20 min: 30 meals
(1 rotten), 7 hunts, 4 killed to eat, 50 snaps, no new stuck humans or loops.

Decisions (user): **erratic is a mental state, not a trait**. Nobody is
born erratic; starving, horrors and flesh push anyone toward it. **Both the
brave and the erratic can kill to eat, with different thresholds**: the
brave only when starving, and only prey too weak to fight back; the erratic
when merely hungry, and almost anyone. Everyone who may hunt will also eat
the dead, and at a lower bar. Eating flesh weighs on the mind in two ways,
**guilt and appetite**: how erratic a human is at the time decides the split. A
body is **fresh for about 5 minutes**. After that it is spoiled, and eating it makes
you sick.

Builds on: `bodies_` / looting (kneel at a body), `foe_` + `startFight`
(hunting), `PREY_HEALTH` (who counts as weak), `dread_` / `scare()`, trust and
gossip (word of an eater spreads), blood on humans (a red mouth), talk
moments and `note()`.

## 1. The erratic meter
- `Human::erratic_` 0..1, plus `erratic()` and `ERRATIC = 0.5` (from here on
  the human counts as erratic).
- Rises from:
  - each second spent starving (`ERRATIC_STARVE`, about 0.5 over 2 starving minutes)
  - dread held high (dread above 0.6 drips into erratic)
  - each first sight of a gnawed body (`ERRATIC_GNAWED`)
  - appetite (step 5)
  - guilt that boils over (step 5)
- Fades slowly while fed and calm (`ERRATIC_FADE`, about 1/600 per s), and never
  below what appetite holds it at.
- Effects besides flesh: wish scores get random jitter in proportion to
  erratic, so choices turn erratic; trust gains are halved; the human greets
  less.
- HUD: `MIND` value on the friends row next to FEAR.

## 2. Flesh on bodies
- On `die()`: `flesh = FLESH_MEALS` (3) and `diedAt = world.time()`.
- `Body` (what a human remembers) gains `flesh` last seen and
  `gnawed`.
- Fresh while `time - diedAt < FRESH_TIME` (300 s). A meal from a fresh body takes
  `FLESH_FOOD` (0.4) off hunger. A spoiled one takes half that, and makes
  the eater sick: `SICK_DAMAGE` health over a few seconds and no healing for
  a minute.
- Eaten bodies look gnawed: an extra blood drop under them and darker
  stains. A real gnawed sprite can come later.

## 3. Who eats the dead (scavenge)
`Want::Flesh`, scored after Food. Tried only when no cake (known, on a
body, or held by someone) is nearer than the body. Allowed when:
- **brave** ≥ `BRAVE_FLESH` (0.6) and starving, or
- **erratic** and hungry, or
- **anyone at death's door**: starving with health < `DESPERATE` (0.15)
  (open question 1).
- Guilt raises the bar (needs more hunger); appetite lowers it (hungry
  enough sooner, and later even when not hungry).
- Never a friend's body unless erratic. Bodies near whoever it fears are
  left alone, the same as when looting.

`Goal::Feed` → `State::Feeding` (kneel, `FEED_TIME` 4 s,
`Action::Feed` hunger cost) → `feed()` takes one meal and puts blood on the
eater's head and front arm (the face and hands).

## 4. Who kills to eat (hunt)
`Want::Hunt`, scored just under Flesh. It picks prey from humans in sight:
- **Brave** (≥ `BRAVE_HUNT` 0.7), starving, not erratic:
  - prey health < `PREY_HEALTH` (too weak to fight back)
  - never a friend
  - only when no one else is in sight (a sane hunter doesn't want to be seen)
- **Erratic**, hungry:
  - any non-friend; above `ERRATIC_MAD` (0.8), friends too
  - weakest first, then nearest
  - witnesses don't stop it
- Appetite lets either hunt sooner (lower hunger bar, larger prey health).
- `Goal::Hunt` reuses the fight: `foe_ = prey` with a `killing_` flag, so it
  doesn't stop when a cake is snatched and doesn't give up on the grudge timer. It
  runs on until the prey dies, or until its own health drops below flee health. Prey flees as now.
- Once the prey is dead it goes straight into feeding on that body.

## 5. The mind after eating
Each meal has weight `w`:
- 1 for a body found
- ×2 if it killed them
- ×1.5 if it had met them
- ×2 if they were a friend

It splits by erratic `e` at that moment:
- `guilt_ += w * GUILT * (1 - e)`
- `appetite_ += w * APPETITE * e`

**Guilt** (the steady, often brave first-timer):
- a scare of `DREAD_GUILT * w` on the spot
- fades slowly (1/1800 per s)
- raises sit chance, lowers Meet/Follow/Escort wishes
- haunted lines and a dread drip near the body or where it happened
- raises the bar for the next flesh meal
- over 1 it boils over: the excess goes into erratic, so guilt can break someone

**Appetite** (the erratic):
- lowers both bars (step 3, step 4)
- keeps erratic from fading below `appetite * 0.8`
- fades very slowly (1/3600 per s)
- craving lines when it sees a weak human or a fresh body while hungry
- not hungry but appetite > 0.6: may still eat the dead, which makes a spiral

Log notes: `ATE FROM @'S BODY`, `KILLED @ TO EAT`, `CAN'T STOP THINKING ABOUT @`.

## 6. Witnesses and word
- Seeing someone feed: scare `DREAD_SAW_FEED` (0.5, halved for the erratic),
  trust toward the eater set to at most `TRUST_EATER` (-0.8), and the eater is
  marked in `Memory` as an eater. Gossip about eaters passes on through the
  existing People tell.
- Known eaters are kept away from like suspects; a timid human runs on sight. A
  brave friend of the eaten picks a fight.
- A red mouth with no wound on the face already reads as suspicious (blood plan
  step 6).
- Erratic witnesses: half the dread, and above `ERRATIC_MAD` they may come
  to share the body.

## 7. Talk, log, HUD
- New moments in `data/lines.txt`: `feed`, `hunt`, `saw_feed`, `craving`,
  `haunted`, `sick`. Each event also writes a `note()`.
- HUD: `MIND n` (erratic), and `GUILT n` / `HUNGER n` (appetite) shown only once
  non-zero, kept inside the measured strips.

## 8. Test
- `make stuck` 30 seeds × 20 min: no new blocks or loops.
- Count per run: flesh meals, hunts started and finished, brave vs erratic
  hunters, guilt breaking into erratic, sickness from spoiled bodies.

## Order
1 → 2 → 3 is the playable core (erratic and eating the dead). 4 needs 3. 5 can come in with 3
and grow with 4. 6 and 7 come after 4. Every step touches `human.cpp`/`human.h`, so
check for edits by other sessions first and use targeted edits.

## Answers (user, 2026-10-29)
1. ~~Death's door~~: no. The timid (< `TIMID`) would rather starve, and never
   eat flesh at all. Steady humans of middling bravery at death's door may.
2. ~~Erratic beyond flesh~~: will be expanded later. For now it only drives flesh
   eating (no jitter, no trust or greeting changes yet).
3. ~~Spoiled bodies~~: a spoiled body is only chosen as a meal with enough
   appetite (`SPOILED_APPETITE`). If eaten, it counts for the mind like a fresh one.
4. ~~Seen killing~~: someone becomes a known eater only when seen eating. The
   kill itself reads as an ordinary beating.
5. Added: sprites for eaten bodies. They change with how much has been eaten
   (decals over the eaten parts) and with decay (the body sprite turns
   pale, then grey-green, then bone).
