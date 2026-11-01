# World generator editor (2026-11-01)

A native raylib tool, `make genview`, for tuning world generation with a
live preview: knobs on one side, a chunk regrown on the other each time a
knob moves. The user chose a native tool over a web page (like the sprite
editor) because a regrow takes under a frame, so the preview is truly live.

## Where things stand

Every generation knob is a `constexpr` at the top of the file that uses
it, so changing one means a rebuild:

- `world/world.cpp`: the kind mix (`KIND_WEIGHTS`, `DATA_CENTRE_CHANCE`,
  `SERVER_ROOM_CHANCE`), doors (`NEW_DOOR_CHANCE`, `DOOR_WIDTH`,
  `DOOR_MARGIN`), keys and cakes (`KEY_DEPTH`, `LOCKS_PER_SPARE_KEY`,
  `CAKES_MIN/MAX`), traps (`SLAM_ROOMS_MAX`, `DART_BAIT_CHANCE`,
  `SPIKES_MIN/MAX`), statue weights, `SHAPES` (footprints and weights).
- `gen_rooms.cpp`: room count and sizes, corridors, loops, floors and
  walls per room, canals, pools, pillars, partitions.
- `gen_maze.cpp`: corridor and wall widths, braiding, halls.
- `gen_plaza.cpp`: margins, bands, colonnades, moat, avenues, lining,
  courts, kiosks, booths.
- `gen_water.cpp`: pool sizes and arms, canal and quay widths.
- `gen_traps.cpp`, `gen_vending.cpp`, `gen_server.cpp`,
  `gen_toilets.cpp`, `gen_dispenser.cpp`: their own sizes and chances.

A chunk is grown by `World::generate(slot, entry)`, which rolls the
footprint and the kind itself. `tools/genprobe.cpp` already grows
interiors on every footprint without a World, and `tools/shot.cpp` draws
a world without a visible window.

## Proposal

### Step 1: knobs in `data/worldgen.txt`

- The generation constants move to `data/worldgen.txt`, one per line:
  name, value, then the range the editor allows, then a `#` comment taken
  from the `///` line it has now. Grouped under headers, one per file
  above. Pairs like `ROOMS_MIN/MAX` stay as two knobs.
  ```
  [rooms]
  ROOMS_MIN        3     1..20     # Rooms a chunk wants, at least
  LOOP_CHANCE      0.3   0..1      # Chance a corridor closes a loop
  ```
- `world/gen_tuning.{h,cpp}` reads it into one table, which the
  generators read by name through a typed handle, looked up once (as
  `talk::` and `sprites::` do), so generation is no slower.
- The values in the file start as today's, so this step is a pure
  refactor: `make check` passes unchanged.
- The game reads it at start; F5 reloads it with lines and sprites. New values
  apply only to chunks grown from then on; a world already grown stays.
- Only knobs that shape what is grown move. Timings the sim runs on
  (`SMELL_EVERY`, `SPIKES_UP_TIME`, `DART_RELOAD_TIME`, airlock and so on)
  stay in code.

### Built (2026-11-01)

- `data/worldgen.txt` holds 177 knobs (226 values, with the lists) over
  ten sections (world, maze, rooms, plaza, water, traps, vending,
  dispenser, server, toilets), each with its range and the comment above
  it. A value may be written as a fraction (`MACHINE_CHANCE 1/3`), worked
  out in floats as the code did.
- `world/gen_tuning.{h,cpp}`: `tune::Int`, `tune::Float` and `tune::List`
  handles, each named by section and name at the top of the file that uses
  it, in place of its `constexpr`. A handle finds its knob the first time
  it is read and keeps its place, so reading one costs what a constant
  did. A reload keeps every knob where it was. A knob the file lacks, or
  with the wrong count of values, stops the game with a message saying
  which.
- Lists: `KIND_WEIGHTS`, the table and statue weights, `SHAPE_WEIGHTS`
  (the footprints stay in code, in the order the comment names), the
  server room sizes (`CLOSET_SIZE`, `ROOM_SIZE`, `HALL_SIZE`: widths then
  depths) and their weights.
- Left in code: timings the sim runs on (smell, spikes, darts, airlock),
  the wake bottle, the dispenser's fill up to full, what is tied to the
  art (how far a machine or dispenser rises over the cells behind it) or
  to the layout's shape (`LOBBY`, the facings, restroom tiles, the
  raised floor).
