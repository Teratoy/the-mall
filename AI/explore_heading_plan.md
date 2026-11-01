# Exploring a big lit room

## What was wrong

The user: "the spawn rooms play like shit because the human explore every
single cell in a unnatural way".

`tools/explore_probe.cpp` (`make exploreprobe`, `EXPLOREPROBE_ARGS="seed
seconds"`) follows the waker until it leaves the spawn room and draws its
walk. Exploring makes for the nearest frontier: known floor beside floor it
has never seen. In the spawn room (54 cells across, fully lit) the only
frontier is the rim of its own sight: 20 cells, and only 6 while dazed. So
each step the rim moves, the target is seen, it picks the nearest rim cell
again, and it walks in widening loops round where it woke, chasing the
edge of its own view. On most seeds it saw every one of the ~2,900 floor
cells, changing target 120-160 times, before it came on the door.

## Decided (2026-11-01)

The user picked three of four:

1. **Commit to a heading.** Exploring keeps the way it last set out:
   a frontier against that way costs more to make for (up to
   `HEADING_COST`, right behind), so once its target is seen it carries on
   to a frontier further the same way rather than turning back to the
   nearest. It crosses a room in long walks.
2. **Doors over open floor.** A plain frontier costs `FRONTIER_COST` more
   than a door (open, or shut and untried), so a doorway in view beats a
   nearer strip of unseen floor.
3. **See farther when lit.** `Sight::RADIUS` goes from 20 to 40: lit cells
   are seen that far, so a lit room is taken in at a glance. The dark is
   still only `DARK_SIGHT` (4), and the dazed still see less, from
   `DAZE_SIGHT` up as the daze wears off.

Not taken: keeping the dazed near the bed.

## Built (2026-11-01)

- **Heading** (`heading_`, `headingCost()` in human_walk.cpp): set each time
  it sets out exploring, and through a door. `HEADING_COST` 300. Blocked by
  someone in a one-wide corridor and giving up, an explorer turns its
  heading round, or two of them head-on make for the same way past each
  other forever (seen in the soak).
- **Doors over floor**: `FRONTIER_COST` 150 on a plain frontier.
- **Sight 40, but people only within 20.** `Sight::RADIUS` is 40 and the
  look feeds memory (the place, what lies there) out to it through
  `forEach`, and the view draws it (`inView`). `sees()`, what every system
  asks about someone, how water looks and so on, answers only within
  `Sight::NEAR` (20), as before. Seeing people 40 off had everyone walking to
  meet everyone and jamming corridors: give-ups in the soak went 32 → 225.
- **Walls seen by the lit floor before them.** The spawn room's walls
  between two fittings are dark; unseen, each was a frontier, and the
  human walked up to one every 8 cells along every wall.
- **A door check keeps going.** Walking to check a door, it wants to go on
  checking without a new roll of `CHECK_DOOR_CHANCE` at every rethink.
  With a stranger in sight it rethinks every step, and it flipped between
  Wander and CheckDoor twice a second.

`make exploreprobe`, spawn room, seeds 1-8, before → after:

| | before | after |
|---|---|---|
| seconds to leave (mean) | 51.5 | 40.4 |
| steps walked (mean) | 151 | 107 |
| explore targets (mean) | 104 | 67 |

It now walks in long straight lines. What is left is the daze: sight
starts at `DAZE_SIGHT` (6) and only reaches 40 as the daze wears off over
40 s, so the first half-minute still goes round the room. Keeping the dazed
near where they woke (not taken) is the lever for that.

Sim time: 0.054 → 0.075 ms a tick (`make simprofile`).

Soak: no loops, no long blocks, mutual blocks 1 → 1, give-ups 32 → 43,
stills 13 → 18 (all humans stood talking). Seed 1 no longer gets out of the
spawn chunk: a shunned, bloody human stands by its only way out and the
one with the key keeps backing off from them, so three die of thirst.

Scenarios: dark-sight now puts the human back where it woke (it leaves the
room before 45 s now) and its near cake within dark sight; hum-lobby runs
90 s. Two are marked known for the user: fresh-blood (the shameless one
gets to the basin only at 60 s) and call-follow (the hearer already sees
all round the voice, so it never needs to walk toward it).
`SCENARIO_TRACE=1 build/scenarios NAME` prints each human's state and goal
as they change.
