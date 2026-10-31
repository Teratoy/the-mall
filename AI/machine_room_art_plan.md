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

## Open

- Where these stand in the world (a machine-room chunk interior?), and
  whether humans can use them.
