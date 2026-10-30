# Chunk shapes plan (2026-10-30)

Today every chunk is a rectangle of slots (1x1, 2x1, 1x2, 2x2) filled by
one of four kinds (rooms, maze, water maze, plaza), and its floor fills
its whole box. This plan makes the mall less boxy in four ways:

- **Footprints that aren't rectangles**: chunks shaped like L, T, S or Z,
  up to 4 slots.
- **Bigger rectangles**: 3x1, 1x3 and 3x3 (this takes over size work from
  step 6 of [worldgen_variation_plan.md](worldgen_variation_plan.md)).
- **New kinds**: mall wing (step 5 of the variation plan, moved here),
  food court, parking/service, atrium.
- **Shaped outlines**: a rectangular chunk whose walkable area is one big
  shape: a ring, a cross, a stepped diagonal band, or islands.

What it should make matter: where a human is starts to shape what they do.
A food court is rich and crowded, so it's where fights over cakes happen.
Parking is bare, with places to hide and to sleep or rob sleepers. A ring
or atrium makes chases loop. Islands and crosses make chokepoints where
guarded water, traps and ambushes count. Long and bent chunks make
crossing one take a while, which feeds hope, despair and theories of the
way out.

Ties into: door and key placement (`placeDoors`, `deepSpots`), all the
`gen_*.cpp` generators, statues, tables, toilets, traps, water and basins.
Humans need no changes: they path and see by tiles, and never read a
chunk's kind.

## User's decisions (2026-10-30)

- "Chunk shapes" means all four: odd footprints, bigger rectangles, new
  kinds and shaped outlines.
- Odd footprints go **up to 4 slots**: L of 3, and L, T, S/Z of 4. (The
  straight 4 is a 4x1 rectangle; see question 1.)
- Odd footprints get **only the kinds that fit the shape**. Rooms, maze
  and water maze work on any outline. Plaza stays square only. The mall
  wing's street follows the bends of the L or T.
- New kinds: **mall wing, food court, parking/service, atrium/gallery**.
- Outlines: **ring/donut, cross/plus, diagonal band, islands**.
- Doors **stay per slot**: at least 1, at most 4 per slot, a 35% chance
  per free edge. Big chunks aren't made scarcer in doors.

## How a chunk's footprint works now

- `Chunk` has `slot` (top-left) and `slots` (size), `covers()` is a box
  test, and `tiles`, `floors`, `walls` cover the whole box.
- `World::perimeter()` already works per slot: it lists each slot edge
  whose neighbour the chunk doesn't cover. `placeDoors`, `markDoors` and
  `drawDoor` all go through it. So doors on an L shape need nothing new
  once `covers()` is right.
- `World::tile()` finds the chunk through `slotChunk_`, so a cell in a
  slot the chunk's box spans but doesn't own is never read from that
  chunk. Render doesn't walk chunks either.
- About 60 places in `src/world` assume the box: `c.w - 1`, `c.h / 2`,
  `interior()`, loops over the whole box. Those are the work in step 1.

## 1. Footprint foundation (pure refactor)

### Data
- `Chunk` gets `std::vector<Vec2i> parts`: the slots it owns, as offsets
  from `slot`. `slots` stays the bounding box. `covers(s)` checks the
  owned slots.
- `bool owns(Vec2i l)`: the local cell lies in an owned slot.
- `interior(l)` becomes: owned, and not on a slot edge that faces a slot
  the chunk doesn't own. Seams between two owned slots are open floor,
  like the middle of a 2x2 today.
- At construction, cells in unowned slots of the box are `Tile::Void`, so
  `carve`, `dig` and the floods never touch them (they only turn Wall into
  floor). The border ring is Wall along outer edges only.

### Generators
- Each generator, and every helper in `gen_common.cpp`, works through
  `owns()`/`interior()` rather than the box bounds. Where it needs a
  centre or a span, it asks the chunk: `Chunk::centre()` is the owned cell
  nearest the box centre.
- `sides()` and `connectRegions()` already label by tile, so they follow
  once Void is treated like the border.
- `deepSpots`, spikes, statues, tables: their loops over the box skip
  unowned cells.

