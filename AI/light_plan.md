# Light and light sources

The Mall is evenly lit everywhere: every cell in sight is drawn at full
brightness, everything out of sight that the human remembers at
`MEMORY_DIM`. Light should make **places feel different and some places
feel wrong**: the flat white of a corridor under strip lights, a plaza
with half its ceiling dead, a server room lit only by its racks' LEDs and
a terminal's glow, a vending machine humming light onto the floor in
front of it, a dark room you can see into only as far as your own feet.
And if the user wants it in the sim, darkness should change what humans
can see, where they sleep, what they fear and where they hide.

It ties into what is already there:
- `Sight` (recursive shadowcasting, RADIUS 20, less for the dazed) and
  `Memory`: humans know only what they've seen.
- The view's in-sight / remembered drawing in `render/world_view.cpp`
  (`dim(c, lit)`, `SIGHT_FADE`), and every `*_art` that takes `lit`.
  Note that `lit` there means **in sight**, not lit by a lamp; that name
  has to change before light goes in (step 1).
- Things that already glow in their art: vending machines' LEDs, CRT
  terminals and console desks, server racks' lamps, water's glint.
- Sleep (where to sleep, robbing sleepers, keeping watch), dread and
  fright, hiding and privacy (relief's private spots), traps, cakes on
  tables, smell (sniffed through walls), sound (heard through walls).
- Statues: the giant desk lamp and light bulb are already in the world.
- The phone canvas (216x468, 6 px cells) and the wide PC layout.

## Ground rules (from CLAUDE.md, not up for debate)

- **Deterministic.** Whatever light decides goes through the world's Rng
  or a human's `rng_`; flicker drawn only in the view may use the clock.
- **Humans know only what they've seen.** If light is in the sim, a human
  learns a room is dark by seeing (or not seeing) into it, and remembers
  it in `Memory`; nobody reads the world's light map to plan.
- **Every new behaviour** gets a `note()` and a `sayLater()` moment with
  lines in `data/lines.txt`, never stating the obvious ("IT'S DARK").
- **Art**: pixel art, 6 px cells. Light is drawn in steps (bands, ordered
  dither), never a smooth gradient that smears the pixels. Light glyphs
  (ceiling fittings, a torch item) go in `data/sprites.txt`.
- **A new `Human::Action`** (switching a light, say) needs a row in the
  hunger, thirst and tiredness tables.

## User's decisions (2026-11-01)

- **Light limits sight, and humans change it** (A3): a human sees a cell
  only if it is lit, or close to its eyes in the dark; wall switches and
  carried lights (a torch) are in.
- **Sources:** ceiling fittings, glowing machines, dark wings, and the
  giant desk lamp and light bulb statues glow.
- **A mall clock:** the whole mall goes through opening and closing on a
  cycle. A **bell rings each hour**, and **another bell rings at opening
  and at closing time**.
- **The player sees what the human sees** in the dark: no more.
- **Darkness does all four:** fear, sleep, hiding and blind trouble (E).
- **A mall day is 20 minutes** (`MACHINE_DAY`, 1200 s), **a quarter of
  it closed**. Closed is **dim, not black**: a few night lights stay on,
  and machines, glowing statues and torches still give light.
- **Humans hear the bells and react**: lines when they ring; closing
  pushes toward sleep and opening wakes them, with hope or dread by
  trait. Bells are in the sim.
- **The time is in the HUD**, to the right of the humans and chunk
  counters, laid out from measured text in the top strip.

Defaults taken from those (tell me to change them): 24 mall hours of
50 s each, so the hour bell rings every 50 s; open 06:00–24:00, closed
00:00–06:00; the world wakes at opening (06:00) on day 1.

## Options that were put to the user

Kept for the reasoning; the answers are under "User's decisions".

**A. Sim or look only?**
1. *Look only*: light is drawn, nothing in the sim reads it. `make check`
   unchanged. Cheapest, no new behaviour.
2. *Light limits sight* (recommended): a human sees a cell in line of
   sight only if it is lit, or within a few cells of its own eyes in the
   dark. Lit things are seen from far across a dark plaza; a human in the
   dark is seen only close. Behaviour follows from that (step 5).
3. As 2, and **humans can change light** (switches, carried lights).

**B. Where light comes from.** Candidates, any mix:
- Ceiling fittings over corridors and rooms (strip lights), placed by the
  generators per room, some dead, some flickering.
- Glowing machines: vending fronts, CRT screens, console desks, server
  rack LEDs, water dispensers.
- Whole dark chunks or districts: a dead wing, a blacked-out maze.
- Light that leaks through open doors from a lit room into a dark one.
- Carried light (a torch, a lighter) as a new item.

**C. Does it change over time?**
- Static: a place is lit or dark for good.
- Failing: lights die one by one (world Rng), now and then one comes back.
- A mall clock: the whole mall dims on a long cycle (closing time), tied
  to `MACHINE_DAY` (1200 s) or its own length; sleep could follow it.

**D. How it looks.** Mall fluorescent white-green under fittings; warm
amber round vending machines; CRT blue-green; deep blue-black in the
dark. In the dark, remembered cells and unlit seen cells must still read
differently from each other.

**E. What darkness does to humans** (only with A2/A3):
- Fear: dark rooms add dread, more for the timid, less for the brave.
- Sleep: the tired prefer a dark corner to sleep in (harder to find and
  rob); the timid prefer a light one.
- Hiding: relief's private spots, hoarders' stashes and the cunning
  waiting in the dark.
- Trouble: walking into an unseen trap or stepping in mess in the dark;
  missing a cake on a table across a dark room.
- Following light: lost and parched humans drift toward lit places.

## Steps

### 1. Rename and light map
- Rename the view's `lit` (in sight) to `inSight` across `render/`, a
  pure refactor: `make check` unchanged.
- `src/world/light.h/.cpp`: light sources `{cell, kind, radius, colour}`
  owned by the world, and a per-chunk light level per cell (0–3 steps),
  computed by shadowcasting from each source (reusing `Sight`'s cast) when
  a chunk is made, and again only where a source changes. Walls block it;
  open doors let it through, shut ones don't.
- Nothing reads it yet: `make check` unchanged.

### Built (2026-11-01)
- The rename: `lit` (in sight) is `inSight` through `render/` and the
  sheet tools, and `CellView::Lit` is `CellView::InSight`. Comments where
  lit meant shading (a rim lit on its top) or a lamp being on stay.
- `src/world/shadowcast.h`: the shadowcasting `Sight` did, as a template
  over what a cell holds and what to do with each cell reached. `Sight`
  and light both use it; `make check` unchanged.
- `src/world/light.h/.cpp`: `LightKind` (fitting, night light, vending,
  screen, rack, dispenser, statue, torch), a radius per kind (fitting 7,
  statue 6, torch 5, vending and night light 3, screen and rack 2,
  dispenser 1), `LIGHT_FULL` 3 and `LIGHT_REACH` 8. A cell's level falls
  from 3 on the source in steps to 1 at the edge of its reach; the
  brightest source wins, and its kind is kept for the tint in step 3.
- Each `Chunk` keeps `light` and `lightFrom` per cell. The world keeps
  the sources (`lights()`, indexed by slot), and `World::light(cell)` and
  `lightFrom(cell)`. In `src/world/lighting.cpp`.
- Sources put up with no dice, so generation is unchanged: vending
  machines, terminals and the console desk (screens), racks, dispensers,
  and the giant desk lamp and light bulb statues. Fittings and night
  lights wait for step 2.
- A chunk is relit from scratch, from every source within reach, when it
  is made, when a chunk is made beside it, and when a door near it opens
  or shuts (`World::setTile`, only where a tile turns opaque or clear).
- `make lightprobe` (`tools/light_probe.cpp`): grows the slots round a
  spawn and prints the sources by kind, how much floor is lit at each
  level, and an ASCII map round the waker or a source
  (`LIGHTPROBE_ARGS="seed width height source"`). Seed 1: 14 sources in
  8 chunks; without fittings 99.7% of floor is dark.
- `make simprofile` (seed 4, 15 min, crowd 6): 1.79 s.

### 2. Sources in generation
- Ceiling fittings placed by `gen_rooms`, `gen_plaza`, `gen_maze` and the
  rest, at regular spacing, from the chunk's own Rng: a share dead, a
  share flickering. Some chunks or districts dark all through.
- Machines are sources: vending, terminals, consoles, racks, dispensers.
- This changes generation's dice, so the soak's worlds change: golden.

### Built (2026-11-01)
- Changed from the plan: fittings roll **dice of their own** (seeded by
  the world's seed and the chunk's id), so the rest of generation and the
  soak are unchanged; `make check` passes as it was.
- `World::hangFittings` (`lighting.cpp`): a grid 8 cells apart from a
  random offset, each point snapped to floor within 2 cells, none closer
  than 4; 15% dead (kept as sources that are off), 8% of the rest
  flicker (a flag only: the view doesn't make them stutter yet); 12% of
  chunks dark all through, never the spawn; 2 night lights per slot;
  none in server rooms, which only their machines light.
- The spawn chunk is lit too (`makeSpawn` calls `lightUp`).
- `make lightprobe` seed 1: 916 fittings, 26 night lights over 8 chunks;
  floor 13% dark, 8% level 1, 47% level 2, 32% level 3.

### 3. Drawing light
- A light layer over the world view: per pixel, the cell's level
  (interpolated in steps between neighbouring cells, ordered dither like
  `SIGHT_FADE`) multiplies the colours, with the source's tint.
- Fittings drawn on the walls' tops/ceiling as glyphs from
  `data/sprites.txt`; flickering ones flicker in the view only.
- Remembered cells stay at `MEMORY_DIM`, desaturated, so memory and dark
  never look alike. `make shot --scene` gets a dark-room scene; a sheet
  (`make lightsheet`) shows each source and level.

### Built (2026-11-01)
- `render/light_view.cpp`, `drawLight`: after everything standing is
  drawn and before the edge of sight fades, a multiply layer over the
  cells in sight. Each cell's colour runs from `DARK` (84, 87, 122; was 58, 62, 98) to its
  brightest source's tint by level (0, 0.6, 0.9, 1.0). Strip lights are
  a cool off-white, vending warm, screens blue, racks green, dispensers
  pale blue, statues and torches amber, night lights blue.
- Cells meet in a 4x4 ordered dither: each pixel takes its own cell's
  colour or the nearer neighbour's by how far from its cell's middle it
  lies, never a wall's over floor, so no light leaks through walls.
- Remembered cells are left as they were (`MEMORY_DIM`). Lit floor in
  sight measures about twice as bright as remembered floor; dark floor in
  sight is darker than memory. In step 4 dark cells beyond a few steps
  drop out of sight anyway.
- Humans and tall art are shaded by the cells they stand over.
- Not yet: fittings drawn as glyphs, flicker.

### Sight, memory and light as one layer (2026-11-01)
User: integrate vision and light better, graphically. The light layer now
does all three shadings that were separate:
- **Memory keeps the light.** `Memory::seeLight` stores each seen cell's
  level and source kind (the sim doesn't plan on it yet; `make check`
  unchanged). Remembered cells are greyed by the light they were seen
  in, at `MEMORY_DIM`, never below `MEMORY_DARK` (half) of remembered
  light (0.6, was 0.5), so a dark place stays readable on the way back. Cells only heard
  of are drawn as lit.
- **The edge of sight fades into memory**, not into black: the last two
  rings mix a quarter and three quarters toward how the cell will be
  remembered. The black `SIGHT_FADE` pass in `world_view.cpp` is gone.
- **Eyes get used to the dark:** a glow round the viewer, 4 cells out,
  as bright as dim light, so in a dark place the human makes out a small
  circle round itself. Step 4 should make `DARK_SIGHT` match it.
- Art out of sight is still drawn dim by each art function; each pixel
  of the layer is divided by how dim its own cell's art is, so the layer
  can darken but never brighten memory, and the fade happens on the side
  in sight.
- Until step 4, a dark place in sight beyond the glow looks about as dark
  as remembered dark; then those cells drop out of sight.

### Dither patterns (2026-11-01)
User asked for other dithering: crossed, hatched, and lines.
- The layer now blends the shades of the four cells round each pixel
  (bilinear) and steps the blend into `SHADE_STEPS` (8) steps, each pixel
  between two steps going up by a 4x4 pattern. Before, a pixel only took
  its own cell's shade or a neighbour's, so a pattern showed only within
  half a cell of a cell's edge.
- Four patterns (`Dither` in `light_view.h`): **ordered** (Bayer dots),
  **hatched** (rising diagonals), **crossed** (a rising diagonal, then the
  falling one over it), **lines** (level rows). **D** in game cycles them
  (the name goes to the terminal); `make shot SHOT_ARGS="--dither NAME"`.
  The user picked **ordered** (2026-11-01).
- The pattern breathes, very subtly (user): each of the 16 places in the
  pattern has its threshold swing by `PULSE` (0.08) at its own phase,
  once every `PULSE_SECONDS` (2.4 s), so pixels on the edge of a step
  come and go one by one. View only, on the clock.

### 4. Light limits sight
- `Sight::look` marks a cell seen only if it is lit, or within
  `DARK_SIGHT` cells (2–3) of the eye. Walls and lit cells beyond a dark
  stretch are still seen.
- Humans are seen by others by the same rule: one standing in the dark is
  seen only close.
- `Memory` keeps which rooms it found dark. Golden changes; look at the
  diff for new stuck humans (walking blind into dark mazes).

### 5. What darkness does
- Dread in the dark by bravery (more for the timid); lines on entering a dark place for the
  first time and on lights dying around it (feeling, not report).
- Sleep: where-to-sleep scores dark corners by trait.
- Hiding: private spots and stashes favour the dark.
- Traps and mess unseen in the dark are walked into.
- The lost and parched drift toward lit places they can see.
- A cake across a dark room is missed until it is near.
- A scenario per behaviour in `tests/scenarios.cpp`.

### 6. The mall clock and its bells
- The world keeps the mall's time of day, from the world's start (no
  dice). At closing time the ceiling fittings go out mall-wide but for a
  few night lights (chosen per chunk at generation); at opening they come back. Machines and
  the glowing statues stay on.
- An hour bell rings each mall hour; a different bell rings at opening
  and at closing. Bells go in the sim's noise list (sound_plan.md step 2)
  so the sound plan plays them.
- Humans hear the bells (a sim event, heard everywhere, softer through
  walls as sound_plan.md has it): `note()` and a `sayLater()` moment for
  the hour bell (rarely, so it isn't every 50 s), and for opening and
  closing. Closing raises the pull to sleep; opening wakes the sleeping
  (not the exhausted); hope or dread by trait, as escape_plan.md's hope.
  Humans learn the clock only by hearing it: one that has heard a few
  closings comes to expect them (`Memory`).
- The light map is redone for every chunk at opening and closing, and
  where a switch or torch changes it.
- The HUD's top strip shows the mall time (HH:MM) to the right of the
  humans and chunk counters, measured with `ui::textRight`; on the wide
  PC layout in the same place. A scenario checks the bells and reactions.

### 7. (If A3) Switches and carried light
- Wall switches per room; a new `Action` with rows in all three tables.
- A torch item, found or vended, carried like a bottle, its light moving
  with the human (light map updated round it each tick: watch the cost
  with `make simprofile`).

## Build order

1 → 3 (look only, with a few sources from 2 in a scene) so the user can
judge the look before anything changes the sim; then 2, 4, 6 (the clock
before behaviour, since sleep and fear lean on it), 5, 7.

## Open questions

All answered (2026-11-01). Left to judge when built:
- The look (step 3): colours, how dark is dark, dither.
- Mall hours of 50 s and 06:00–24:00 opening are defaults; the sleep
  plan's ~15–20 min awake and ~90 s asleep don't line up with a 15 min
  open day, so watch whether closing-time sleep fights the sleep tuning.
- `DARK_SIGHT` (2–3 cells) and whether walking blind into dark mazes
  leaves humans stuck (step 4's golden diff).
