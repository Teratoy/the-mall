# Water dispensers

Server rooms are warm, humming and locked behind two keys, and the only
water in a data centre is its washroom toilets. A water dispenser (an
office water cooler: a big upturned jug on a stand with a tap) stands in a
server room. It holds water that never comes back on its own, but a human
can pour a bottle into it. So it is a store of clean water behind locked
doors: drunk down by whoever gets in, and kept up only by someone who
carries water there.

It builds on server rooms (`AI/server_rooms_plan.md`, `gen_server.cpp`),
thirst and bottles (`human_thirst.cpp`, `human_bottle.cpp`), and copies
how vending machines are placed, seen and remembered (`vending.h`,
`gen_vending.cpp`, `human_vending.cpp`, `Memory::machines`).

## User's decisions (2026-10-31, from the brief)

- **In server rooms.**
- **New sprites.**
- **Holds 3 thirst bars of water**: 3.0 in bottle units, as a full bottle
  is 1.0 and takes thirst from 1 to 0.
- **Never refills on its own.**
- **Refilled from bottles**: a human pours its bottle in.

Answered (2026-10-31):
- **One per server room**, closets too, against a north or side wall,
  clear of the door, cables and racks.
- **Starts with 1 to 3 bars**, never found dry. **Blocks walking, not sight.**
- **Refilled to keep water safe**: one who has learnt water is scarce and
  sleeps among the servers stores water there, behind its locked doors;
  and one who has **settled** in a server room keeps it full as its home
  water.
- **A whole bottle goes in**, as far as it fits, and **only water it would
  drink itself**: never foul or bloody. So a dispenser's water is always
  clean.
- **Remembered** at the level last seen (the jug is see-through), **told**
  like water tips, and **guarded** by a settler like any water.
- **Better than a basin**: cool, clean water, preferred over basins and
  pools when the walk is close; same gulps, standing at the tap. Bottles
  fill there too, draining it.

## Built (2026-10-31)

World:
- `src/world/dispenser.h` (`Dispenser`, `DISPENSER_HOLDS` = 3), `Tile::Dispenser`
  (blocks walking, not sight). `drinkable()` now takes in dispensers; the new
  `pooled()` is open water only (pools, canals, basins), used wherever blood,
  filth, washing and relieving go into water, so a dispenser's water stays
  clean. `World::dispenser(cell)`, `dispensers()`, `drawWater`, `pourIn`.
- `src/world/gen_dispenser.cpp`: one per server room, closets too, called at
  the end of `furnishServerRoom` (one line in `gen_server.cpp`): a cell
  against the north, west or east wall, not on a cable, more than 2 cells
  from the inner door, every room cell round it floor, so it never walls
  anything off. Its own dice (from the world seed and the room), 1 to 3 bars.
- Drawn y-sorted with the rest (`src/render/dispenser_art.*`, sprites
  `water dispenser`, `water dispenser side`, mirrored against an east wall):
  the jug's water rows show the level, 2 rows a bar; remembered ones dimmed
  at the level last seen. Map symbol `map dispenser`, legend COOLER.
  `make spritesheet` draws every level.

Humans (`src/human/human_dispenser.cpp`, state in `dispense_`):
- Seen: `Memory::dispensers` keeps the level seen; the first one ever:
  "FOUND A WATER DISPENSER", `[first_dispenser]`. Remembered dry, it is not
  water to it (`waterBeside`). Found or drunk dry when it came for it or
  keeps it: "FOUND THE WATER DISPENSER DRY", `[dispenser_dry]`.
- Drinking: standing at the tap (the pointing pose), the same gulps as at
  a basin, each taken out of the jug. "DRANK FROM A WATER DISPENSER",
  `[dispenser_drink]`. Knowing one with water in it, other water counts as
  6 cells farther (`COOLER_PREFER`). Bottles fill there too ("FILLED MY
  BOTTLE AT A WATER DISPENSER"), a share at a time out of the jug.
- Keeping one: settled, the nearest it knows within 6 cells of home (and
  settling now counts a dispenser as water to make a home by); or, having
  learnt water is scarce, the one in the server room it falls asleep in
  ("MEANS TO KEEP WATER HERE, BEHIND THE LOCKED DOORS"). Not thirsty, with
  a clean bottle at least half full and room for half a bar in its
  dispenser, it walks there (Want::Fill) and pours the bottle in (the
  pouring pose, `Action::Pour`): "POURED MY BOTTLE INTO THE WATER
  DISPENSER", `[refill_dispenser]`. It never fills its bottle at the
  dispenser it keeps.
- Told: a water tip to a dispenser carries the level the teller saw.
  Guarded: a settler's home water within 2 cells counts a dispenser.
- HUD status: DRINKING AT THE COOLER, FILLING UP AT THE COOLER, POURING
  INTO THE COOLER.

Tests: scenarios cooler-drink, cooler-dry, cooler-fill, cooler-keep,
cooler-home; hum-out now empties its room's dispenser so the human still
has to push out to drink. The soak prints a `dispensers:` line per seed.
Soak: 30 dispensers in 2 of 6 worlds, 1 drink, 1 bottle filled; seed 2
(beside a data centre) now has nobody die of thirst (was 1) and lives
longer; no new stuck humans, loops or blocks (target flips 6 → 4).

Calls made while building (for the user to check):
- **Clean means clean**: "only water it would drink itself" is read as no
  fouler or redder than 0.05, and the dispenser keeps no dirt at all.
- **A bottle at least half full**, with **room for half a bar** in the
  dispenser, is worth the walk to pour in.
- **Never fills its bottle from the one it keeps**, or it would pour in and
  draw out in a loop.
- **"Better than a basin"** is 6 cells of walking.
- Keeping water behind locked doors costs keys to get back in, so the
  scarce sleeper pours in when it next sleeps there or wakes there.
