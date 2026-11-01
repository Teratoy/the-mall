# Trying doors

Humans used to know a shut door was locked the moment they saw it, and
which side of an airlock door gives at a push. Now a shut door is only a
shut door until they walk up and try the handle.

In the world every shut door is in fact locked: an exit door, once
unlocked, stays open for good, and the airlock doors and slam gates are
the only ones that shut. So trying never opens a plain door. It costs a
walk, and it teaches.

## User's decisions (2026-11-01)

- Q1 What a shut door hides: every shut door stays locked, but a human
  has to try one before it counts it as locked, until the lesson is
  learnt and it stops trying.
- Q2 Airlocks: which side gives is learnt by trying it. From inside it
  pushes open; from outside it is locked like any other.
- Q3 Told: a door someone told it the way to is known to be locked,
  untried.

## What was built

- `doors_` (`Human::Doors`): the sides it has tried each shut door from,
  the doors it was told of, how many it has found locked, and whether it
  has learnt.
- A shut door is known locked once tried from any side, told of, or once
  the lesson is learnt (`doorKnownLocked`). Untried, it is worth walking
  up to like the edge of what it knows: exploring makes for it (with the
  door hunch), and the ones that were explored past stay on the list.
- Come to an untried shut door exploring, or checking a door it knows,
  it tries it (one it makes for with a key to open, it just opens):
  - Locked: "TRIED A DOOR: LOCKED", `TryDoorLocked`.
  - An airlock door from its inside: it gives, "PUSHED A DOOR OPEN",
    `DoorGave`, and from then on it knows that door pushes from there
    (`memory.pushes`). That a door is an airlock's, and which way is in,
    is still seen (`memory.airlocks`), so the double doors are known for
    what they are; only that one gives from inside has to be felt.
- The lesson: after `DOOR_LESSON` (3) locked doors, "LEARNT THAT SHUT
  DOORS ARE LOCKED", `LearnDoors`. After that it counts every shut door
  locked and stops going to try them.
- Boxed in, though: with nowhere left to explore, it tries the
  shut doors it knows from any side it hasn't tried them from, lesson or
  not, before it gives up at a dead end. That is how one learnt and shut
  in a server room still finds its way out.
- Told: a route someone tells it ending at a shut door marks that door
  known locked, and only doors the teller knows locked are told.

## Defaults chosen (not asked)

- The lesson takes 3 doors and does not fade.
- Trying the handle is instant on arrival; the walk is the cost.

## Tests

Scenarios `try-door`, `learn-doors`, `airlock-push`, `told-door`.
`knowChunk` in `tests/probe.h` now counts the chunk's plain doors tried,
as a human that explored it would have. `airlock-surprise` checks the
human never stays in the lobby long, not where it stands at 40 s: trying
the doors from inside makes it leave a moment later.

The soak (2026-11-01): no loops or long blocks, fewer humans standing
still (16 to 13), a little less OpenDoor time, and 26 chunks reached
over the six worlds instead of 30, the walks to try doors taking time.

## Open questions

None.
