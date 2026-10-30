# World generation variation plan (2026-10-29)

Decisions: statues block walking and sight; each statue rolls a material
(marble, bronze, stone, pop-art colours); the plan covers plazas, mall
wings, richer rooms, districts and bigger chunks.

## 1. Giant household statues (foundation)

### Data
- `Tile::Statue`: not `passable`, `opaque`. Only the statue's base is
  statue tiles. Its floor stays in `floors[]` underneath. Humans path and see by
  `passable()`/`opaque()`, so they handle it with no change. Keys, cakes,
  `deepSpots` and dart lines all want `Tile::Floor`, so they skip statues
  without extra code.
- `world/statue.h`:
  - `enum class StatueKind { Mug, Teapot, RubberDuck, Toilet, Chair, Toaster, DeskLamp, AlarmClock, Spoon, LightBulb }`
  - `enum class StatueMaterial { Marble, Bronze, Stone, Pop }`
  - `struct Statue { kind, material, bool flip, bool plinth, uint8_t scale (1 or 2 = x1.5), Vec2i base /* world cell, footprint top-left */ }`
  - `Vec2i footprint(const Statue&)`: the cells the base covers, per kind
    and scale. A plinth adds a cell all round.
- `World` keeps `std::vector<Statue> statues_`, like keys and cakes.

### Sizes (scale 1; a human is 15x18 px, about 2.5x3 cells)
| kind | sprite px | footprint cells |
|---|---|---|
| Mug | 42x48 | 7x3 |
| Teapot | 60x48 | 10x4 |
| Rubber duck | 48x48 | 8x4 |
| Toilet, seen from the front (replaced the sneaker) | 54x60 | 9x4 |
| Chair | 42x72 | 7x4 |
| Toaster | 54x42 | 9x4 |
| Desk lamp | 48x84 | 5x3 (base only, the arm hangs over) |
| Alarm clock | 48x54 | 8x3 |
| Spoon (stuck upright) | 18x90 | 3x2 |
| Light bulb | 36x60 | 6x3 |

Objects are 2 to 5 humans tall. Scale 2 is for plazas and big halls only.

### Rendering (`render/statue_sprites.{h,cpp}`)
- Drawn procedurally, one function per kind, into a raylib `Image` at 1:1
  world pixels (the same pixel size as humans). Shapes are filled
  rects/ellipses with a 1px outline and light from the top left. Textures
  are cached per (kind, material, scale, flip).
- Materials are 4-tone ramps: outline, dark, mid, light. Marble is near
  white with grey veins, bronze is gold-brown with green patina flecks,
  stone is grey. Pop gives each part its own colour: red mug with a white
  inside, yellow duck with an orange beak, and so on.
- Side view like humans: the bottom of the sprite sits on the footprint's
  south edge, and the rest rises over the cells to the north. Statues are
  y-sorted with humans by base y, so a human behind a statue is hidden by
  it.
- A plinth is drawn under the statue as a block in the chunk's wall
  palette (top plus south face, like walls).
- A statue is drawn when any of its footprint cells is remembered. It is
  lit when any of those cells is seen, and dim otherwise, like keys.

### Placement (`gen::statues` in `world/gen_statues.cpp`)
- A spot is valid when the whole footprint and a 1-cell ring round it are
  floor. That ring means a statue can never cut anything off, by the same
  argument as pools. It also stays at least 3 cells from the inside of any
  door.
- Generators return a small `Layout { std::vector<Rect> rooms; }` hint, so
  statues can be centred in rooms instead of landing anywhere.
- Order in `World::generate`: kind generator, then doors, then
  `connectRegions`, then flood, then **statues**, then traps, then
  `deepSpots`. Statues never dig. `dig` also learns to treat
  `Tile::Statue` as blocking, as a safety net.
- How many:
  - Spawn: 60% chance of one statue, kept off the centre.
  - Rooms: each room that fits footprint + ring + 2 has a 25% chance of a
    centrepiece; at most 3 per slot.
  - Maze: each hall has a 30% chance.
  - Water maze: 10% chance per chunk.
  - Plaza and mall wing: see below.

## 2. Richer rooms (in `gen_rooms.cpp`)
- New shapes: Donut (a rect with a solid core of at least 3x3), U, T,
  rounded rect, diamond.
- Pillars: rooms at least 16x16 sometimes get a grid of 1x1 or 2x2 wall
  pillars every 4 to 6 cells. They are placed after corridors and only
  where `openAround` holds.
