# Plans

Each feature planned before it was built has a plan here, with the user's
decisions, what was built, and what is still open. Read a feature's plan
before changing it. When you build a step, note it under that step
("### Built (date)"), and update the status below.

Status as of 2026-10-30.

| Plan | What it covers | Status | Still open |
|---|---|---|---|
| [worldgeneration.txt](worldgeneration.txt) | The original world spec: endless chunks, border walls, shared exit doors, keys per door | Built | — |
| [worldgen_variation_plan.md](worldgen_variation_plan.md) | Giant household statues, richer rooms, plazas, mall wings, districts | Steps 1–4 built | Steps 5 and 6 (mall wing, bigger chunks, districts) moved to chunk_shapes_plan.md; statues in mazes and water mazes |
| [chunk_shapes_plan.md](chunk_shapes_plan.md) | Odd chunk footprints (L, T, S), bigger rectangles, outlines (ring, cross, band, islands), mall wing, food court, parking, atrium | Steps 1, 2, 12 built (footprints; 3x1, 4x1, 3x3; no mazes on 3+ slots; big plazas) | Steps 3–11 |
| [blood_plan.md](blood_plan.md) | Wounds and bleeding, stains on world and bodies, washing, suspicion, escort, trails | All 7 steps built | Q2: should some spike victims survive? Q3: should washing or an escort slow bleeding? |
| [blame_plan.md](blame_plan.md) | Witnesses tie a bloody human to a fresh body: blame, run or avenge, shout it, hearers suspect | Built | Blame as its own chat episode; clearing a blamed name |
| [cannibalism_plan.md](cannibalism_plan.md) | The erratic mind, flesh on bodies, scavenging, hunting, guilt and appetite | All 8 steps built | Erratic will be expanded later (user); snapping may need tuning if it should be rarer |
| [talking_plan.md](talking_plan.md) | Lines in data/lines.txt, moments, tone effects, bubbles, pacing | Built | Muttering alone; accusations spreading to overhearers; lies about people |
| [escape_plan.md](escape_plan.md) | Waking, wanting out, hunches, hope and despair, settling, theories of the way out | Steps 1–4 built | Step 5 misleading clues; step 6 getting out is off: no way out for now (user). The open questions at the end are answered under "User's decisions" |
| [chitchat_plan.md](chitchat_plan.md) | Casual talk from context: episodes, chat urge, topics, lines, replies | Steps 1–5 built | Step 6 effects (bonding trust, dread from stories, place remarks teach); step 7 probe; muttering alone (Q4). The other questions are answered under "User's decisions" |
| [relief_plan.md](relief_plan.md) | Bladder and bowels, private spots, mess, dirty feet, foul water, toilets, being seen | All 6 steps built | — |
| [thirst_plan.md](thirst_plan.md) | Thirst, basins, bloody and foul water, guarded water, water tips, hope near water | All 5 steps built | Tuning from play |
| [bottle_plan.md](bottle_plan.md) | Water bottles: everyone wakes with one; sipping, filling, pouring out, dirty water carried over, puddles, looted, snatched, given, sold, lied about | Built | Three calls for the user to check (two bottles at most; giving pours water; the lie). Thirst no longer kills in the soak |
| [hoarding_plan.md](hoarding_plan.md) | Stashes of cakes and bottles on the floor: making, using, stealing, resentment, tips and lies, groups guarding in turns | Planned | — |
| [sleep_plan.md](sleep_plan.md) | Tiredness, sleeping, where to sleep, robbing sleepers, keeping watch, dreams | All 5 steps built | Tuning from play (aim: ~15–20 min awake, ~90 s asleep) |
| [lies_plan.md](lies_plan.md) | Lies out of spite (bait that re-arms, rigged cakes, false tips, slander) and for gain (keeping a cake, selling made-up traps); fixes to greeting so news gets told | Steps 1, 2a, 2c, 3, 4 and gain built | Still rare in normal play (foes come from fights, so liar and foe are seldom calm together); 2b set aside; questions 1, 3–5 unanswered |
| [android_plan.md](android_plan.md) | A debug APK: `make apk` with the NDK, data files as assets, obituary in app storage, touch and the back button | `make apk` and steps 1–3 built | Step 4: try it on the S25 Ultra; the navigation bar still shows |
| [pc_layout_plan.md](pc_layout_plan.md) | A wide 1014x468 canvas for PC windows: stats, log, square world, HUD over map, all always open | Built | Q1: scale on 1080p (smooth below 2x for now) |
| [shut_in_plan.md](shut_in_plan.md) | Remembering a cry from a slam room: going later with a key, fetching one, calling back, telling others | Built | — |
| [feral_plan.md](feral_plan.md) | The feral human's look: a flag that repaints every frame ragged (dirty, torn clothes, its own hair messed up) on the same skeleton | Art built; never set in play | When it turns feral and what changes (not asked yet); questions 1–2 on the look |
| [human_split_plan.md](human_split_plan.md) | Splitting Human into per-system classes | Not started | Advised against for now; if done, only system by system, when that system is being reworked anyway |

## Known gaps found by the tests

- **Lies are rare in normal play**: they work when the chance comes up,
  but foes are made by fights, so a liar and its foe are seldom calm
  together. See [lies_plan.md](lies_plan.md).

## Built without a plan file

These were built straight from the user's requests. Their decisions are in
the code's comments (and in project memory):

- Hunger, cakes, fighting for them, sharing: `src/human/human_fight.cpp`
- Traps (spikes, slam rooms, dart cakes): `src/world/gen_traps.cpp`, `human_fight.cpp`, `human_rescue.cpp`
- Tables, water, toilets and restrooms: `src/world/gen_*.cpp`
- Traits and scored wants: `src/human/traits.h`, `choose()` in `src/human/human.cpp`, the WANTING table in `human_tuning.h`
- Trust, tells, lies, names, gossip: `src/human/human_social.cpp`
- Dread and fright: `scare()`/`fright()` in `src/human/human.cpp`
- Groups: `src/human/human_company.cpp`
- Looting and kills: `src/human/human_fight.cpp`
- Self-lessons: `learn()` in `src/human/human.cpp`
- Life log, HUD, stats menu: `src/human/life_log.h`, `src/scenes/world_scene.cpp`
- Menu and obituary: `src/scenes/`, `src/human/obituary.cpp`

## Writing a new plan

Follow the shape of the newer plans (thirst, relief, sleep):

1. What the feature should make matter, and what it ties into.
2. "User's decisions", with the date.
3. Numbered steps, each small enough to build and check on its own.
4. "Build order" and "Open questions".
5. As steps are built: "### Built (date)" under each, with what changed from the plan and why.

Add a row to the table above.
