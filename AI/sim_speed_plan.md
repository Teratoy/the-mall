# Sim speed

Where the sim spends its time, what was made faster without changing what
happens, and what could be made faster still, some of it only by letting
what happens change a little.

## How it was measured

- `make simprofile` (tools/sim_profile.cpp) runs one soak seed's world in
  one process, built the way the soak builds it, with none of the soak's
  watching, and prints the time a tick and a hash of where everyone ended.
  Two builds that print the same hash ran the same.
  `SIMPROFILE_ARGS="seed minutes crowd"`, default `4 15 6`.
- `make gprof` builds the same run with `-pg` into `build/pg/` (no drawing
  code) and writes gprof's flat profile and call graph to `build/gprof.txt`.
  It runs from the repo root, since `data/` is looked for beside the build
  folder: run from `build/pg/` it finds no names or lines and the world goes
  differently.
- There is no `perf` or `valgrind` in the toolbox, only gprof.
- gprof doesn't see into the soak's forked seeds (the profiling timer isn't
  inherited by fork). Hence the separate tool.

## Findings (2026-10-31, seed 4, the slowest: 21 humans, 15 chunks by the end)

Before, the sim alone took 84.7 s for 15 minutes, 3.1 ms a tick:

| Where | Share |
|---|---|
| `Human::searchFrom` (Dijkstra over memory) and what it calls | ~90% |
| — of which `Memory::tile` (a hash lookup per cell) | 19% |
| — the search's own `cost`/`from` hash maps | 11% |
| — blood and mess costs, traps, occupants, per neighbour | ~12% |
| `observe` (sight, memory) | ~2% |
| everything else | <8% |

The cause: **46% of searches found nothing, and those made 89% of the
search work**, because a failed search floods everything the human
remembers (~10,000 cells). Nearly all were `seekWater`, `seekFood` and
`seekToilet`, tried again and again by a human who knew of no water, cake
or toilet at all.

## Built (2026-10-31): no change to what happens

`make check` passes against the old golden output; each seed ends with the
same hash.

1. The search keeps costs, where it came from and whether a cell is open in
   per-slot grids reused from search to search by a stamp, not hash maps
   made each time. Each cell is judged open once per search; neighbours away
   from a slot's edge are found by offset. A neighbour already reached as
   cheaply is skipped before blood and mess are weighed (they only add).
2. `Memory::tile` keeps the last slot at hand. A copied Memory starts
   without it.
3. Memory counts the cells it knows of each tile (`Memory::known`). Seeking
   water with none known, a toilet with none known, or food with no cake
   known skips the search. It still draws the one die a search draws
   (`Human::searchedNothing`), so everything after goes as before.
4. The soak counts distinct cells with a sorted buffer, not a `std::set`
   built every tick for every human.

| | Before | After |
|---|---|---|
| Seed 4, sim alone | 84.7 s (3.14 ms a tick) | 8.1 s (0.30 ms a tick) |
| Seed 2, sim alone | 67.6 s | 8.5 s |
| Cells expanded, seed 4 | 89.6 M | 25.4 M |
| `make check` (16 cores) | ~70 s | ~15 s |

## What is left (seed 4, after)

| Where | Share |
|---|---|
| Search | ~70%: mostly `pursue`'s explore search (succeeds, 6,854 of them), and its door and key searches, which still fail ~750 times and flood |
| `observe` (shadowcast sight, writing memory) | ~21% |
| `move`, `update`, the rest | <10% |

## Proposed next steps

### No change to what happens (would do without asking, but listed)

- **A1. Other searches that can't succeed.** The same shortcut for
  `pursue`'s searches for a locked door (`lockedDoorBeside`: none known if
  `known(DoorLocked) == 0`) and for keys (`memory.keys` empty and no cake
  wanted). Also `seekFill`, `offerDrinkEscort` and `askGroupForWater`
  (no water known). Maybe 10–20% more.
- **A2. Ask `done` once per cell.** `searchFrom` calls `done(n.at)` twice
  for every cell it takes off the queue. The `done` lambdas are pure but for
  writing the same value twice (`shied`, `door`, `whose`), so once is
  enough. Maybe 5%.

### Would change what happens: need your answer (then `make golden`)

- **B1. Don't search again for what wasn't there.** A human whose search
  for something failed waits until its memory gains a cell of that kind
  (or a few seconds pass) before searching again. A search fails the same
  way while nothing it knows has changed, except where it depends on
  others: who stands in the way, who it's avoiding. So now and then a human
  would notice a way open a moment later. Large saving where explore and
  door searches flood.
- **B2. A\* for a search with one known goal** (`c == target`). Same path
  lengths, but where two paths are equally good it may pick the other one,
  so every soak world drifts from the golden output.

### Questions

1. Go ahead with A1 and A2? They keep `make check` unchanged.
2. Is B1 acceptable: a human that just failed to find something doesn't
   look again until it learns something new or a few seconds pass?
3. Is B2 acceptable: changing which of two equally good paths is taken?
