# Machine room art

Old-school computer furniture, art only: nothing is placed in the world or
does anything yet.

## User's decisions (2026-10-31)

- **Server rack**: an old machine-room rack. Built as two cells wide, 25 px
  tall (a head over a human), drawn front-on from its foot like the vending
  machine, with a side view. No door, so the lights show.
- **CRT terminal**: one cell, standing on its own pedestal, front-on from
  its foot, a little shorter than a human.
- **Console desk**: 6 cells wide and 4 deep (first 3 deep, then made
  bigger), against a wall, set up for one human, with a swivel chair. No
  function for now; the look should say "one operator's station". Not
  geometric: realistic and lively, not a row of the same box.
- **Cable decals**: 1-cell floor tiles. Pieces: 3 straight looks that meet
  seamlessly so a run can mix them, each turned either way; a cross; 4
  corners; 4 T-joins. No end pieces. First a bundle of 2-3 cables; then
  only the black cable, with the ties scaled down to it.

## Built (2026-10-31)

- `src/render/server_rack_art.h/.cpp`: `drawServerRack`, `drawServerRackSide`.
  A tape drive whose hubs turn in bursts, blinking status lights, two
  drives, a scrolling green screen, keyboard, power unit.
- `src/render/computer_art.h/.cpp`: `drawTerminal` (6x13, bottom-left of
  its foot) and `drawConsoleDesk` (36 px wide, its footprint's top-left;
  the screens rise `CONSOLE_RISE` px above). The desk has a wing coming
  forward each side, inner edges slanting in; a small screen turned in, a
  big main screen, an instrument box with a sweeping round scope on a lamp
  panel, a printer with green-bar paper curling out, a mug, a keyboard set
  askew, a switch panel, a floppy, a red phone, loose sheets, a chair
  turned a little. The screens stand on the desk on feet, casting a shade,
  with their deep case tops seen from above and a glint on the glass.
- `src/render/cable_art.h/.cpp`: `drawCable(x, y, links, variant, lit)`,
  `links` a mask of `CABLE_N/E/S/W`. A 2 px black cable; straights are
  plain, sagging, or tied. A T or cross lays the branch over the run's
  last two pixels, so the cables meet.
- Previews: `tools/server_rack.cpp` (`build/server_rack.png`) and
  `tools/computers.cpp` (`build/computers.png`, with a small room wired up).
  Built by hand; see their header comments.

## Server rooms (2026-10-31)

The user asked for three server rooms of different sizes, as layouts to
look at first; world generation comes later, with its own questions.
Sizes picked: a 6x5 closet, a 12x9 room and a 20x14 hall (inside the walls).

`tools/server_rooms.cpp` (`build/server_rooms.png`) draws them from maps of
letters at the top of the file, so a layout is changed by editing its map:
- **Closet**: two racks and a terminal against the back wall, wired
  together and out through the side wall.
- **Room**: three racks and the console desk along the back wall, a second
  row of two racks and a terminal, cables under each row joined by a trunk
  and leaving through the right wall, perforated tiles in the cold aisle.
- **Hall**: racks either side of the console desk along the back, a long
  second row of nine racks, three terminals in front, a trunk down from the
  back row to the collector run.

Seen from the front, a rack hides the four rows behind it, so rows of racks
stand five or six rows apart, or the aisle between them disappears.
Cables run on the floor in front of each row; where a run ends, it goes
into the machine or wall it reaches.

## Open

- Where the server rooms go in the world (as chunk interiors?), how often,
  whether the machines block their cells, and whether humans can use them.