- F5 in game reads it with lines and sprites. The soak reruns when it
  changes, and the APK ships it.
- `make check` passes unchanged, and `genprobe 20 png` gives the same
  output and pictures as before on every footprint and kind.

### Step 2: growing one chunk on demand

- `World` gets a way to grow a given kind on a given footprint at a slot,
  with a given seed, used only by the tool through a `GenViewProbe` friend
  in `tests/probe.h`. `generate()` itself still rolls as now, so the game
  and the soak are unchanged.

### Built (2026-11-01)

- `World::generate()` now only rolls the footprint and the kind, then
  calls `World::grow(kind, origin, parts, entry, mayCentre)`, which grows
  the chunk as before. The data centre's own dice are rolled even when the
  kind is given, so a chunk grown as asked comes out as the game would
  have grown it had it rolled that kind.
- `World::grows(kind, parts)`: whether the game ever grows that kind on
  that footprint (plazas only on a whole square, mazes no bigger than
  `MAZE_SLOTS_MAX`). `generate()` uses it for its kind weights.
- `GenViewProbe` in `tests/probe.h`, a friend of World: `grow(world, kind,
  footprint)` grows the chunk through the spawn's door, placed so the slot
  past the door is its own (Spawn gives the spawn itself; nothing when the
  game never grows that kind there); `rolled(world)` lets the game roll
  it; `spawnDoor(world)` gives the door and the cell past it.
- `genprobe` grows every kind on every footprint the game allows this way,
  5 seeds each, and checks them as it checks world chunks: 295 chunks, 0
  failed. `make check` passes unchanged.

### Step 3: `tools/genview.cpp`, `make genview`

A window, PC only. The knob panel on the left, the chunk on the right.

- **Knobs:** grouped as in the file, folding groups; a slider per knob
  within its range, and a field to type a value. A knob changed from the
  file's value is marked, and can be put back.
- **Live:** any change regrows the chunk on the same seed, so what moves
  is that knob's doing, not the dice.
- **Seed:** locked by default; R rerolls, and the seed shows so it can be
  given to the game or a bug report.
- **Picks:** chunk kind (spawn, rooms, maze, water maze, plaza, data
  centre, or rolled as the game does) and footprint (1x1 to 3x3, L, T, S,
  turned and mirrored).
- **View:** pan and zoom; the game's own art (floors, walls, water,
  statues, tables, machines, keys, cakes, traps), or a flat map at a pixel
  or two a cell, with a key to switch.
- **Compare:** before and after side by side, the file's values against
  the changed ones, same seed.
- **Spread:** a 3x3 grid of nine seeds at once, to see the range a setting
  gives rather than one lucky chunk.
- **Save:** changes stay in the tool until a Save button (or S) writes
  them back into `data/worldgen.txt`, keeping its comments and order.
  Ctrl+Z undoes a knob change.
- **Driven from chat (user: yes):** the tool watches `data/worldgen.txt`
  and reloads within a second when it changes on disk, so Claude in a chat
  session can edit the file and the window regrows in front of the user.
  Knobs the user has changed but not saved are kept over the reload.
- **What it shows, for Claude:** each regrow also saves the view to
  `build/genview.png` and the state to `build/genview.txt` (seed, kind,
  footprint, view, and every knob that differs from the file), so Claude
  can look at what the user sees and check a change did what was meant.
- **Restart in place:** the state file is read back at start, so after a
  rebuild for a code change the tool opens on the same seed and view.

### Built (2026-11-01)

- `tools/genview.cpp`, `make genview`, `./genview.sh` from the host (it
  runs it in the toolbox). Usage at the top of the file.
- Knob panel: sections fold; a slider for each value (lists one per value,
  numbered in the order the comment gives), stepped in whole numbers for
  knobs written without a point; double-click the value to type one (a
  fraction too); Left/Right step the knob under the mouse; a changed knob
  shows yellow with a tick where the file's value is; right-click its name
  to put it back; Ctrl+Z undoes. The hovered knob's comment shows in the
  status line.
