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
