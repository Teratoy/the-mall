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
- The game reads it at start; F5 reloads it with lines and sprites (see Q7
  for what that does to a world already grown).
- Only knobs that shape what is grown move. Timings the sim runs on
  (`SMELL_EVERY`, `SPIKES_UP_TIME`, `DART_RELOAD_TIME`, airlock and so on)
  stay in code.

### Step 2: growing one chunk on demand

- `World` gets a way to grow a given kind on a given footprint at a slot,
  with a given seed, used only by the tool through a `GenViewProbe` friend
  in `tests/probe.h`. `generate()` itself still rolls as now, so the game
  and the soak are unchanged.

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
  or two a cell (Q4).
- **Compare:** before and after side by side, the file's values against
  the changed ones, same seed.
- **Spread:** a 3x3 grid of nine seeds at once, to see the range a setting
  gives rather than one lucky chunk.
- **Save:** S writes the changed values back into `data/worldgen.txt`,
  keeping its comments and order. Ctrl+Z undoes a knob change (Q2).
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

### Step 5: drop humans

- A button runs the sim headless on the chunk (with the world around it
  grown as usual) for some minutes with a crowd, and paints a heatmap over
  the chunk: where humans walked, slept, got stuck, bled and died. Shows
  how a layout plays, not just how it looks (Q5).

### Step 6: play this

- Opens the game on that seed with the edited values, for a look in play.

### Step 7: asking Claude from the editor (user: yes)

- A key opens a prompt box at the bottom of the window. The user types
  what they want ("more pillars in big rooms", "why is this plaza so
  empty?") and Enter sends it.
- The tool runs the `claude` CLI headless (`claude -p`, on the user's
  Claude account, no API key) in the project folder, in the background so
  the window stays live. It passes the prompt, `build/genview.png`, the
  state file, and a short brief: what the knobs are, that it may change
  only `data/worldgen.txt`, and to answer in a sentence or two.
- Its tools are limited to reading the project and editing
  `data/worldgen.txt`. The file watch then regrows the chunk, so the
  answer and the change arrive together.
- The answer shows in a strip under the prompt box; a spinner while it
  works; Esc cancels.
- One Claude session is kept for the tool's run (`--resume` with the id
  from the first reply), so later prompts remember earlier ones. A key
  starts it fresh.
- Asked for something no knob covers (a new kind of room, a change to how
  a generator works), it changes nothing and says so, and the idea is
  noted for a chat session to build with a plan, `make check` and a commit
  (Q8).

## Open questions

1. **What it's for:** only tuning the knobs that exist, or later also
   drawing hand-made chunks or pieces (a set room, a plaza layout) that
   the generator can place? The plan covers tuning only; hand-made pieces
   would be a plan of their own.
2. **Saving:** changes stay in the tool until S writes them (my pick), or
   every change goes straight to the file? Either way a saved change
   alters the game, so `make check` fails and `make golden` follows,
   committed with it.
3. **Which knobs move in step 1:** every knob that shapes what is grown,
   about 120 of them (my pick), or a first set (the kind mix, rooms,
   plaza)?
4. **View:** the game's art, a flat map, or both with a key to switch (my
   pick)?
5. **Drop humans:** how many, how long by default, and which heatmaps
   matter most (walked, stuck, slept, died)?
6. **Ranges:** I would set each knob's range at about a half and twice its
   value, and you widen any in the file. Fine?
7. **F5 in game:** new values apply to chunks grown from then on (my pick),
   or F5 throws the world away and grows it again on the same seed?
8. **Ideas the prompt box can't do:** noted in `AI/worldgen_ideas.md`,
   with the seed and a screenshot name, for a chat session to pick up (my
   pick), or only shown in the answer strip?
9. **Which model the prompt box uses:** the CLI's default, or a faster,
   cheaper one (Sonnet or Haiku) since it only turns knobs (my pick:
   Sonnet)?