### Check
Every chunk is still a rectangle, so `make check` must pass unchanged.

## 2. Bigger rectangles

- `SIZES` gains `{3,1}` and `{1,3}` at 0.6 and `{3,3}` at 0.25 (from the
  variation plan). See question 1 for 4x1.
- Doors stay at up to 4 per slot, so a 3x3 may have up to 36. Keys follow
  doors as now.
- Cakes, slam rooms, spikes and statues already scale per slot.
- **Generation time**: `connectRegions` floods the whole chunk on each
  pass. At 300x300 that may be slow. Time it in the new tool (step 9).
  If it's over about 20 ms, label once and union regions instead of
  flooding again.

## 3. Odd footprints (L, T, S/Z)

### Shapes
| shape | slots | weight (draft) |
|---|---|---|
| L3 | 3 | 1.0 |
| L4 | 4 | 0.6 |
| T4 | 4 | 0.5 |
| S/Z4 | 4 | 0.4 |

Each shape is turned in 90° steps and mirrored. `generate()` already
tries every placement of every size that covers the entered slot with
all slots free. It now does the same for each turn of each shape.

### Which kinds
- Rooms, maze and water maze: any footprint. Rooms are placed only where
  their whole box is owned. Corridors that would cross an unowned slot
  stop at the Void, and `connectRegions` joins the rest.
- Plaza: square rectangles only, as now.
- Mall wing: the street follows the footprint's spine, so an L has a bent
  street and a T has a T junction (step 5).
- Food court, parking, atrium: see each step for which footprints fit.

## 4. Shaped outlines

An outline is a mask laid over a rectangular chunk before its kind
generates. Cells outside the mask are made solid (wall, or water for a
water core), and the kind carves only inside it. So you get a ring of
rooms, a cross-shaped maze, and so on.

- **Ring/donut**: a wide band round a core of solid wall, or of water you
  can see across but not walk across. At least 12 cells wide.
- **Cross/plus**: four arms 20 to 40% of the chunk wide, meeting in a
  hub. On a 2x1 it's a long arm with a short crossbar.
- **Diagonal band**: a band from one corner to the opposite corner, made
  of steps at least 6 cells each, so every edge is straight and water
  edges stay at 90°.
- **Islands**: 2 to 5 blobs (plain rects or ovals), joined by bridges 3
  to 5 cells wide and 6 to 15 long, which are the chokepoints. Between
  them is solid wall or water.

Doors: each door must open onto the outline's floor. `connectDoor`
already digs from the door to the nearest floor, so a door outside a ring
gets a short corridor in. That corridor must stay off a water core
(`dig` never touches water, so it routes round). If a door can't reach,
it's skipped as now.

Which kinds take an outline, and how often, is question 2.

## 5. Mall wing (`gen_mall.cpp`)

As in the variation plan, plus bends:
- A wide street (6 to 10 cells) down the footprint's spine: straight in
  a rectangle, bent in an L, a T junction in a T, a plus crossing in a
  2x2 or 3x3.
- Shops on both sides, 6 to 14 wide and 6 to 12 deep, each with a 3-cell
  doorway onto the street. Shops are split by single walls. Each shop
  takes one of 3 or 4 shop floors the chunk rolls.
- Sometimes an anchor store: one big shop at the end of the street.
- Down the street, sometimes planters or pillars, or a statue following
  the ring rule. Sometimes a small statue as a shop window display.
- Fits: long rectangles (2x1, 3x1), L, T. Not 1x1.

## 6. Food court (`gen_foodcourt.cpp`)

- One big hall, with counters (rows of tables) round its edge and a
  dense grid of small tables in the middle.
- Many more cakes than other kinds (draft: 3x per slot), and 2 to 4
  basins. Every cake still lies on a table.
- A restroom off the hall is likely (draft 70%).
- Dart bait is likelier here, since there are long straight runs between
  tables.
- Fits: square and near-square rectangles, and L.
- This chunk is where crowds and fights happen. Watch the soak for it
  turning into a death pit; see question 4.

## 7. Parking / service (`gen_service.cpp`)

