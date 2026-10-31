# Sprite editor

A local web page for redrawing the game's sprites, shaped like the lines
editor (`AI/lines_editor_plan.md`): stdlib Python in `tools/`, a page in
`tools/sprite_editor/`, a launcher `sprites.sh`, `make sprites`.

## Where things stand

Sprites are drawn in code. Most are glyphs, rows of letters, each letter
mapped to a colour by an `ink()`:

- `render/vending_art.cpp`: the machines' front and side, the sign and
  flank pictures and the goods behind the glass, per machine (cake,
  bottle, gun). Some inks change in play: the LEDs blink, the body and
  shade are each machine's own.
- `render/gun_art.cpp`: the gun.
- `render/toilet_art.cpp`: the toilet, turned to its wall, streaked with dirt.
- `render/world_view.cpp`: key, cake, heaped cake, spikes and plates.
- `render/map_view.cpp`: the map's 5x5 one-colour symbols.
- `render/icon_art.cpp`: the spiral icon.
- Not glyphs: humans (`human_look.cpp`, `human_sprites.cpp`, built from
  rolled colours), floors, walls, statues, tables, blood and mess, which
  are procedural.

## Proposal

- **`data/sprites.txt`** holds each glyph sprite as a block: its name, a
  comment line, its inks (letter, then a hex colour, or `@role` for an ink
  the game fills in, such as a machine's body or a blinking LED) and its
  rows, `.` see-through. The game reads it at start, F5 reloads it with
  the lines, and the APK ships it like `lines.txt`. The art code reads the
  rows and inks from it instead of its constants; what the code does with
  them (dimming, turning, dirt, blinking) stays in code.
- Sizes stay fixed by the code (a cake is 6x6): the editor draws inside
  that box, so nothing in the game moves or overlaps.
- Moving a sprite to data must draw it byte-identically: `make check`
  unchanged and `make shot` pictures equal before and after.
- The server makes small edits like the lines editor: one sprite block at
  a time, refused if it changed underneath, so it never overwrites another
  session's work. The page reloads when the file changes.

## Open questions (answered below)

1. Where editable sprites live: `data/sprites.txt` (F5 reloads), or the
   editor rewriting the `.cpp` arrays (rebuild needed)?
2. Which sprites first, and are humans in scope?
3. Which drawing tools?
4. How it previews?

## User's decisions (2026-10-31)

1. **Storage: `data/sprites.txt`.** Glyph rows and inks move to a data
   file the game loads; F5 reloads it with the lines, no rebuild. It ships
   in the APK like `lines.txt`.
2. **Scope: every glyph sprite.** The vending machines (front, side,
   goods), gun, toilet, key, cake, heaped and held cake, spikes, plates,
   the map's symbols, the icon's spiral and the sleeper's Z. Humans stay
   procedural for now.
3. **Tools:** pen, eraser, fill, eyedropper, palette swatches, grid, zoom,
   undo and redo; line and rectangle; editing the palette (change an ink's
   colour, add an ink); onion skin, another sprite of the same size shown
   faintly under the one being drawn.
4. **Preview:** at game scale (1x and 3x), beside a human for size, and
   live: the machines' LEDs blink as in the game, and a machine shows
   full and empty with its goods behind the glass.

A `make sprites` target and a `sprites.sh` launcher, like the lines editor.

## How it works (built 2026-10-31)

- `data/sprites.txt`: every glyph sprite as a block, grouped (Items,
  Traps, Toilets, Vending machines, Map, Other). Its header explains the
  format: `sprite NAME`, `# comments`, `ink L RRGGBB[AA] what`, `blink A B`,
  then the rows, `.` see-through.
- `src/render/sprites.{h,cpp}`: `sprites::get(name, w, h)` returns the
  sprite, reading the file the first time; `sprites::reload()` reads it
  again on F5. A sprite that is missing or the wrong size is reported on
  stderr and drawn as a magenta box; one broken by a later edit is kept as
  it was. References stay good across reloads.
- What the code still decides: each sprite's size, where it goes, what
  some letters mean (the toilet's H browns with dirt, its R and t streak;
  the map symbols draw every letter in their ink `#`, on a dark outline),
  and the goods' slots in each machine, since those are gameplay.
- The machines were three shared glyphs coloured per kind; each kind now
  has its own front, side and goods, with the sign, the flank picture and
  the coils drawn in, so each can be redrawn alone. Their LEDs blink as `o`
  and `O` (`blink o O`), swapped on odd blinks, as the checkerboard did.
- Key, cake and trap drawing moved from `world_view.cpp` to
  `render/item_art.cpp`.
- `tools/sprite_editor.py` (`make sprites`, `./sprites.sh`, port 8766)
  and `tools/sprite_editor/index.html`: the list of sprites by group, the
  drawing in the middle, the previews and inks on the right. Every stroke
  is saved straight away, one block at a time, refused if the block
  changed underneath; the page checks the file every two seconds.
- Keys: P pen, E eraser, G fill, I eyedropper (or right-click, or
  Alt-click), L line, O rectangle (Shift fills it), Ctrl+Z / Ctrl+Shift+Z
  or Ctrl+Y, `;` grid, + and - zoom (or Ctrl+wheel), 1-9 pick inks, up and
  down step through the sprites. Inks: click the swatch to draw with it,
  type a colour (double-click it for a picker), name it, `+ ink` adds one
  with a free letter, `×` removes one no pixel uses.
- Onion skin: any sprite of the same size, faintly under the drawing;
  "spikes" starts with "spikes up" under it, "plate" with "plate down".
- Previews, on a floor, black or light: 1x and 3x; at 3x beside a human
  (`tools/sprite_editor/human.png`); for a machine's front, side or goods,
  the machine full and empty with its goods in their slots
  (`tools/sprite_editor/ref.json`), its LEDs blinking 4 times a second.
  Both come from `make spriteref` (`build/sprite_sheet --ref`), so remake
  them when the machines' slots or the human's look change.
- Checking art stays the same: `make spritesheet` draws every sprite in
  every way the game does to `build/sprite-sheet.png`, to `cmp` before and
  after; `tools/art_snapshots.sh DIR` saves many shots (water glints follow
  the wall clock, so water pixels can differ between any two runs).

### Not done

- Humans: still procedural (`human_look.cpp`). A template with colour
  roles is the likely way, if wanted.
- Procedural art (floors, walls, statues, tables, blood, mess, the
  bottle, the cake held in the hand) has no glyph to edit.
- Sizes are fixed; making a sprite bigger needs the code changed with it.
