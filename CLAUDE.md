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
toolbox run -c claude-build make check        # soak vs golden output (~75 s)
```

| Command (inside the toolbox) | What it does |
|---|---|
| `make` | Builds `build/themall`. Zero warnings is the norm: keep it so |
| `./play.sh` (from the host) | Rebuilds if needed and opens the game window |
| `make check` | Runs the headless soak (`build/stuck_soak 6 15 1 6`, one process per seed) with the scenarios beside it, diffing the soak with `tests/golden/soak.txt`. The soak's output is kept in `build/soak.txt` and only rerun when the soak, the data files or the Makefile change |
| `make scenarios` | The scenarios alone (3 s): set-up situations that check rare features still happen. `build/scenarios NAME` runs one and prints every log line |
| `make golden` | Rewrites the golden output after a change meant to alter behaviour. Commit it with that change. Straight after a failed `make check` it just copies `build/soak.txt`, so it takes no time |
| `make coverage` | Lists log lines (`note(...)`) and talk moments that neither the soak nor the scenarios reached |
| `make stuck` | The soak with its own arguments: `build/stuck_soak [seeds] [minutes] [verbose] [crowd]`. `TRACE=seed:id:from:to` follows one human |
| `make bottlesheet` | Draws every pose with a bottle at several levels and dirt, scaled up, to `build/bottle-sheet.png` |
| `make dirtysheet` | Draws one human beside itself dirty in every pose, with cake, bottle, blood and remains, to `build/dirty-sheet.png`. `DIRTYSHEET_ARGS="42 4 gun"` draws a gun tucked into each kind of bottom instead, and `"42 4 shoot"` a human shooting all eight ways |
| `make shootanim`, then `python3 tools/frames_gif.py build/shoot-anim build/shoot.gif` on the host | One human shooting all eight ways, its spent casings piling up on the floor, beside ammo boxes, as frames in `build/shoot-anim/`, joined into a GIF (the toolbox has no PIL). See [AI/gun_art_plan.md](AI/gun_art_plan.md) |
| `make genprobe` | Grows chunk interiors on every footprint (L, T, S, boxes) and checks they keep to it. `GENPROBE_ARGS="20 png"` also saves pictures to `build/genprobe-png/` |
| `make apk` (on the host, not the toolbox) | Builds `build/TheMall-debug.apk` for the phone with the Android NDK. See `android/apk.mk` and [AI/android_plan.md](AI/android_plan.md) |
| `make lines` (host or toolbox), or `./lines.sh` (the "The Mall Lines" desktop shortcut) | The lines editor: a local web page at http://localhost:8765 for rewriting `data/lines.txt`, with the game's bubble drawn beside each line. See [AI/lines_editor_plan.md](AI/lines_editor_plan.md) |
| `make sprites` (host or toolbox), or `./sprites.sh` | The sprite editor: a local web page at http://localhost:8766 for redrawing `data/sprites.txt`. See [AI/sprite_editor_plan.md](AI/sprite_editor_plan.md) |
| `make spritesheet` | Draws every sprite from `data/sprites.txt` in all the ways the game does, at 1x, to `build/sprite-sheet.png`: `cmp` it before and after an art refactor. `make spriteref` remakes the sprite editor's human and machine slots |
| `make simprofile` / `make gprof` | Times the sim alone on one soak seed (`SIMPROFILE_ARGS="seed minutes crowd"`); `gprof` writes a profile to `build/gprof.txt`. See [AI/sim_speed_plan.md](AI/sim_speed_plan.md) |
| `make trips` | How walks for needs go in the soak's worlds: finished or dropped on the way, and for what; how far water was when it got thirsty; what the thirsty who know no water do. `TRIPS_TRACE=seed:id` follows one human (pass it with `toolbox run -c claude-build env TRIPS_TRACE=...`, since `toolbox run` drops the environment). See [AI/needs_planning_plan.md](AI/needs_planning_plan.md) |
| `make shot SHOT_ARGS="..."` | Saves `build/shot.png` without a visible window. See `tools/shot.cpp` for the options (`--scene`, `--seconds`, `--seed`, `--zoom`, `--stats`, `--log`, `--reveal`, `--wide`) |

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
  `deaths_`, `mind_`, `rescue_`, `escape_`, `gore_`, `escorting_`, `errand_`,
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
  - blood, thirst, bottle, relief, grime, smell, sleep, flesh, hope, rescue, company, chat, topic, episode
  - errand: keeping to a walk for a need, topping up, turning back, looking for water or food, the other need while there
- `src/world/`: chunks and generation (`gen_*.cpp`), blood, mess, traps,
  tables, statues, toilets.
- `src/render/`: pixel art drawn in code; `src/scenes/`: menu, create,
  world (HUD, log, stats), obituary; `src/ui/`: the pixel UI.
- `data/lines.txt`: everything humans say. The user edits it, and F5
  reloads it in game. Its header explains the format.
- `data/names.txt`: what names are grown from.
- `data/sprites.txt`: the glyph sprites (items, traps, toilet, vending
  machines, map symbols, icon, the sleeper's Z), read by `render/sprites.cpp`;
  F5 reloads it. The sprite editor writes it. Art drawn from letters goes
  here, not in a glyph array in code; humans and other procedural art stay
  in code.
- `data/lines_review.json` and `data/lines_drafts.json`: the lines editor's
  review marks, and rewrites Claude drafts for the user to accept or reject
  there (format at the top of `tools/lines_editor.py`). Draft rewrites go in
  the drafts file, not straight into `data/lines.txt`.
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
- Voice lines never state the obvious: no literal account of what just
  happened ("PARTED FROM HUT, THEY WON'T TRUST ME NOW"). The player saw it;
  the line gives the feeling ("THEY WON'T TRUST ME NOW"). The life log may
  say it plainly.
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
