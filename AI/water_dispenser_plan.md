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

## Proposed, for the user to answer (see Open questions)

1. **The dispenser in the world**
   - `src/world/dispenser.h`: `struct Dispenser { Vec2i at; Vec2i facing; float water; float foul, red; uint32_t seed; }`,
     kept by `World` with a cell map like toilets and machines;
     `World::dispenser(cell)`, `dispensers()`.
   - `Tile::Dispenser`, one cell, blocks walking. Sight: see the question.
   - Used from the floor cell in front of it.
2. **Where** (`gen_server.cpp`, inside `furnish`, targeted): against a
   side or north wall of the room, never in front of the airlock's inner
   door, never on a cable, floor round it so it blocks nothing. Placed
   with the room's own dice after the racks and desk, so a mall without
   server rooms comes out as before.
3. **Drawn** (`data/sprites.txt`, sprite `water dispenser`, one cell wide,
   about 6x17 px, standing up like a terminal): a translucent blue jug on
   a white stand, two taps. The jug's water letters are drawn up to its
   level, so how full it is shows from across the room, as a bottle does.
   Dimmed and held at the remembered level when only remembered. A map
   symbol `map dispenser`.
4. **Seen and remembered**: `Memory::dispensers`, cell → the level and
   dirt it saw. First sight: a note and a `FirstDispenser` moment.
5. **Drinking from it**: `State::Drinking` as at a basin, standing at the
   tap in place of kneeling; each gulp takes the same DRINK_GULP out of
   the dispenser. It counts as water for `seekWater` while it remembers it
   holding some; drunk dry: a note and a moment. **Filling a bottle** at it
   works as at water and takes from it.
6. **Refilling it** (`State::Pouring` at the dispenser, or a new
   `Action::Refill` with rows in all three need tables): the bottle's water
   goes in (as much as fits), foul and red mixing in as in a bottle.
   Who does it and why: see the question.
7. **Telling**: see the question.
8. **Scenarios**: drink-dispenser, dispenser-dry, refill-dispenser,
   whatever telling or guarding we decide.

## Open questions

1. How many per server room, and do closets get one?
2. Where in the room: against a wall near the door, or anywhere?
3. Does it block sight? (It is about waist-to-chest high, like a terminal.)
4. Does it start full, or anywhere from empty to full?
5. How much does one bottle refill: the bottle's whole contents (up to a
   bar), dirt and all?
6. Why would a human pour a bottle in, rather than keep it?
7. Do humans remember its level and tell others of it?
8. How does drinking from it compare with other water (clean, preferred
   over toilets, standing at the tap)?
9. Can it be settled by, guarded and fought over like other water?
