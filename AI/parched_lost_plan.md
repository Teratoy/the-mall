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
- **Since bottles (merged into main the same day)** everyone wakes with a
  full bottle, asks those it sees carrying water for a drink, and fights for
  it. On main the long loop is gone: in 6 worlds of 15 minutes each pair asks
  once or twice. What is left is the pair asking each other back and forth
  (Goe and Feronstedt 12 s apart, each having just shown it knows of none),
  and, once the bottles are dry, the same boxed-in end.

## User's decisions (2026-10-30)

- **Asking again:** whoever is asked and knows of no water says so out
  loud, and the asker remembers it. It asks them again only once they have
  been out of its sight, since they may have found water meanwhile.
- **Boxed in and parched, with nothing to drink it knows of:** it mostly
  sits, and lies down (awake, not sleeping), and calls through a locked
  door for help, as one shut in a slam room does. Anyone in earshot with a
  key may come and open it.
- **Company with one as lost:** whether a parched human leaves a companion
  who is parched too and knows of no water depends on its traits.
- **The snap:** the erratic go for flesh only when hungry, not by default
  (appetite no longer pulls the bar below hungry). An erratic who is only
  parched attacks, at sight, anyone carrying a bottle, even an empty one.
  Hungry, it chases flesh to eat.

## Steps

1. **Asked, and knew of none.** When nobody it asks knows of water, the one
   asked says so (`talk::Moment::NoWater`, "NO. I'M DRY TOO."), and the
   asker keeps them in `water_.knewNone`; the one asked keeps the asker
   too, since it plainly knows of none either. Whoever it has not seen
   since is let go of the list, to be asked again. It asks the nearest
   not on the list; with everyone on it, it asks nobody.
2. **Parched and boxed in.** Parched, with no key, nothing to drink it
   knows of and nowhere left to explore, Wander gives way to languishing:
   it walks to the nearest locked door it knows, then lies down there
   (awake, eyes open) or sits, a while at a time. While there and
   parched, it calls through the door now and then (`Moment::CallDoor`);
   anyone within the slam room's HELP_RADIUS hears it (`hearHelp`) and a
   key holder may come and open it.
3. **Company by traits.** Parched, it leaves a companion who is parched
   too and has said they know of no water, unless sociable enough (from
   STAY_LOST_SOCIABLE; a friend counts for more). It doesn't pick them
   again while it remembers they know of none. Logged "LEFT @. THEY KNOW
   NO MORE THAN I DO", said as `Moment::PartDry`.
4. **The snap.** The erratic flesh and hunt bars no longer drop below
   HUNGRY. Erratic, parched, not hungry and with nothing of its own to
   drink, it goes for anyone it sees carrying a bottle, full or empty
   (`Moment::BottleRage`), and takes it with a blow when it has room.
5. **Scenarios:** two parched humans boxed in with empty bottles ask each
   other at most once, then lie and call through a door; an erratic,
   parched human goes for an empty bottle.

## Build order

1, then 2 and 3, then 4, then the scenarios; `make check` and golden once.

### Built (2026-10-30, all steps)

- **Step 1** (`human_thirst.cpp`): `water_.knewNone` maps who knows of no
  water to when it last saw them (`keepTrackOfDry`, every frame); out of
  sight APART_TIME 20 s, they may be asked again. The answer comes when the
  asking is said (`answerWater`, from `say()` in human_speech.cpp, as for
  AskWay and AskDrink): whoever knows has already told; else the one asked
  says `NoWater`. Both keep the other on the list (the asker plainly knows
  of none either). An ask still queued to one who has since shown they know
  of none goes unsaid, so two asking each other at once ask once. Log:
  "ASKED @ WHERE THERE'S WATER", "@ KNEW OF NO WATER EITHER".
- **Step 2** (`languish`, called first in Want::Wander): parched, keyless,
  nothing to explore. It walks to the nearest locked door (Goal::CheckDoor)
  and there lies down (half the time, LIE_TIME 15–30 s) or sits. "NO WATER
  ANYWHERE. LYING BY A LOCKED DOOR, CALLING FOR HELP". Lying is Sitting
  with `water_.lying`, drawn as the new `Pose::Lie` (on its back, blinking
  as one seated does), "LYING DOWN" in the HUD. `rescue_.callDoor` and
  `callThroughDoor` (human_rescue.cpp) call `Moment::CallDoor` every 20–40
  s (as a slam room cry), heard through walls within HELP_RADIUS by those
  who can't see the caller (those in there with it just hear the line).
  Hearers weigh coming as for a slam room; `letOut` counts a caller whose
  door is now open, whichever wall's cell of it was unlocked: "OPENED THE
  DOOR @ WAS CALLING THROUGH", "@ LET ME OUT".
- **Step 3** (`leavesLost`, human_company.cpp): stays if sociable + 0.3 ×
  timidity (+0.3 for a friend) reaches 0.8 (STAY_LOST); else leaves and
  doesn't pick them again while it remembers they know of none. "LEFT @.
  THEY KNOW NO MORE THAN I DO", `Moment::PartDry`.
- **Step 4**: `fleshBar` and `huntBar` never drop below HUNGRY for the
  erratic. `bottleRaging` (erratic, parched, not hungry, nothing to sip,
  room for a bottle) makes a reflex in `choose()` (`bottleRage`): it goes
  for the nearest it sees carrying any bottle, "WENT FOR @'S BOTTLE, FULL OR
  NOT", `Moment::BottleRage`; `startFight` keeps after them and `landPunch`
  takes an empty bottle too.
- **Scenarios**: parched-lost, parched-door, parched-leave, bottle-rage.
- **Tool**: `tools/lifelog.cpp` (`build/lifelog seed [minutes] [crowd]
  [name] [every]`) prints a soak world's logs, and every few seconds one
  human's goal, state, needs and whether it knows a frontier, a locked
  door, water or a toilet. That is how the boxed-in diagnosis was made.
- **Soak**: thirst deaths 2 → 6 over the six worlds, kills to eat 14 → 9.
  With erratic humans no longer hunting before hungry, fewer die by murder,
  and in dry worlds (seed 4 has no water anyone finds) more die of thirst;
  none of the new thirst deaths were languishing. One new 30 s still (a
  crowd of seven greeting at once, seed 5) and one target flip (seed 5, a
  hoarder going back and forth between Stash and Explore while moving its
  stash; stash code, not this change).

## Open questions

- Should a parched human with a key, boxed in only by doors it could
  open, ever hold back? It opens them as before.
