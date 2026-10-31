# Server rooms in the world

The machine room art is done (see [machine_room_art_plan.md](machine_room_art_plan.md)):
the server rack (2 cells, 25 px tall), the CRT terminal (1 cell, 13 px),
the console desk (6x4 cells, its screens rising 9 px), and cable floor
decals (straights, corners, Ts, cross). `tools/server_rooms.cpp` lays out
three rooms by hand: a 6x5 closet, a 12x9 room and a 20x14 hall. This plan
puts server rooms into the generated mall.

Status: planned, questions answered (2026-10-31). Not started: build it
after the vending-machines branch is merged (see below).

## User's decisions (2026-10-31)

- **Where**: in room networks, **rarely** (about one network in ten), and
  in a new chunk kind of their own, a **data centre**, **uncommon** (about
  one chunk in eight), of any footprint room networks use (L, T, big boxes):
  corridors with server rooms and halls off them.
- **Grown to fit**, not fixed maps: racks in rows facing south, the desk on
  the back wall where the room is big enough, terminals, cables routed
  between them. Any room size works and no two look alike.
- **A double door: two doors in a row**, an airlock. An outer door, a small
  lobby, an inner door, each locked and opened with **one key**, so
  getting in costs **2 keys**. The doors **shut and lock again behind**
  whoever went through, so every visit costs keys; but **from the inside
  they open freely**: the inner door from the room, the outer door from
  the lobby. Nobody is ever shut in.
- **Racks block sight**; terminals and the desk don't. All of them block
  walking.
- **First sight**: seeing a server room for the first time, a human
  comments on it (a life log line and a spoken moment) and **remembers
  it**. Keep that memory for later uses.
- **A good place to sleep**: the warm hum of the servers makes a server
  room a good sleeping spot (the user first left this out, then asked for
  it). Behind its locked doors it is also safe.

Defaults taken where nothing was said (change them if wrong):
- The desk blocks its top and wings; its chair's cells stay walkable floor.
- A door shuts once nobody stands in it, a moment after.

## What the code already has to build on

- **Toilets and restrooms** (`gen_toilets.cpp`): a toilet is a `Tile` that
  blocks walking, not sight, kept in a `World` map by cell. A maze may get a
  restroom: a tiled hall carved into the maze where it fits, its walls lined
  with toilets. A server room in a maze can be cut the same way.
- **Room networks** (`gen::rooms`): rooms of assorted shapes joined by
  corridors, furnished with pillars or partitions afterwards. A rectangular
  room can be turned into a server room there instead.
- **Statues** keep `behindStatues_`: cells their body is drawn over, where
  nothing is put, since nobody could see it. A rack covers about four rows
  behind it, the desk's screens two.
- **The vending-machines branch** (`claude/vending-machines-5f0e7f`, not yet
  merged) adds `Tile::Vending`, a machine list with a cell map, placement
  with dice of its own seeded per chunk (so the rest of the mall is laid
  out as before), and drawing tall front-on machines in `world_view.cpp`.
  Server rooms should copy those patterns, and be built **after that branch
  is merged**, on top of it: both touch the `Tile` enum, `world.h`,
  `world.cpp`, `world_view.cpp` and the golden output.

## Steps

### 1. The machines in the world

- `src/world/machine_room.h`: racks, terminals and console desks, each with
  its cell (bottom-left of its foot, or the desk's top-left) and a seed so
  their lights don't blink in step. `World` keeps them with a cell map, as
  toilets and vending machines are kept.
- New tiles: `Tile::Rack` blocks walking and sight (add it to `opaque`);
  `Tile::Console` blocks walking only, for terminals and the desk's top and
  wings. The chair's cells stay floor.
- The cells a rack's body rises over (four rows behind it) and the desk's
  screens (two) go into `behindStatues_`, so nothing is put where it can't
  be seen.

### 2. Cables on the floor

- Decals, like blood and mess: walked over, never blocking. A `World` map
  cell → which edges the cable leaves by (`CABLE_N/E/S/W`) and its look.
- Laid from the machines: a run in front of each row of racks, joined by a
  trunk, out through a wall, as in the preview rooms (the end-of-run rule in
  `tools/server_rooms.cpp`'s `linksAt`).

### 3. The airlock doors

- A server room has one way in: outer door, a lobby a cell or two deep,
  inner door. Both doors are locked. A key opens one from the outside
  (the outer from the corridor, the inner from the lobby) and is used up;
  from the inside each opens freely.
- Each door shuts and locks again a moment after nobody stands in it.
- Slam room gates are the nearest thing already built (`gates_`: keys open
  them, they slam shut), so the doors build on gates, with a side that
  opens freely. Humans must know a door is free from inside, so walking
  never treats the way out as locked.
- Humans with one key may open the outer door and wait in the lobby, or not
  go in: see how they behave in the soak, and ask the user if it's odd.

### 4. Where they appear (`src/world/gen_server.cpp`)

- **In room networks** (rare): a rectangular room, big enough for a closet
  at least, becomes a server room: its openings walled up but one, where
  the lobby and its two doors go.
- **The data-centre chunk** (`ChunkKind::DataCentre`, uncommon, any
  footprint): a corridor network with server rooms of all three sizes off
  it, each behind its airlock, and maybe a plain room or two.
- **Growing a room's inside**: rows of racks facing south against the
  north wall and then every five or six rows, so the aisles show; the
  console desk on the north wall when the room is 6 or more wide and deep
  enough; terminals at row ends and in the open; floor all round each row's
  ends so nothing is walled off; cables last.
- Raised floor: grey tiles (an existing pattern and palette, or a new
  pattern for the perforated tiles of the cold aisle).
- Server rooms go in after all digging, as slam rooms do, so no corridor
  cuts a second way in.
- Dice of their own, seeded per chunk, as vending does, so the rest of the
  mall stays as it was where there is no server room.
- `make genprobe` grows them on every footprint; check every room has its
  one way in and every machine can be walked round.

### 5. Drawing

- `world_view.cpp` draws cables on the floor layer, and racks, terminals
  and desks among the standing things, sorted by their foot row, dimmed and
  still when only remembered (the art does that already).
- Doors: the airlock's doors drawn as the exit doors are, shut or open.
- The map (`map_view.cpp`) shows their cells as furniture.

### 6. Humans: first sight and memory

- Seeing a server room's machines for the first time: a `note()` in the
  life log and a new `talk::Moment::FirstServerRoom` through `firstSight`
  (as FirstDoor and FirstToilet are), with lines in `data/lines.txt` that
  give the feeling, not a recap.
- `Memory` remembers each server room it has seen (its inner door and its
  floor), learnt only by seeing or being told, never read from the world.

### 7. Humans: sleeping by the servers

- In `seekSleep`'s spot cost (`human_sleep.cpp`), a cell inside a
  remembered server room scores better: warm, humming, and behind locked
  doors. Tired, a human with keys may head for one it knows; the price is
  2 keys, so it is a choice between a safe sleep and a way out.
- A `note()` and a moment for falling asleep to the hum.
- A scenario: a tired human with two keys and a known server room goes in
  and sleeps there.

### 8. Tests

- `make check` will change: compare the soak's diff for stuck humans,
  loops or blocks, then `make golden`.
- Scenarios for what the soak seldom reaches: going through the airlock
  with 2 keys, being let out freely, the doors relocking, first sight,
  sleeping by the servers.

## Open questions

None now. Asked and answered above; new ones go here as the build turns
them up.
