# Computers plan

Server rooms hold small CRT terminals (one cell) and a big console desk (6x4
cells), drawn in `src/render/computer_art.cpp`. Today a human only remarks on
the room as a whole (`FirstServerRoom`). The user wants curious humans to try
the computers: the first time they see each kind (once the terminal, once the
desk) they walk up close and try to use it, to learn something about the
place and what is going on. Every computer is locked by a password. Later
they may try again on a different computer, never one they already tried, and
find it locked again, with a different line each time.

## What is there to build on

- `human_inspect.cpp`: `discover(Find)` queues a first sight while exploring
  or wandering; `inspectNext()` walks the human to a free cell beside it
  (`Goal::Inspect`), `lookAt()` faces it, stands 0.6 s to 4 s by curiosity
  and says the find's line there. Pressing needs, fear, fights, escorting and
  running all turn it aside (`freeToInspect`).
- `World::pieces()` / `World::piece(cell)`: every rack, terminal and desk,
  with its kind and cell. Humans don't remember single pieces yet, only the
  room (`Memory::serverRooms`).
- No new art yet: no typing pose, no lit password screen.

## Proposal

- **Seeing a computer.** In `seeServerRoom()` (or a new `seeComputer()`), a
  terminal or desk in sight is remembered in `Memory` (`computers`, by its
  cell, with its kind). Remembered only as seen, so nobody reads the world.
- **First try of each kind.** The first terminal and the first desk a curious
  enough human sees become a find for `discover()`, like the other first
  sights, but on arriving it doesn't just look: it **tries the computer**.
  It stands at the desk's chair / in front of the terminal, faces it, and
  stays a while in a new state with a new `Human::Action::Type` (rows in the
  hunger, thirst and tiredness tables, about like `Rest`/`Sit`). Then the
  password stops it: log `TRIED A TERMINAL, LOCKED BY A PASSWORD` /
  `TRIED THE CONSOLE DESK, LOCKED BY A PASSWORD`; moments `TryTerminal` and
  `TryDesk`, said there.
- **Remembered tries.** The cells of computers it has tried are kept in
  `computers_` (a new struct beside the system, `human_computer.cpp`). A
  tried one is never tried again.
- **Trying again.** Later, exploring or wandering past an untried computer it
  knows, with nothing pressing, a curious human may want to try once more: a
  new `Want::Computer` (row in WANTING), or a find again via `discover()`.
  Each later try gets its own moment, `TryAgain`, with lines of their own, and
  says a different line each time (no repeat of a line it already said on a
  computer while lines remain).
- **Scenarios.** `computer-terminal` (curious human in a spawn room set by a
  terminal: `TRIED A TERMINAL` follows `WENT CLOSER`), `computer-desk`, and
  `computer-again` (tries a second terminal, never the first; the two lines
  differ), plus `computer-incurious` (walks past, no try).

## Open questions

1. **How curious is curious enough** to try a computer at all? (The airlock
   lesson uses 0.65 for "curious"; inspecting has no floor, only reach.)
2. **Does trying cost time or anything else?** How long does it stand at it
   (e.g. 3 s to 8 s by curiosity)? A new action `Type` costing like sitting,
   or more (the mind working)? Any other cost: a key, nothing?
3. **Do they remember which computers they've tried?** (Proposed: yes, by
   cell, so never the same one twice.) Do they also remember the passwords
   don't work, so it becomes a lesson?
4. **How often and why do they try again?** Only when walking past an untried
   one while exploring or wandering; or do they make for one on purpose
   (hope, boredom, being stuck)? Does it stop after a while: after N failed
   tries, or does the want fade each time, or never stop for the most
   curious?
5. **Is anything learnt at all?** Nothing, or a feeling: hope (something out
   there runs this place), dread (it's all locked away from them), a nudge to
   the hope system (`human_hope.cpp`)? Different by trait?
6. **Do they tell others?** E.g. a new Tell ("THE MACHINES WANT A PASSWORD"),
   so a told human tries less, or still tries?
7. **Do onlookers react** to someone at a computer: watch, comment, come
   over, or nothing?
8. **On screen:** does the screen light up differently while being tried (a
   password prompt, `****` blinking), and does the human get a typing pose
   (hands forward at the keys), or does it just stand idle facing it?
9. **Art:** if there's a prompt look, should it go in `data/sprites.txt` (a
   new small sprite drawn over the screen) or in `computer_art.cpp`, and
   should `make shot` get a scene showing it?
10. **The desk:** where does it stand to use it: in the chair (the cell under
    the desk's middle, already walkable), or anywhere in front?
11. **Lines:** about how many lines for the first terminal, the first desk,
    and the later tries? Should a try-again line differ by whether it was a
    terminal or the desk, or be shared?

## User's decisions

(to fill in)
