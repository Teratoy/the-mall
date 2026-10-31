# Vending machines

Cakes lie on tables and never come back once eaten; water stays put and
bottles only come with humans. A vending machine is a third kind of place
that holds food or a way to carry water: seen from far off (lit, its stock
behind the glass), remembered, fought over, and told of or kept secret.
It ties into hunger (`seekFood`), thirst and bottles (`human_bottle.cpp`),
hoarding (it is a stash nobody owns), and tips and lies (`Memory`, `tell`).

The art is done (`src/render/vending_art.*`, preview `tools/vending.cpp`):
a front view 12x21 px, two cells wide, and a side view 9 px deep. Cake
machines hold 6 cakes, bottle machines 6 empty bottles. Gun machines are
out of scope: guns have no gameplay yet, and none are placed.

## User's decisions (2026-10-31)

- **Paid in keys**: a cake or a bottle costs **3 keys**, fed into the
  machine and gone. So a cake costs a way out, and keys get a second worth.
- **Restocks one item a day**, a day being **20 minutes** of world time
  (about a waking stretch and a sleep). Cake and bottle machines alike.
  Machines start with **2 to 6** of their 6 slots full.
- **Buttons only**: no breaking in, shaking or tipping for now, and the
  machine never fights back.
- **Rooms and plazas, uncommon**: about one chance in three per slot in
  room networks and plazas, cake or bottle at even odds, now and then a
  pair side by side. None in the spawn room or in mazes. No gun machines.
- **North walls and side walls**: against a north wall it faces the viewer
  (front view, its stock behind the glass); against a west or east wall it
  faces into the room and shows its flank (side view, mirrored for an east
  wall). Never against a south wall.
- **Learnt like cakes**: seen (remembering the stock behind the glass as it
  was then), and told at greetings as cakes are: shared with friends, sold
  by the greedy, kept from rivals by the cunning.
- **Bought only when hungry**: hungry, with 3 keys in hand and no cake it
  knows of nearer, it buys one; starving, even when a known cake is a bit
  nearer. The greedy also buy to hoard; the curious hold on to their keys
  longer for doors. A bottle only by a human with no bottle, thirsty or
  wanting one to fill.

## 1. The machine in the world

- `src/world/vending.h`: `struct Machine { Vend kind; Vec2i at; Vec2i front; bool side; bool flip; uint8_t stock; uint32_t seed; }`.
  `at` is its left (or top) cell, `front` the floor cell it is used from.
- `Tile::Vending`: blocks walking, not sight, like a table or toilet. Two
  cells along the wall, both `Tile::Vending`.
- `World::machine(cell)`, `World::machines()`, `World::vend(cell)` takes the
  next item out (lowest set bit of stock).

## 2. Where they appear (`src/world/gen_vending.cpp`)

- Against a straight stretch of wall, the two cells in front of it floor
  and the cells round it floor, so it blocks nothing (as toilets do).
- Which chunk kinds, which walls, how many: see the open questions.

## 3. Seeing and remembering

- `Memory::machines`: cell → the stock it saw behind the glass, updated
  whenever it sees the machine. Planned on, never the world's truth.
- The map shows remembered machines (`map_view.cpp`), the world view draws
  remembered ones dimmed and still with the stock it remembers.
- First sight of a machine: a note and a `FirstVending` moment.

## 4. Using one (`State::Vending`, `Action::Vend`)

- Standing on its front cell, it works the machine for VEND_TIME; an item
  drops into the tray and it takes it (a cake into `cakes`, a bottle into
  `bottles`).
- A row in all three need tables in `human_tuning.h`.
- Log: "GOT A CAKE FROM A MACHINE", "GOT A BOTTLE FROM A MACHINE",
  "THE MACHINE WAS EMPTY"; moments vend_cake, vend_bottle, vend_empty.

## 5. Choosing it

- Hungry: a machine it remembers with cakes in it is a cake spot for
  `seekFood` and `Want::Fetch`, at the same cost as a cake on a table.
- Bottles: see the open questions (one bottle at most is carried now).

## 6. Telling

- A new tip kind, `Kind::Machine`, told at greetings like cakes and water.

## Build order

1. Machine struct, tile, generation, drawing, map; `make genprobe`, a shot.
2. Memory, using one, choosing it; the soak.
3. Telling; scenarios; `make golden`.

## Open questions

- None blocking: all seven were answered above (2026-10-31).

### Built (2026-10-31, all steps)

- `src/world/vending.h` (Machine, MACHINE_PRICE, MACHINE_DAY; the `Vend` enum
  moved here from the art header), `src/world/gen_vending.cpp`
  (placeMachines, standMachine), `World::vend`, `World::updateMachines`,
  `Tile::Vending`. `src/human/human_vending.cpp` (machinePath, buyCakePath,
  hoardBuys, buyBottle, goBuy, startVending, takeVended, seeMachine).
  `State::Vending`, `Goal::Vend`, `Action::Vend` (rows in all three need
  tables), `Memory::machines` (kind and stock as last seen), `Tell::Kind::Machine`.
- Drawn y-sorted with humans and statues: front view against a north wall,
  side view against an east wall, mirrored against a west one
  (`drawVendingMachineSide` got a `flip`). The map shows remembered
  machines; its legend now counts its rows from measured text, as MACHINE
  made it need a fourth on the phone.
- Moments `first_vending`, `vend_cake`, `vend_bottle`, `vend_empty`,
  `tell_vending`, `sell_vending`. Log: "FOUND A MACHINE THAT TAKES KEYS",
  "PAID 3 KEYS FOR A CAKE (BOTTLE) FROM A MACHINE", "THE MACHINE WAS EMPTY",
  "TOLD @ WHERE A MACHINE IS".
- Scenarios: vend-cake, vend-bottle (side on), vend-empty, tell-vending,
  sell-vending. The soak prints a `machines:` line per seed. `make genprobe`
  checks every machine has wall behind it, never faces away, and can be
  reached. `make shot SHOT_ARGS="--vending"` shows one, `--map` opens the map.
- Calls made while building (for the user to check):
  - **Machines are placed with dice of their own** (from the world's seed
    and the chunk), so the rest of the mall comes out as it did before
    machines; the soak only moved by what machines do.
  - **A machine tip comes with the way there**, as water tips do: without
    it a hungry listener could never path to a machine out of sight.
  - **Selling word of a cake machine can't happen**: those it would help
    are hungry with no cake to pay with. Word of a bottle machine is sold.
  - **Keys are paid only when something drops**: finding it empty costs
    nothing.
  - Working a machine uses the pointing pose (an arm out to the buttons).
  - The greedy (0.75 and up) buy to hoard only when there is nothing else
    to fetch; the curious (0.7 and up) buy only when starving or knowing of
    no other food. A starving human goes to a machine up to half again as
    far as the nearest food it knows.
  - Humans remember a machine as they last saw it, so one they saw empty
    is not gone back to after it restocks, until seen again.
- Soak (6 worlds): 27 machines, one bottle bought; no new stuck humans,
  loops or blocks. One new target flip (a human with no keys between
  exploring and a far cake), from the worlds drifting apart, not from vending.

