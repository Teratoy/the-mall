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

2026-10-31:

1. Curious enough: `traits.curious` 0.5 and up. The curious walk farther and
   stand longer, as with inspecting.
2. A try lasts 3 s to 8 s by curiosity, a new `Action::Type` costing about
   what sitting does. No key or other cost.
3. Remembered by cell: a computer tried is never tried again.
4. Tried again only in passing: an untried computer in sight while exploring
   or wandering, nothing pressing. Each failed try halves the chance of the
   next, so it dies out after a few.
5. A feeling only: the first try gives a little hope (someone runs this
   place), each later lock a little dread.
6. Not told to others.
7. Curious onlookers (0.5 and up) who see someone at a computer comment on
   it, once a life each.
8. A typing pose (hands forward at the keys), and while it types the screen
   shows a small password prompt with blinking stars, drawn in
   `computer_art.cpp`; a `make shot` scene shows it.
9. The desk is used from its chair (the walkable middle cell under its
   screens), facing up.
10. Lines: about 5 each for the first terminal and the first desk; one shared
    try-again moment with about 8, never the same line twice while any are
    left.

The onlooker's floor is the same 0.5 as trying (not asked separately).

## What was built

- `src/human/human_computer.cpp`, state in `Human::computers_`: every
  terminal and desk seen, by cell, weighed once: the first of its kind
  `First` (always tried), later ones `Meant` or `Passed` by a roll of
  0.5^tries; `Tried` once typed at, never again. After a try, the other
  `First` ones are weighed again, so a second terminal in the same room
  gets the roll too.
- Seen in `observe()` (`seeComputer()`), a meant computer within reach (10
  steps, 5 to 15 by curiosity) becomes a `Find` with `rig` set, walked to by
  the inspect system to where it is used from (`ServerPiece::usedFrom()`: a
  terminal from either side, the desk from its chair cells). Not got to (too
  far, unreachable, 15 s, something pressing) it says nothing and lets
  computers be for 10 s.
- There: new `State::Typing` and `Action::Type` (rows in all three tables,
  costed as sitting), 3 s to 8 s by curiosity, looking round as it types.
  Something pressing (`freeToInspect`) breaks it off: `LEFT THE COMPUTER,
  SOMETHING MORE PRESSING`, no line. Done: `TRIED A TERMINAL. LOCKED BY A
  PASSWORD` / `TRIED THE CONSOLE DESK. ...` / `TRIED ANOTHER TERMINAL. A
  PASSWORD AGAIN`, its line said there (`sayHere()`, shared with
  `lookAt()`), the first try lifting hope 0.05, later ones dread 0.05.
- Log `STEPPED UP TO A TERMINAL TO TYPE` / `TOOK THE CHAIR AT THE CONSOLE
  DESK TO TYPE` as it starts; an onlooker: `WATCHED @ TRY A COMPUTER`.
- Moments `TryTerminal`, `TryDesk`, `TryAgain` (10 lines; `talk::pick`
  prefers the line said longest ago, so later tries vary), `WatchTyping`.
- Art: `Pose::Type` (`Arm::TypeUp/TypeDown`, forearm out at the waist,
  the hand tapping at 5 a second); while someone in sight types, the
  terminal's screen, or the desk's big screen before the chair, shows a
  prompt line and a field of stars filling in, flashing refused when full
  (`prompt()` in `computer_art.cpp`). On the same row machines now draw
  before humans, so the typist sits in front of the desk's chair.
- `make shot SHOT_ARGS="--typing terminal"` or `"--typing desk --seed 1"`.
- Scenarios `computer-terminal`, `computer-desk`, `computer-hungry`,
  `computer-incurious`, `computer-never-same`, `computer-again` (two later
  tries, two different lines), `computer-watch`. `cooler-fill` now starts
  with `seenItAll`, as it got pulled aside to a terminal. `Stage::run` takes
  a callback run after each step.
- Soak: unchanged but for `Typing` in "states never seen": in the 6 soak
  worlds no curious human gets into a server room. `make coverage` leaves
  only `TRIED ANOTHER CONSOLE DESK` unreached (a room has one desk).
