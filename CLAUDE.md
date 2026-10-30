# The Mall

A 2D emergent sandbox in C++20 and raylib 5.5. Humans wake up in an
endless mall with no memory and try to get out. Nobody controls them.
They explore, and get hungry, thirsty, tired and needing to go. They take
keys and cakes from tables, open doors, trust and lie to each other,
fight, bleed, settle and die. What happens comes from rules, not
scripts. The player makes one human and follows it in a phone-portrait
view.

The user designs the game and builds it up step by step with agents.
Gameplay decisions are theirs: when a design question isn't settled in a
plan, ask, and write the answer into the plan.

## Build, run, test

The host (Bazzite) has no dev headers. Build inside the `claude-build`
toolbox, which has raylib-devel. Its sudo needs the user's password, so
don't try to install anything.

```bash
toolbox run -c claude-build make -j8          # build/themall
toolbox run -c claude-build make check        # soak vs golden output (~1 min)
```

| Command (inside the toolbox) | What it does |
|---|---|
| `make` | Builds `build/themall`. Zero warnings is the norm: keep it so |
| `./play.sh` (from the host) | Rebuilds if needed and opens the game window |
| `make check` | Runs the scenarios, then the headless soak (`build/stuck_soak 6 15 1 6`), diffing it with `tests/golden/soak.txt` |
| `make scenarios` | The scenarios alone (3 s): set-up situations that check rare features still happen. `build/scenarios NAME` runs one and prints every log line |
| `make golden` | Rewrites the golden output after a change meant to alter behaviour. Commit it with that change |
| `make coverage` | Lists log lines (`note(...)`) and talk moments that neither the soak nor the scenarios reached |
| `make stuck` | The soak with its own arguments: `build/stuck_soak [seeds] [minutes] [verbose] [crowd]`. `TRACE=seed:id:from:to` follows one human |
| `make shot SHOT_ARGS="..."` | Saves `build/shot.png` without a visible window. See `tools/shot.cpp` for the options (`--scene`, `--seconds`, `--seed`, `--zoom`, `--stats`, `--log`, `--reveal`) |

**The sim is deterministic.** Same seeds, same output, byte for byte. So:
- A pure refactor must pass `make check` unchanged. If it doesn't, the
  refactor changed behaviour. Find out why; don't run `make golden` over it.
- A behaviour change will fail `make check`. That's expected. Look at the
  diff to see that it changed what you meant (no new stuck humans, loops
  or blocks), then `make golden`.
- Keep it deterministic. All dice go through the human's `rng_` or the
  world's Rng. Never use `std::rand`, the clock, or the order of a
  container keyed by pointers to decide anything.
- The soak only covers what 6 short worlds happen to do. Rare features
  get a scenario in `tests/scenarios.cpp`: a few humans set up in a fresh
  spawn room (traits picked by seed with `seedWhere`, needs, trust and
  memory set through `SoakProbe` in `tests/probe.h`), run for a minute or
  so, then checked by the lines in their logs. When you build a feature
  the soak seldom reaches, add a scenario for it. `make coverage` lists
  what is still unreached.
- A scenario marked `known` is a gap the tests found and the user hasn't
  decided on yet. It's reported but doesn't fail `make check`. Once it
  passes, drop the note.

Put probes and tools in `tools/`, not in a scratchpad. Scratchpads are
lost, and several useful probes already have been.

## Where things are

- `src/human/human.h`: the Human class. Each system's state is a struct
  beside that system's methods (`voice_`, `chat_`, `social_`, `fight_`,
  `deaths_`, `mind_`, `rescue_`, `escape_`, `gore_`, `escorting_`,
  `water_`, `sleep_`, `relief_`). State the core loop uses everywhere
  stays loose (`rng_`, `clock_`, `path_`, `rethink_`, `dread_`, ...). New
  state for a system goes in its struct.
- `src/human/human.cpp`: update, needs, `choose()` (reflexes first, then
  scored wants) and `pursue()`.