- Top bar: kind (rolled as the game does, spawn, rooms, maze, water maze,
  plaza, data centre), footprint (the nine the game grows, then L3, L4,
  T4, S4), turn, mirror, seed (< > R), view (V: art or map), mode (one, C
  before/after, G nine seeds; clicking one of the nine takes its seed),
  Save (S) with the count of changed knobs. A kind the game never grows on
  a footprint says so in the pane.
- The art is the game's own drawWorld, revealed, without humans, drawn
  once a regrow into a texture the size of the chunk, then panned (drag)
  and zoomed (wheel, F fits). The map is a pixel a cell, coloured as
  genprobe's pictures.
- To tell a knob read as a fraction from a whole one, the file writes
  fractions with a point (`POOL_SIZE_MAX 7.0`); `tune::` checks every
  knob is written as the code reads it, and stops with a message if not.
- `tune::set` and `tune::save`: save writes only the knobs' lines again,
  keeping comments, ranges and a value written as a fraction while it
  holds.
- Watching and the saved view work as planned; tried by changing the file
  under a running genview (read again, unsaved knob kept), and saving into
  a copy (only the changed lines differ; saving twice gives the same file).

### Step 4: overlays and readout

- Overlays, each on its own key: doors and their keys, the sides of the
  water (`gen::sides`), floor that can't be reached from the entry door
  (red), traps with dart lines and slam rooms, deep spots, private spots,
  where vending machines could stand.
- Readout per chunk, and as min/average/max over the grid: floor %, water
  %, dead ends, longest walk from door to door, rooms, tables, keys,
  cakes, traps, statues, machines, toilets. Uses `genprobe`'s checks too,
  and says so loudly when one fails (floor outside the footprint, a door
  that can't be reached).

### Built (2026-11-01)

- A second row in the top bar: five overlays, any together (keys 1 to 5),
  each a picture a pixel a cell over the art or the map:
  1. doors and keys: open doors orange, locked red, keys yellow, cakes
     pink, the entry white;
  2. water sides (`gen::sides`), a colour each;
  3. out of reach from the entry, locks as open: orange where only
     swimming gets, red where nothing does;
  4. traps: spikes red; dart plates pink with their shooter and line cyan;
     slam plates pink with their gate red;
  5. deep spots, where keys and the chunk's human may start.
- T: the readout, per chunk: floor and water %, the longest walk between
  two doors that can reach each other, the farthest floor from the entry
  (in steps, locks as open), doors and new doors, keys, cakes and those
  over water, tables, statues, machines, toilets, basin cells, server
  rooms, dispensers, spikes, dart plates, slam rooms. Before/after puts
  the two side by side, what differs yellow; nine seeds gives the least,
  mean and most.
- genprobe's checks moved to `tools/chunk_checks.h` and are run on every
  chunk grown: what they would fail it for shows in red across the
  chunk, readout or not.
- The readout and any fault go into `build/genview.txt` too, a line per
  chunk, for Claude.
- Not built: dead ends (no good measure on corridors three wide), a count
  of rooms (the rooms generator's boxes are not kept), private spots, and
  where a vending machine could stand (that test lives inside
  `placeMachines`). Each can come if wanted.

### Step 5: drop humans

- A button runs the sim headless on the chunk (with the world around it
  grown as usual) for some minutes with a crowd, and paints a heatmap over
  the chunk: where humans walked, slept, got stuck, bled and died. Shows
  how a layout plays, not just how it looks.
- How many humans, for how long, and which heatmap shows are picked in
  the tool.

### Step 6: play this

- Opens the game on that seed with the edited values, for a look in play.

## User's decisions (2026-11-01)

1. **What it's for:** tuning the knobs that exist, for now. No hand-made
   chunks or pieces.
2. **Saving:** changes stay in the tool until saved, with a Save button
   in the window as well as the S key.
3. **Which knobs move:** every knob that shapes what is grown, in one
   step (step 1).
4. **View:** both the game's art and a flat map, with a key to switch.
5. **Drop humans:** chosen in the tool: how many, for how long, and which
   heatmap shows.
6. **Ranges:** about half to twice each knob's value, for now; widened in
   the file where needed.
7. **F5 in game:** new values apply only to chunks grown from then on.
8. **No Claude inside the tool:** the prompt box that ran `claude -p` from
   the editor was dropped. Claude can still drive it from a chat session
   through the file watch and the saved view (step 3).
