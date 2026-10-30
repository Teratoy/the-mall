# Feral humans

A human who has eaten many humans turns **feral**: it looks it. This plan
covers the art only, as asked. When a human turns feral and what that
changes in how it acts is for later (see the cannibalism plan), and is not
built.

## What was asked (2026-10-30)

A feral "normal human" that overlaps correctly with everything a normal
human does: the bottle in the pocket and in the hand, the cake in the
hand, pointing, the skeleton, and where wounds show.

## User's decisions (2026-10-30)

- **A ragged look, nothing unnatural.** No red eyes, no sallow or green
  skin, no painted blood. They look dirty, their clothes torn, their hair
  messed up. (A first version had red eyes, a bloodied chin, grey-green
  skin and bare feet; dropped.) Real blood still comes from the blood
  system, as on anyone.

## How it is drawn

A flag on `HumanLook`, `feral`. It repaints the **same frames** from the
same skeleton, so every pose keeps its head, hands, feet and pocket in the
same pixels, and nothing that rides on the body needs to know:

- **Dulled before drawing**: hair, clothes and shoes a little darker with
  dirt. The skin keeps its own colour.
- **Hair messed up**: `Hair::Matted`, whatever the human's own style was.
  Tufts sticking up, a clump over the brow, hanks down the back of the
  head to the neck with strays off them. It is never rolled, and comes
  after the rolled styles so `HumanLook::random` draws the same dice.
- **A wear pass** (`HumanLook::wear`) over the finished frame, keyed by
  each pixel's place on the standing pose (the same `rest` map blood
  uses), so it moves with the body from pose to pose:
  - smudges of dirt on the skin, and the hands black with it;
  - clothes torn: at a hem (the shirt's bottom, sleeve ends, trouser
    cuffs) about half the pixels torn through to the skin, elsewhere a
    few; some more frayed darker. Which pixels is fixed per human.
- **No pixel is added or taken off the body's outline** except the hair,
  which belongs to the head as hair always has. So `BodyBlood::parts`,
  the bottle spots, the cake and the pointing arm are untouched.
- **Remains**: dirt is the skin in shade, so a feral body rots and is
  eaten like any other (the rot still finds its skin).

The sim never sets `feral` yet, so the soak can't move.

## Preview

`make feralsheet` (tools/feral_sheet.cpp) writes `build/feral-sheet.png`:
one human beside itself feral in every `Pose`, carrying a cake, with the
bottle in the pocket and in the hand, pointing three ways, bloodied, with
feces and piss, and as remains eaten, rotten, bones and curled. It draws
through `HumanSprites::draw` as the game does. `FERALSHEET_ARGS="seed
scale"` picks another human.

## Open questions for the user

1. **All at once or by degrees**: a flag (feral or not), or a gradual
   change with every human eaten (first the hair, then dirt, then rags)?
2. **Own hair**: should it keep something of its own style (a bald human
   stays bald, a cap stays on), or always grow the same matted mop?
3. **Hunched / claws**: a hunched head or clawed hands would move pixels
   the skeleton uses (the head's place, the hands'), and so need care with
   sipping, the cake and blood. Wanted?

## Built (2026-10-30)

The flag, the colours, the matted hair, the wear pass and the preview tool.
Then toned down to the ragged look the user asked for.
