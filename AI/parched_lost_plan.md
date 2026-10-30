# Parched and lost

Two thirsty humans who know no water keep asking each other for it, and sit
or follow each other while they die of thirst. A parched human should act
like one: stop asking those who already said they know nothing, and stop
sitting about while there is anything left to try.

Found in a test pass on 2026-10-30 (`build/lifelogs 6 25 4 10` in the main
checkout, World seed `s*7919`, `Rng(s)`, crowd 4):

- Seed 2, Goe and Feronstedt, 85-306: "DOES ANYONE KNOW WHERE THERE'S
  WATER?", "I NEED WATER. ANY IDEA?", "WATER... PLEASE..." about 12 times,
  always to each other. Goe is on Wander, mostly sitting, from 240 to 310 at
  thirst 0.95-1; Feronstedt keeps company with it at thirst 1 for 120 s. At
  310 Goe snaps, kills Feronstedt to eat, and dies of thirst at 403.
- Seed 3, Carman, Sley and Hut, 116-354; seed 6, Jubankinsirk and Vaino:
  the same.

## Diagnosis

- **They were boxed in.** Each one logs "LEARNT TO TAKE KEYS FIRST" just
  before it starts sitting (Goe 233, Sley 189, Hut 290, Jubankinsirk 139).
  That line is written only when Explore finds nothing left to explore, a
  locked door is known and it has no key. With no water, no toilet and no
  frontier anywhere it can walk to, every want fails but Wander, which walks
  a few cells and sits (`pursue`, Want::Wander). Nothing it does can find
  water, and nothing tells it to try what is left.
- **It shows up only without the spawn basin.** The main checkout's
  uncommitted work takes the basin out of the spawn room
  (`World::makeSpawn`). On the committed code every spawn room has water,
  and 12 worlds of 15 minutes give no asks at all. With the basin gone, a
  spawn area with locked doors and no keys left has no water at all.
- **The ask has no memory and no answer.** `askGroupForWater` asks the
  nearest one it walks with every 30 s (ASK_WATER_EVERY). Whoever knows
  tells; if nobody does, nothing is said back, and it asks the same one
  again 30 s later, for as long as both live.
- **Company holds them together.** Feronstedt keeps company with Goe
  (Want::Company), so it follows another lost, parched human about instead
  of looking on its own.
- **The snap.** Erratic grows while parched (ERRATIC_PARCHED), so a parched
  human snaps; Goe was also hungry (0.70), so it went for flesh. A meal of
  flesh already takes FLESH_DRINK 0.1 thirst off (thirst plan, "all extras
  in"); that bought Goe about 90 s.

## User's decisions (2026-10-30)

(to be filled in from the questions below)

## Steps

1. **Asked, and knew of none.** When nobody it asks knows of water, the one
   asked says so ("NO. I'M DRY TOO."), and the asker remembers who.
2. **Parched and boxed in, it tries what is left** instead of wandering
   and sitting.
3. **Company lets go of the lost.** (if decided)
4. **The snap and thirst.** (if decided)
5. A scenario: two parched humans boxed in with no water they know.

## Open questions

See the user's decisions above.
