# Beating its head against the wall

The user asked: "stuck erratic humans will try killing themselves by hitting
a wall with their head. animate and implement it".

## What it ties into

- The erratic mind: `isErratic()` in `human_flesh.cpp` (AI/cannibalism_plan.md).
- Shut in: `rescue_.slamGate` (a slam room, no key) and `rescue_.callDoor`
  (parched, calling through a locked door) in `human_rescue.cpp`; banging on
  the door (AI/shut_in_plan.md) is the template: a wish taken as a reflex in
  `choose()`, a walk to the spot, a state with blows on a timer, a noise per
  blow heard through walls.
- Hope: `gloom()` and settling in `human_hope.cpp` (AI/escape_plan.md).
- Wounds and blood: `wound()`, `gore_.sprays` (a spray toward the wall lands
  on its face), `takeHit`'s health loss, bleeding to death in `update()`.

## User's decisions (2026-11-02)

1. **Who is stuck:** erratic, and either shut in with no key (a slam room, or
   parched and boxed in calling through a locked door), or hopeless (gloomy:
   hope under the gloom bar, settled or not), even when it could walk away.
2. **When:** only after being stuck a while (about 2 minutes), then a chance
   every so often, higher the gloomier it is.
3. **How it ends:** in bouts. A bout is a few blows, each a bleeding head
   wound with blood on the wall. It stops dazed, then goes again later.
   Usually it dies over a few bouts; a rescue in between can save it.
4. **Heard:** each blow is a thud. Hearers out of sight take it as someone
   shut in, as they take banging, when it is shut in.

## Defaults chosen (to confirm)

- Stuck for `HEADBANG_AFTER` 120 s straight, then a roll every 15 s:
  8% + 25% x gloom, +7% shut in; once it has had a bout, at least 50% (it
  goes on with it). After a bout, no roll for 25 to 50 s.
- Only rolled when it is free (idle, walking, sitting; not asleep, fighting,
  fleeing or busy).
- The wall: the nearest floor it knows, within 25 steps, with a wall to the
  east or west (the pose is side-on), else none and no bout.
- A bout: 3 to 5 blows, one every 0.8 s, the head drawn back, then slammed
  in. Each blow takes 0.06 to 0.10 health at once (as a punch does) and opens
  or deepens a blunt head wound half as deep again (its own, so nobody is
  credited with the kill), spraying blood and leaving a splat on the wall's
  face. A blow that takes the last health kills it there. It dies over 1 to
  3 bouts; the open wound keeps it from healing between them.
- After a bout it sits down dazed (sight shrinks, as on waking).
- The wish to go at a wall is kept up to 10 s while it knows no wall beside
  floor within reach yet (just woken in the fog, say), then let go.
- Out of sight within 16 cells: shut in, heard as banging on its gate;
  otherwise "HEARD SOMETHING POUNDING ON A WALL" and a little dread, the
  first time from each.
- Seen: "SAW @ BEATING THEIR HEAD AGAINST THE WALL", dread, a moment.
- Costs: hunger 1/400, thirst 1/320, tiredness 1/600 a second.
- The HUD: "BEATING THEIR HEAD ON THE WALL", walking "GOING TO A WALL".
- The death log stays "BLED TO DEATH" when the bleeding ends it; a last blow
  logs "BEAT MY HEAD ON THE WALL ONE LAST TIME".

## Steps

1. State `headbang_`, `State::Headbanging`, `Action::Headbang`,
   `Goal::Headbang`, the roll and the wish, the walk, the bout, wounds,
   noise, hearing and seeing, notes and moments.
2. Art: `Pose::HeadbuttBack` and `Pose::HeadbuttHit` from a new
   `HumanLook::headbutt(frame)`: the front hand flat on the wall, the head
   drawn back; then the head thrust ahead and down into it, eyes shut.
3. A `thud` sound; moments `Headbang`, `SawHeadbang`, `HearThud`.
4. Scenarios: an erratic human shut in bangs its head and dies; a hopeless
   one does; a watcher sees it.

## Built (2026-11-02)

