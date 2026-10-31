# Dirty humans, then smell

A human who goes too long without washing turns **dirty**: it looks
ragged, and (next, not built yet) starts to smell. Being dirty for too long
makes a smell form, and smell shows as flies.

The look began as a "feral" look for humans who had eaten many humans; the
user renamed it dirty and gave it to going unwashed instead (2026-10-31).

## User's decisions

- **A ragged look, nothing unnatural** (2026-10-30): no red eyes, no sallow
  or green skin, no painted blood. They look dirty, their clothes torn,
  their hair messed up. Real blood still comes from the blood system, as
  on anyone.
- **Each keeps its own hair, messed up** (2026-10-31): the cap stays on,
  with hair sticking out under it; the bald stay bald; a beard grows
  ragged under the chin.
- **Dirty comes from going unwashed** (2026-10-31): after about **10
  minutes** without washing; blood, feces or piss on it speed it up.
- **Washing for it depends strongly on modesty** (2026-10-31): the modest
  wash as soon as they turn dirty, if nothing more urgent presses. A human
  with no modesty never washes for being only dirty, nor for **dried
  blood**: only for **fresh blood and feces**.
- **Dirty is only the look for now** (2026-10-31): nobody thinks less of a
  dirty human. Smell brings the effects.

## How it works (built 2026-10-31)

`human_grime.cpp`, state in `Human::grime_`:

- **Grime** builds by 1 every `GRIME_TIME` (600 s), twice as fast while
  any blood, feces or piss is on it, up to `GRIME_MAX` (3). At 1 it turns
  dirty: note `TURNED DIRTY`, moment `got_dirty`.
- **Washing for it**: modesty below `MODEST_WASH` (0.3), never; from
  `MODEST_FULL` (0.7) up, as soon as it turns dirty; between, only once the
  grime is up to `GRIME_PUT_UP` (1.5) further on. It is a reason for
  `Want::Wash`, adding up to `GRIME_WASH` by modesty, so the modest put it
  before company and exploring, but not before hunger, thirst, going or
  fear. The erratic never wash for it. Note `WASHED OFF DIRT`.
- **Dried blood**: `startWash` now washes blood off only while some of it
  is still wet, unless modest enough (`washesForLooks()`). Feces as before.
- **Any wash** scrubs the grime off once the blood and feces are off,
  `GRIME_SCRUB` a step; washed down to 0 it is clean again: note
  `WASHED CLEAN`, moment `washed_clean`.
- **Stats**: a GRIME row under BODY.
- **The look**: `HumanSprites::setDirty` redraws every frame from the
  look with `HumanLook::dirty` set, each time the human turns dirty or
  clean. The world scene calls it every frame from `Human::dirty()`.
- **Scenarios**: `dirty-wash` (the modest wash it off, the shameless stay
  dirty), `dry-blood` (only the modest wash dried blood),
  `fresh-blood` (the shameless wash fresh blood).

Note: the brave already put off washing blood (WANTING's Wash row), so a
bold, shameless human often lets fresh blood dry while doing something
else, and then keeps it. That is the old tuning meeting the new rule.

### How the look is drawn

A flag on `HumanLook`, `dirty`. It repaints the **same frames** from the
same skeleton, so every pose keeps its head, hands, feet and pocket in the
same pixels, and nothing that rides on the body (bottle, cake, pointing
arm, blood, feces, remains) needs to know:

- **Dulled before drawing**: hair, clothes, cap and shoes a little darker.
  The skin keeps its own colour.
- **Hair messed up**: each style keeps its shape, with strays and tufts
  off it: short hair sticks up and out, long hair tangles into a mop down
  the neck, spikes go uneven, a bun comes undone, a mohawk flops over, a
  ponytail frays, hair sticks out from under a cap. Bald stays bald.
- **A wear pass** (`HumanLook::wear`), keyed by each pixel's place on the
  standing pose (the same `rest` map blood uses), so it moves with the
  body: smudges of dirt on the skin, hands black with it, and clothes torn
  (about half the pixels at a hem, a few elsewhere) and frayed.
- **No pixel is added or taken off the body's outline** except the hair,
  which belongs to the head. Dirt is the skin in shade, so the remains
  still rot and are eaten like any other body.

### Preview

`make dirtysheet` (tools/dirty_sheet.cpp) writes `build/dirty-sheet.png`:
one human beside itself dirty in every `Pose`, carrying a cake, with the
bottle in the pocket and in the hand, pointing three ways, bloodied, with
feces and piss, and as remains. It draws through `HumanSprites::draw` as
the game does. `DIRTYSHEET_ARGS="seed scale"` picks another human;
`DIRTYSHEET_ARGS="seed scale styles"` draws one row per hair style.

## Next: smell (annotated, not built)

What the user asked for (2026-10-31):

- **Being dirty for too long makes a smell form** on the human.
- **Flies show smell**: a very simple flies animation over smelly cells.
  A smelly human is **constantly followed by flies**.
- **Feces smell until they dry.** Feces **footprints don't**, for
  simplicity.
- **Bloody corpses smell.**
- **Foul water smells** past a certain foulness.

### User's answers on smell (2026-10-31)

1. **When**: a human smells after about **5 more minutes dirty** (grime
   1.5 of 3). Any wash that gets it clean ends the smell **at once**.
2. **What it does**, besides the flies:
   - **Others keep away**: humans avoid walking, standing or sitting near
     smelly cells and smelly humans (the modest more).
   - **Puts off appetite**: they won't stop to eat or sit next to a smell,
     waiting to eat until away from it; and a smelly (rotting) body is less
     tempting to eat, except to the erratic and the starving.
   - Not trust or greeting: no trust cost, no refusing to greet.
3. **Out of sight**: **yes, close by**. Within a few cells, even through
   walls, a human smells it and learns something smells there. This is a
   new way of knowing, alongside seeing and being told, and so finding
   bodies by smell becomes possible.
4. **Bodies**: **any rotting body** smells, bloody or not, until it is bare
   bones; blood on it makes it smell at once.
5. **Foul water** smells from foulness **0.5** (where it sickens).
6. **Flies**: a **swarm by strength**, 1 to 5 one-pixel dots buzzing in
   small loops over the smelly cell or head, drawn in code like the z's of
   sleep. A smelly human's flies move with it.

Also as asked before: feces smell until they dry; feces footprints don't.

### To settle while building (suggested defaults)

- Smell strength 0 to 1 per source: a smelly human by how far past the
  mark its grime is, feces by freshness, a body by rot (and blood), water
  by foulness past 0.5. Flies = 1 + 4 x strength.
- "Close by": smelled within 3 cells, through walls, for the strongest;
  less for faint smells. Remembered in `Memory` as a smell at a cell, with
  when.
- Keeping away: a path cost on cells next to a smell it knows (like
  `messCost`), more for the modest; nobody keeps away from its own smell.

## Open questions on the look

1. **Hunched / claws**: a hunched head or clawed hands would move pixels
   the skeleton uses (the head's place, the hands'), so would need care
   with sipping, the cake and blood. Wanted?
