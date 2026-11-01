# Plans

Each feature planned before it was built has a plan here, with the user's
decisions, what was built, and what is still open. Read a feature's plan
before changing it. When you build a step, note it under that step
("### Built (date)"), and update the status below.

Status as of 2026-10-31.

| Plan | What it covers | Status | Still open |
|---|---|---|---|
| [worldgeneration.txt](worldgeneration.txt) | The original world spec: endless chunks, border walls, shared exit doors, keys per door | Built | — |
| [worldgen_variation_plan.md](worldgen_variation_plan.md) | Giant household statues, richer rooms, plazas, mall wings, districts | Steps 1–4 built | Steps 5 and 6 (mall wing, bigger chunks, districts) moved to chunk_shapes_plan.md; statues in mazes and water mazes |
| [chunk_shapes_plan.md](chunk_shapes_plan.md) | Odd chunk footprints (L, T, S), bigger rectangles, outlines (ring, cross, band, islands), mall wing, food court, parking, atrium | Steps 1, 2, 9, 12 built (footprints; 3x1, 4x1, 3x3; no mazes on 3+ slots; `make genprobe`; big plazas) | Steps 3–8, 10, 11 (odd footprints are probed but not yet in `SHAPES`) |
| [blood_plan.md](blood_plan.md) | Wounds and bleeding, stains on world and bodies, washing, suspicion, escort, trails | All 7 steps built | Q2: should some spike victims survive? Q3: should washing or an escort slow bleeding? |
| [blame_plan.md](blame_plan.md) | Witnesses tie a bloody human to a fresh body: blame, run or avenge, shout it, hearers suspect | Built | Blame as its own chat episode; clearing a blamed name |
| [cannibalism_plan.md](cannibalism_plan.md) | The erratic mind, flesh on bodies, scavenging, hunting, guilt and appetite | All 8 steps built | Erratic will be expanded later (user); snapping may need tuning if it should be rarer |
| [talking_plan.md](talking_plan.md) | Lines in data/lines.txt, moments, tone effects, bubbles, pacing, readable bubbles (min time, urgent cut-in) | Built | Muttering alone; accusations spreading to overhearers; lies about people |
| [escape_plan.md](escape_plan.md) | Waking, wanting out, hunches, hope and despair, settling, theories of the way out | Steps 1–4 built | Step 5 misleading clues; step 6 getting out is off: no way out for now (user). The open questions at the end are answered under "User's decisions" |
| [chitchat_plan.md](chitchat_plan.md) | Casual talk from context: episodes, chat urge, topics, lines, replies | Steps 1–5 built | Step 6 effects (bonding trust, dread from stories, place remarks teach); step 7 probe; muttering alone (Q4). The other questions are answered under "User's decisions" |
| [relief_plan.md](relief_plan.md) | Bladder and bowels, private spots, mess, dirty feet, foul water, toilets, being seen | All 6 steps built | — |
| [thirst_plan.md](thirst_plan.md) | Thirst, basins, bloody and foul water, guarded water, water tips, hope near water | All 5 steps built | Tuning from play |
| [parched_lost_plan.md](parched_lost_plan.md) | Parched with no water known: no asking the same one twice, lying by a locked door calling for help, leaving one as lost, erratic going for bottles | All 4 steps built | Tuning from play |
| [bottle_plan.md](bottle_plan.md) | Water bottles: one lies half full where each human wakes, left until needed, gone or stolen; water is scarce, kept once seen; carried on top of its cakes; sipping, filling, pouring out, dirty water carried over, puddles, looted, snatched, given, sold, lied about | Built; then one bottle at most, carried on the cakes, poured bottle to bottle | Calls for the user to check: giving pours water; the lie. Thirst kills more since bottles are found (17 deaths in 6 worlds, was 6) |
| [hoarding_plan.md](hoarding_plan.md) | Stashes of cakes and bottles on the floor: making, using, stealing, resentment, tips and lies, groups guarding in turns | Steps 1–5 built | Step 6 (groups guard in turns). Still rare in normal play: nobody learns whose a stash is in the soak, so steps 3–5 happen only in scenarios |
| [foul_water_memory_plan.md](foul_water_memory_plan.md) | Remembering foul water: choosing other water, knowing lines, cravers mind only filth, warning those it walks with | Built | Warning outside the group; lying about foul water |
| [sleep_plan.md](sleep_plan.md) | Tiredness, sleeping, where to sleep, robbing sleepers, keeping watch, dreams | All 5 steps built | Tuning from play (aim: ~15–20 min awake, ~90 s asleep) |
| [lies_plan.md](lies_plan.md) | Lies out of spite (bait that re-arms, rigged cakes, false tips, slander) and for gain (keeping a cake, selling made-up traps); fixes to greeting so news gets told | Steps 1, 2a, 2c, 3, 4 and gain built | Still rare in normal play (foes come from fights, so liar and foe are seldom calm together); 2b set aside; questions 1, 3–5 unanswered |
| [wronged_company_plan.md](wronged_company_plan.md) | Leaving the one it hit or robbed: guilt that holds trust down, slow to fade; the cunning stay if never found out | Built | Guilt beyond the parting line |
| [android_plan.md](android_plan.md) | A debug APK: `make apk` with the NDK, data files as assets, obituary in app storage, touch and the back button | `make apk` and steps 1–3 built | Step 4: try it on the S25 Ultra; the navigation bar still shows |
| [pc_layout_plan.md](pc_layout_plan.md) | A wide 1014x468 canvas for PC windows: stats, log, square world, HUD over map, all always open | Built | Q1: scale on 1080p (smooth below 2x for now) |
| [shut_in_plan.md](shut_in_plan.md) | Remembering a cry from a slam room: going later with a key, fetching one, calling back, telling others | Built | — |
| [lessons_plan.md](lessons_plan.md) | Self-lessons announced once, again, or for good; a slower fade; a line for surviving thirst | Built | — |
| [trap_wary_plan.md](trap_wary_plan.md) | Wary of the next trap like one that caught it: table cakes after darts, one-door rooms after a slam | Built | For later: should seeing a slam shut on someone else make it wary too? |
| [lines_editor_plan.md](lines_editor_plan.md) | `make lines`: a local web page for rewriting data/lines.txt, with bubble previews, review marks and Claude's drafts | Built | — |
| [dirty_plan.md](dirty_plan.md) | Going unwashed: grime, turning dirty (a ragged look on the same skeleton), washing for it by modesty, dried blood only washed by the modest; smell from the dirty, bodies, feces and foul water, sniffed through walls, flies | Built | Hunched/claws on the look; tuning from play |
| [waking_talk_plan.md](waking_talk_plan.md) | More muttering in the first minutes: stirring, standing, reaching for memory, the room through the fog, the quiet, calling out, it goes on | Built | Dread from the quiet; longer waking; the mall's sound; the timid steering away from a voice |
| [vending_plan.md](vending_plan.md) | Cake and empty-bottle vending machines: paid in 3 keys, restock one a day, in rooms and plazas against north or side walls, remembered and told of like cakes | Built | Tuning from play; gun machines wait for guns |
| [gun_art_plan.md](gun_art_plan.md) | Gun sprites: a gun tucked in the waistband, shooting eight ways with recoil, the held gun, ammo boxes, spent casings thrown out and piling up, reloading round by round | Art built; no gameplay | Aiming north into the screen; reload speed by trait; all gun gameplay |
| [machine_room_art_plan.md](machine_room_art_plan.md) | Old computers as art: server rack, CRT terminal, console desk, cable floor decals | Art built | Where they stand in the world; any use |
| [server_rooms_plan.md](server_rooms_plan.md) | Server rooms in the world: grown in room networks and a data-centre chunk, two locked doors in a row (2 keys in, free out, relocking), racks block sight, first sight remembered, a good place to sleep | Built | Defaults to confirm (a key per room; slipping through behind someone); telling of them |
| [server_room_keys_plan.md](server_room_keys_plan.md) | Learning the server room airlock: the second time through, humans learn the double doors eat keys and keep theirs (unless curious), tell others at greetings, and drowsy inside a server room sleep there rather than pay to come back | Built | None |
| [computers_plan.md](computers_plan.md) | Trying the computers: curious humans walk up to the first terminal and the first console desk they see and type at it, finding a password; later ones now and then, never the same twice, less and less; a typing pose and a password prompt on screen | Built | Telling others; anything ever unlocked |
| [running_plan.md](running_plan.md) | Running beyond fights: urgent needs, fear, chasing, racing for cake and water; breath and being winded; run and winded poses; traits, wounds and load | Steps 1–3 built (running, breath and winded, run and winded poses, breath in stats) | Steps 4–9: who runs and how fast, needs, fear, chases and dropping, races, scenarios |
| [sprite_editor_plan.md](sprite_editor_plan.md) | `make sprites`: a local web page for redrawing the glyph sprites, moved to data/sprites.txt (F5 reloads), with game-scale, human and machine previews | Built | — |
| [sim_speed_plan.md](sim_speed_plan.md) | Where the sim spends its time; faster search with the same results; `make simprofile`, `make gprof` | Search speedups built (seed 4: 84.7 s to 8.1 s, `make check` unchanged) | A1, A2 (exact); B1, B2 change behaviour: questions 1–3 |
| [sound_plan.md](sound_plan.md) | Sound and music: view-only noises from the sim, sounds from data, hearing through the followed human's ears, pops, mood; music later | Steps 1–3 and 5 built, with the first sounds; pops from step 6 | The rest of step 4 (more sounds, place loops); 7 mood; bare feet need the look first; Android background |
| [needs_planning_plan.md](needs_planning_plan.md) | Looking after needs with some foresight: topping up, a sense of range, looking for water when none is known, sticking to an errand, two errands on one trip; `make trips` measures it | All 5 steps built | Suspect-by-water step loop (older); rescue during errands |
| [repeat_lines_plan.md](repeat_lines_plan.md) | Lines for what keeps happening: a tally per kind (cakes, bodies, water, keys and doors, darts, blood, fights, going, sleep, the quiet, faces, rotten flesh and toilet water), milestones, again/often lines; bodies harden or wear down dread; `tools/repeat_survey.py` counts repeats | Built (2026-11-01): tally, milestones, 12 moments, `again`/`often`, body dread; scenarios `repeat-milestones`, `repeat-harden`, `repeat-grieve` | 217 drafted lines to accept in `make lines`; other effects of repeats (noted for later) |
| [light_plan.md](light_plan.md) | Light and light sources: light limits sight (dark seen only close), ceiling fittings, glowing machines and statues, dark wings, switches and torches; a 20-minute mall day with hour bells and opening/closing bells humans hear; dark fear, sleep, hiding, blind trouble; the time in the HUD | Steps 1–5 built: light map, fittings, drawn as one layer with sight and memory; light limits sight (`DARK_SIGHT` 4); the dark frightens, the bold sleep and the modest hide in it, the fearful make for light, mess goes unseen | Steps 6–7 (mall clock and bells; switches and torches); 3 water-asking scenarios known; the cunning waiting in the dark |
| [worldgen_editor_plan.md](worldgen_editor_plan.md) | `make genview`: a native raylib tool for tuning world generation live; knobs moved to data/worldgen.txt (F5 reloads), kind and footprint picks, locked seed, before/after, a 3x3 seed grid, overlays, readout, a heatmap from dropped humans; Claude can drive it from chat through a file watch | Planned, all questions answered | — |
| [human_split_plan.md](human_split_plan.md) | Splitting Human into per-system classes | Not started | Advised against for now; if done, only system by system, when that system is being reworked anyway |

## Known gaps found by the tests

- **Lies are rare in normal play**: they work when the chance comes up,
  but foes are made by fights, so a liar and its foe are seldom calm
  together. See [lies_plan.md](lies_plan.md).
- **Stashes are rare in normal play**: hoarders stash where nobody
  watches, so nobody learns whose a stash is, and stealing, resentment and
  telling of stashes happen only in the scenarios. See
  [hoarding_plan.md](hoarding_plan.md).

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