All four steps, in `src/human/human_headbang.cpp` (state in `headbang_`).

- `weighHeadbang()` each update: the stuck clock and the roll, which sets
  `headbang_.want`. `goAtWall()` in `choose()`, after banging on a door,
  starts a bout beside a side wall or walks to one (`Goal::Headbang`).
- `State::Headbanging`, `Action::Headbang`; `headbanging()` runs the blows,
  `headbutt()` is one; broken off by a hit (as any state is) or when no
  longer stuck. Then `sitDown()` dazed.
- Seen and heard at the start of each bout: `sawHeadbang()`,
  `hearThud()`, or `hearHelp(..., true)` when shut in.
- `NoiseKind::Thud` and a `thud` sound in `data/sounds.txt`.
- Art: `HumanLook::headbutt(0|1)` (`Arm::Brace`, the head leaning -1 and
  +2), `Pose::HeadbuttBack` and `Pose::HeadbuttHit`; in `make dirtysheet`.
  `make shot SHOT_ARGS="--headbang --seconds 25.5 --frames 260"` films a bout.
- Log: "BEAT MY HEAD AGAINST THE WALL", "BEAT MY HEAD ON THE WALL ONE LAST
  TIME", "SAW @ BEATING THEIR HEAD AGAINST THE WALL", "HEARD SOMETHING
  POUNDING ON A WALL". Moments `Headbang`, `SawHeadbang`, `HearThud`, with
  lines and talk_flow sections.
- Scenarios: `headbang-shut-in` (3 bouts, dies; a boxed-in hearer hears it as
  banging), `headbang-hopeless` (a watcher boxed in with it sees it; one
  boxed elsewhere hears thuds), `headbang-sane` (not erratic: never).
- Soak: 9 bouts in 6 worlds, 4 deaths from it (seeds 1, 5, 6); nothing new
  stuck, no new loops or flips.

## Stepping in (2026-11-02)

The user asked: "caring humans can stop people from doing it and have a
chance of calming people, making them snap out of erratic state and gain a
very little amount of hope".

Defaults chosen (to confirm):
- **Who steps in:** anyone who sees a bout start and cares, as for the shut
  in (not erratic, greed under 0.67), and isn't its foe (trust above -0.2)
  or fighting it, with a way to it no longer than 20 steps. It goes to them
  (`Goal::Calm`, "GOING TO STOP @"). Every bout seen is a new chance.
- **Stopping:** reaching them while they beat their head, or sit dazed after
  a bout (up to 20 s after), ends the bout: they are held back and sit, and
  the carer stands over them, a hand on their shoulder, talking them down
  for 4 s (`State::Calming`, `Action::Calm`, `Pose::Comfort`). Calming costs
  as greeting does.
- **Calming:** at the end, a chance of 25%, +40% x how much they trust the
  carer, +20% x how sociable the carer is. Calmed: their mind drops well
  below erratic (to 0.2, held up by any craving for flesh, so cravers stay
  erratic), hope rises 0.05, and they trust the carer more (+0.2). Not
  calmed: the bout is still stopped, and they go on as before.
- Log: "STOPPED @ BEATING THEIR HEAD AGAINST THE WALL", "TALKED @ DOWN",
  "COULDN'T GET THROUGH TO @"; held: "@ STOPPED ME", "@ TALKED ME DOWN.
  FEEL MORE LIKE MYSELF". Moments `Calm` (the carer, to them), `CalmedDown`
  and `NotCalmed` (them, to the carer).

Built (2026-11-02): `wantCalm`, `stepIn`, `startCalming`, `calmingDown`,
`heldBack` in `human_headbang.cpp`, `calmedBy` in `human_flesh.cpp`;
`HumanLook::comfort()` (the front hand out at the shoulder). Scenarios
`headbang-calm` (a caring friend boxed in with it stops it at once, fails
the first time, talks it down on a later bout) and `headbang-greedy-watch`
(a greedy watcher never steps in). In the soak no carer happens to see a
bout, so `Calm` and `Calming` are never seen there.

## Open questions

- Should it ever survive and stop for good (beyond being talked down)?
