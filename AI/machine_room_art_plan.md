# Machine room art

Old-school computer furniture, art only: nothing is placed in the world or
does anything yet.

## User's decisions (2026-10-31)

- **Server rack**: an old machine-room rack. Built as two cells wide, 25 px
  tall (a head over a human), drawn front-on from its foot like the vending
  machine, with a side view. No door, so the lights show.
- **CRT terminal**: one cell, standing on its own pedestal, front-on from
  its foot, a little shorter than a human.
- **Console desk**: 6 cells wide and 3 deep, against a wall, set up for
  one human: a row of six screens across the back (the outer two turned in
  towards the seat), and a swivel chair in front. No function for now; the
  look should say "one operator's station".
- **Cable decals**: 1-cell floor tiles, a bundle of 2-3 cables. Pieces: 3
  straight looks that meet seamlessly so a run can mix them, each turned
  either way; a cross; 4 corners; 4 T-joins. No end pieces.

## Built (2026-10-31)

- `src/render/server_rack_art.h/.cpp`: `drawServerRack`, `drawServerRackSide`.
  A tape drive whose hubs turn in bursts, blinking status lights, two
  drives, a scrolling green screen, keyboard, power unit.
- `src/render/computer_art.h/.cpp`: `drawTerminal` (6x13, bottom-left of
  its foot) and `drawConsoleDesk` (36 px wide, its footprint's top-left;
  the screens rise `CONSOLE_RISE` px above). The desk carries a mug, a
  switch panel each side of the keyboard, and a stack of green-bar printout.
- `src/render/cable_art.h/.cpp`: `drawCable(x, y, links, variant, lit)`,
  `links` a mask of `CABLE_N/E/S/W`. The bundle is a thin putty cable each
  side of a thick black one, symmetric so corners keep the lanes. Straights
  are plain, sagging, or tied. A T or cross lays the branch over the run's
  last two pixels, so the thick cables meet.
- Previews: `tools/server_rack.cpp` (`build/server_rack.png`) and
  `tools/computers.cpp` (`build/computers.png`, with a small room wired up).
  Built by hand; see their header comments.

## Open

- Where these stand in the world (a machine-room chunk interior?), and
  whether humans can use them.
