# Trap wariness plan

A human hit by a trap should come away wary of the next one like it, not
only of that one cell. Today a human darted three times in a row goes for
a fourth cake at a fifth of its health, while barely hungry, because what
it learns is just "this plate".

## What the test pass saw, and what it turned out to be

The obliviousness test pass (main with uncommitted work, crowd 4) saw:
- Seed 6, Iza: darts at 1133, 1147, 1155 and 1273, all fetching cakes.
  Health 0.80, 0.47, 0.05, then at 0.22 it went for a cake and fell to
  0.07. Hunger only 0.27 / 0.08.
- Seed 5, Warbacurn: slammed in at 1131 and 1164 (let itself out with
  keys), then at 1187 with no key left, shut in to the end.
- Seed 3, Tac: spotted spikes at 174, told of a trap at 185, spiked at 215
  picking up a key, bled to death.
- Seed 3, Granter darted 4 times; seed 4, Lamarke darted a second after
  selling news of a trap.

`tools/trap_probe.cpp` (`make trapprobe`, `TRAPPROBE_ARGS="seeds minutes
crowd"`) logs every trap set off with its cell, whether the human knew that
cell, how many traps of that kind it knew, and its health and hunger.

On this branch, 30 worlds x 25 minutes, crowd 4: **204 traps set off, none
on a cell the human knew**, except one starving human going for a dart
cake it knew of, which is by design (`avoids`: the starving brave darts).
Repeats are all new plates of the same kind, almost all darts. The worlds
of the test pass came from main's uncommitted world changes, so those
exact runs (Tac's among them) can't be replayed here; no same-cell repeat
showed up in any run here, so there is no avoidance bug to fix. The
problem is how a hit generalises.

Why it keeps going: `Want::Fetch` collects cakes up to `carryMax()` even
when not hungry, and nothing in it weighs health or past hits. Any cake on
a table may be a dart plate, and a cake at the far end of a small room
behind one door may be slam bait.

## User's decisions (2026-10-30)

1. **Darts: wary of all table cakes** for a while. It leaves them alone
   unless hunger outweighs the wariness, and being hurt makes it warier.
   Each volley adds to it, and it fades over minutes.
2. **Slam: a key for it.** It takes no bait in a small room with one door
   (judged from what it has seen) unless it carries a key to let itself
   out.
3. **Starving always risks it**, as starving humans already brave a dart
   plate they know of.
4. **A trusted tip is safe**: a cake that someone it trusts told it of
   skips the wariness, so a rigged tip from a "friend" hurts more.
5. **Seeing someone else darted** makes it wary too, a little less than
   being hit itself (user, 2026-10-31).

## Steps

1. **Wariness state.** A per-kind wariness (darts, slam), 0 to 1, raised by
   each hit of that kind and fading over time like lessons do. Not part of
   `learn()` (another session is changing its logging).
2. **Darts.** While wary, a cake on a table it hasn't seen safely taken is
   weighed: hunger against wariness, more so when hurt.
3. **Slam.** A cake or key in a small room behind a single door, judged
   from memory (a bounded flood fill from the table that stops at doors),
   is weighed the same way, or needs a key in hand.
4. **Log and talk.** A `note()` line when it first turns away from a table
   out of wariness, and a `sayLater()` moment with lines.
5. **Scenarios.** Darted once, a human passes over the next table cake
   when not hungry; starving, it still takes it. Slammed once, it passes
   over the next slam bait.

### Built (2026-10-30)

All five steps, in one go.
- `Human::Wary` (`wary_` in human.h): `darts` and `slam`, each +0.5
  (`WARY_HIT`) per own hit in `learnTrap`. Darts fade over ten minutes
  from full (`WARY_DARTS_FADE`), slams like lessons (`LESSON_FADE`, half an
  hour). The fading sits beside the lessons' in `update`; `learn()` is
  untouched.
- `shyOf(c)` (human_fight.cpp) says why it leaves a cake alone. Starving,
  or a tip from someone trusted at `WARY_TIP_TRUST` (0.3) or more: no
  wariness. Slam: a key-less human, wary, with the cake in a
  `oneDoorRoom` (a flood fill over memory from the table, stopped by
  walls and doors; any unknown cell or more than `WARY_ROOM_MAX` cells
  means it can't tell; side-by-side door cells are one door). Darts: a
  table cake while hunger < wary × (1 + 0.8 × health lost). One volley:
  fetching only when hungry (0.5) at full health, only when starving if
  badly hurt; two volleys: only starving.
- `cakeInReach` takes the place of `inReach(…, memory.cakes)` in
  `seekFood`, `Want::Fetch` and the FetchCake rethink, and `takeInReach`
  doesn't lift a cake it's shy of in passing.
- `noteShy`: "LEFT A CAKE ALONE. IT COULD BE RIGGED" / "LEFT THE CAKE IN
  A ROOM WITH ONE DOOR. NOT WITHOUT A KEY", once after each hit, with
  moments `WaryCake` / `WarySlam` (`[wary_cake]`, `[wary_slam]` in
  data/lines.txt).
- Scenarios: `wary-darts`, `wary-starving`, `wary-tip`, `wary-slam`,
  `wary-slam-key`. `SoakProbe::setWary` sets wariness directly.
- Soak: seeds 4 and 6 changed; stuck, loop and block counts the same.
- 2026-10-31: a volley seen going off adds `WARY_SAW` (0.35) to darts
  wariness, against 0.5 for its own; one seen leaves it fetching table
  cakes from hunger 0.35 at full health. Scenario `wary-saw`.

## Open questions

- **For later (user, 2026-10-31):** should seeing a slam room shut on
  someone else make a human wary of one-door rooms too, as seeing darts
  does? Not decided; for now only its own slams count. If built: in
  `learnTrap`, a seen slam (`!mine`) would raise `wary_.slam` too, but
  `slam` is an on/off wariness (any amount above 0 asks for a key), so
  "a little less" would have to mean it fades sooner, not that it is
  weaker.
- Wariness doesn't touch the cakes humans fight over or loot from bodies,
  only cakes on tables.