- Partitions: a wall line across a big room with 1 or 2 gaps 3 cells wide.
  `connectRegions` still runs after as a safety net.
- Niches: a 3-deep recess carved into a room wall to hold a small statue
  (spoon, bulb, mug). It is a dead end, so it is exempt from the ring rule:
  its only opening faces the room.

## 3. Plaza chunk kind (`gen_plaza.cpp`)
- One grand hall that fills the chunk with a margin: rect, octagon or
  disk. It has one floor plus an inlaid border band of a second floor
  (`paintRect` ring), like a carpet.
- Optional grid of 2x2 pillars, every 8 to 12 cells.
- Fountain in the middle: a 2 to 3 wide water moat round a plinth island
  holding a scale-2 statue. The island can't be reached because canals
  can't be crossed, so it is pure decoration. The moat keeps off door
  approaches (via the `dry` point).
- 2 to 6 smaller statues round the hall, each following the ring rule.
- Sizes: 1x1 or 2x2.

## 4. Mall-wing chunk kind (`gen_mall.cpp`)
- A wide street (6 to 10 cells) along the chunk's long axis. In 2x2 chunks
  it is a plus-shaped crossing.
- Shops on both sides, 6 to 14 wide and 6 to 12 deep, each with a doorway
  3 cells wide onto the street. Shops are split by single walls. Each shop
  takes one of 3 or 4 shop floors the chunk rolls, like `rooms` does
  today.
- Sometimes an anchor store: one big shop at the end of the street.
- Down the middle of the street, sometimes a row of planters or pillars,
  or a statue following the ring rule. Sometimes a small statue as a shop
  window display.
- Sizes: prefers 2x1, 1x2 and 3x1.

## 5. Districts and bigger chunks (`world.cpp`)
- Each new chunk looks at generated neighbours. With 50% chance it copies
  one neighbour's wall style. With 40% chance it reuses that neighbour's
  floor palette, with a new pattern. Otherwise it rolls fresh styles. This
  way areas feel related without every chunk matching.
- New sizes: {3,1} and {1,3} at weight 0.6, {3,3} at 0.25. Door placement
  is already per slot, so it scales.
- Chunk kind is picked from a weight table per size:
  - Mall wing likes long chunks.
  - Plaza likes square chunks.
  - Default weights: Rooms .40, Maze .20, WaterMaze .10, Plaza .12,
    MallWing .18.
- Watch out: `connectRegions` floods the whole chunk on each pass. At
  300x300 that may be slow, so time generation and cap passes, or label
  once and union, if needed.

## Order of work
1. Statue foundation with 3 kinds (mug, rubber duck, chair) and all 4
   materials. Placement in spawn and rooms. **Done 2026-10-29.**
   `gen::rooms` returns `RoomBox`es for this; nothing is put behind a
   statue (`behindStatues_`); the followed human shows through as a ghost
   when a statue hides them.
2. The other 7 kinds. **Done 2026-10-29.** Desk lamp is 8x3 (the statue is
   as wide as its base). Placement now rolls one kind per room and finds
   the spot nearest the centre it fits (up to 4 cells off); if it fits
   nowhere the room gets none, so small kinds don't crowd out big ones.
   Room chance raised to 35%.
3. Richer rooms (shapes, pillars, partitions, niches). **Done 2026-10-29.**
   Pillars, partitions and donut cores are walled up after corridors are
   dug. Niches go only in north walls, so the statue faces the room; a room
   with no statue in its middle gets a niche 25% of the time. Checked: no
   chunk's floor is split on its side of the water (1044 chunks, 20 seeds).
4. Plaza. **Done 2026-10-29.** Square chunks only (weight .12, others
   .50/.25/.13). 70% have a fountain: a round or square pool round a
   square island sized to the grand (1.5x, art repainted at scale, not
   stretched) statue on its plinth; else the grand statue stands on the
   floor. Statues and moat width scale with slots across, not area.
5. Mall wing. Moved to [chunk_shapes_plan.md](chunk_shapes_plan.md) (2026-10-30).
6. Districts and bigger chunks. Bigger chunks moved to
   [chunk_shapes_plan.md](chunk_shapes_plan.md); districts stay here.

Each step builds and is checked in the game before the next.

## Coordination
Another session is adding traps right now (`gen.h`, `gen_traps.cpp`,
`trap.h`; `world.cpp` is not wired yet). Statues touch the same
`World::generate`, `gen.h` and the `Tile` enum. Start step 1 after the
trap work lands, or agree on the generate() order above with that session.