- Long bare halls with a grid of 2x2 pillars every 6 to 8 cells. Concrete
  floor, plain walls.
- Loading bays: 4 to 6 cell deep recesses along the walls. Dead ends,
  dark corners, good for sleeping and hiding.
- Narrow service corridors (2 cells wide) wind between halls, as the
  back way.
- Few cakes (draft: 0 to 1 per slot), no basins. Toilets only in a
  staff restroom, sometimes.
- More spikes and slam rooms than usual.
- Fits: any footprint. Likes long chunks and odd shapes.

## 8. Atrium / gallery (`gen_atrium.cpp`)

- A ring walkway (8 to 12 cells) round a big middle you can see across
  but not walk across: water (a huge pool) or an open void. Void would
  need a new `Tile::Drop` (opaque: no; passable: no). See question 3.
- Shops round the outside, like the mall wing's.
- 2 to 4 bridges may cross the middle, as chokepoints.
- Statues at the corners of the ring.
- Fits: square chunks (2x2, 3x3), maybe 1x1 small. This is the ring
  outline from step 4 with shops; build it after step 4.

## 9. Generation probe (`tools/genprobe.cpp`)

A headless tool, `make genprobe`, that builds many chunks of every
shape, kind and outline over many seeds, and checks:
- every floor cell on a door's side of the water is reachable from that
  door;
- every door has floor just inside it;
- no floor, key, cake or table lies in an unowned slot;
- how long each generation takes, by size;
- how many of each kind, shape and outline came up.

It also saves a few `make shot` pictures per new kind. Variation steps
3 and 4 were checked this way by hand ("1044 chunks, 20 seeds"); this
keeps that check.

## Kind weights

Draft weights per footprint. 0 means the kind doesn't fit there.

| kind | 1x1 | 2x1/3x1 | 2x2 | 3x3 | L/T/S |
|---|---|---|---|---|---|
| Rooms | .35 | .25 | .30 | .25 | .35 |
| Maze | .20 | .15 | .15 | .15 | .25 |
| Water maze | .10 | .08 | .08 | .05 | .10 |
| Plaza | .12 | 0 | .10 | .10 | 0 |
| Mall wing | 0 | .30 | .10 | .10 | .15 |
| Food court | .10 | .07 | .12 | .10 | .05 |
| Parking/service | .13 | .15 | .07 | .10 | .10 |
| Atrium | 0 | 0 | .08 | .15 | 0 |

## Build order

1. Footprint foundation. `make check` unchanged. Commit.
2. Genprobe tool (step 9), against today's chunks, as the baseline.
3. Bigger rectangles. Time 3x3 generation. `make golden`.
4. Odd footprints with rooms, maze, water maze. Probe, shots, golden.
5. Outlines on rooms and maze.
6. Mall wing.
7. Parking/service.
8. Food court.
9. Atrium (needs outlines).

Each step builds, passes the probe, and is checked in the game before
the next. The soak will change at every step from 3 on: look at the diff
for new stuck humans or blocks before `make golden`.

## Open questions

1. **4x1 and 1x4**: they're 4 slots and straight, so they fit "up to 4
   slots". Include them, or keep the long rectangles to 3?
2. **Outlines**: which kinds take one (rooms, maze, plaza, water maze?),
   and how often? Draft: rooms 25%, maze 20%, plaza 30% (a ring plaza,
   a cross plaza), never water maze.
3. **Atrium middle**: water (no new tile; you can drink and wash at the
   edge) or a drop/void you can see across (new `Tile::Drop`, and maybe
   falling later)?
4. **Food court plenty**: three times the cakes may make it a magnet and
   a death pit. Is that what you want, or should cakes there be normal
   and it's just tables and basins?
5. **Do humans notice places?** Chitchat has place remarks (its step 6
   is still open). Should humans say something new on first entering a
   food court, parking or atrium ("Food! Everywhere!"), with a `note()`
   and a moment? Or leave places silent until chitchat step 6?
6. **Districts**: the other half of variation step 6 (neighbours sharing
   wall and floor styles) stays in that plan. Do it before, with, or
   after this one?