- `src/human/human_tuning.h`: the tuning shared by human.cpp,
  human_walk/social/fight/speech.cpp, including the per-action hunger,
  thirst and tiredness tables and the WANTING table. Other systems keep
  their tuning at the top of their own `human_*.cpp`.
- `src/human/human_<system>.cpp`, one file per system:
  - walk: walking and search
  - social: trust, greeting, tells, lies, names
  - fight: cakes, fights, hits, looting
  - speech: lines and the log
  - blood, thirst, relief, sleep, flesh, hope, rescue, company, chat, topic, episode
- `src/world/`: chunks and generation (`gen_*.cpp`), blood, mess, traps,
  tables, statues, toilets.
- `src/render/`: pixel art drawn in code; `src/scenes/`: menu, create,
  world (HUD, log, stats), obituary; `src/ui/`: the pixel UI.
- `data/lines.txt`: everything humans say. The user edits it, and F5
  reloads it in game. Its header explains the format.
- `data/names.txt`: what names are grown from.
- `data/obituary.txt`: the player's save file. It isn't tracked by git,
  so don't commit or reset it.
- `AI/`: plans, one per feature, with the user's decisions and what was
  built. Start at [AI/README.md](AI/README.md).
- `tests/stuck_soak.cpp`: the soak; `tests/scenarios.cpp`: the scenarios.
  Both reach Human's and World's internals through the `SoakProbe` friend
  in `tests/probe.h` (add accessors there); `tools/shot.cpp` uses
  `ShotProbe` the same way. Never `#define private public`.

## Rules the user set

- Every action costs hunger, thirst and tiredness. A new `Human::Action`
  needs a row in all three tables in `human_tuning.h`.
- Humans know only what they've seen or been told (`Memory`). No human
  reads the world's truth.
- Every new behaviour adds a `note()` line to the life log and a
  `sayLater()` moment. New moments go at the end of `talk::Moment`, with
  lines in `data/lines.txt`.
- HUD: everything stays inside the two shaded strips, laid out from
  measured text (`ui::textRight`, `ui::textCentred`, `ui::wrap`). Never
  let text float free or get cut off. New per-human stats go in the stats
  menu (`WorldScene::drawStats`), not the top strip.
- Art: 1 cell is 6 px. Human sprites are 15x18. The canvas is 216x468
  (phone portrait). Bodies of water are basic shapes, turned only in 90°
  steps, never with a diagonal edge.
- Search goal costs are in tenths of a cell (10 per straight step), which
  is easy to get wrong.
- Before a feature of any size, write or extend a plan in `AI/` and get
  the user's answers to its open questions.
- `AI/APIkey.txt` is a secret: never read it, print it or commit it.
- Reference for the art and the human behaviour this grew from: the Godot
  project `/var/home/aki/Toyfactory/pocket-rave`.

## Code style

Match what's there:
- `///` docs in plain, full sentences, telling what the human does and
  why, in the game's own words ("Thirsty, it walks to the nearest water
  it knows").
- Named constants with a `///` line each, at the top of the file that
  uses them.
- Members end in `_`; functions are camelCase verbs.
- About 120 columns, 4-space indent, braces on the same line.

## Working alongside other sessions

Several agent sessions often work on this project at once, and sessions
sharing one folder have overwritten each other's work before. So:

- **Work in your own git worktree when other sessions may be running.**
  The app can start sessions in a worktree. By hand:
  ```bash
  git worktree add -b my-feature "../The Mall-worktrees/my-feature"
  ```
  Build and `make check` there as usual. When the work is done, the user
  merges it into `main`. Then:
  ```bash
  git worktree remove "../The Mall-worktrees/my-feature"
  ```
  Each worktree has its own `build/`, so its first build takes a while.
- In a shared folder: run `git status` first, and edit with targeted
  changes. Never copy a whole file back over another session's edits.
- Commit each finished step, with `make check` (or `make golden`) done.
  The repo is on `main`, with no remote.
- A pure refactor of Human touches nearly every human file, so only do
  one when nothing else is editing them.
