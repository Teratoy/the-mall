# Sound and music

The Mall is silent. Sound should make the player **feel where they are
and what is near before they see it**: footsteps on the far side of a
wall, a slam gate banging shut two rooms away, the hum of a server room,
flies over something that has begun to rot, a cry for help through the
walls. And it should carry the followed human's mood: the waking fog,
dread, hope and despair, sleep. Music, if any, is the mall's own and the
human's mood, never a soundtrack for action.

It ties into what is already there:
- Speech bubbles and `talk::Moment` (voices, urgent cries cut in).
- Traps, doors, keys, cakes, bottles, water, toilets, vending machines,
  server rooms, smell and flies, blood, sleep, running and breath.
- The followed human's dread, hope and despair, waking daze.
- Speed X1–X16, pause, the phone and the wide PC layout, Android.

Pocket Rave (the reference) plays DJ sets from audio files, with one
volume and a **low-bit** switch that crushes the music to 4 bits at
5 kHz so it sounds like an old console (`scripts/sound_settings.gd`).

## Ground rules (from CLAUDE.md, not up for debate)

- **Sound is view only.** The sim stays deterministic and headless: no
  sound decides anything, and `make check` must pass unchanged when sound
  goes in. The sim may *append* to a list of things that made a noise;
  only the view reads it.
- **Humans hear nothing new.** What humans hear is already sim (cries
  for help through walls, shouted blame). If the mall should make a sound
  humans react to (muzak they remark on, say), that is a sim feature with
  its own plan, and lines in `data/lines.txt` (see waking_talk_plan.md's
  open question on "the mall's sound").
- Art drawn from data goes in `data/`, reloaded by F5, with a sheet tool
  in `make`: sounds should follow the same pattern.

## User's decisions (2026-11-01)

- **Made by synth, from data:** every sound is a few synth parameters in
  `data/sounds.txt`, reloaded by F5, rendered by `make soundsheet`. No
  sample files.
- **Music later:** no music in this plan. Step 7 keeps only the mood
  of the sounds themselves; the music gets its own plan when the user
  makes it.
- **The followed human's ears:** the player hears what it could hear,
  muffled through walls, next to nothing while it sleeps deeply. Dragging
  the map doesn't move the ears.
- **Voices are a soft pop:** a small sound as a bubble opens, nothing
  per letter. Cries (urgent moments) still get their own, louder sound.
- **Crushed low-bit:** the whole game sounds like Pocket Rave's low-bit
  (few bits, low sample rate, old console), built into the synth rather
  than a switch.
- **Speed thins sound:** the faster it runs, the fewer and quieter the
  sounds; cries, slams and deaths always play.
- **Wet and real violence:** crunches, squelches, drips, unpleasant on
  purpose, within the low-bit sound.
- **Footsteps differ** by bare feet or shoes, and by wet or bloody
  feet (2026-11-01).
- **A CONFIGURATION menu** on the main menu holds the sound settings,
  and the pause menu opens the same menu (2026-11-01).

## 1. Audio core

- `src/audio/`: `InitAudioDevice` in `main.cpp`, an `Audio` object owned
  by the app, closed on exit. raudio is already in the Android build
  (`android/apk.mk`).
- Volumes: master, sounds, music (each 0–1, heard as the square, as in
  Pocket Rave), saved in a small settings file beside the obituary, not
  tracked by git.
- A **CONFIGURATION** scene (user's name for it), opened from a button
  on the main menu under OBITUARY and from the pause menu, with BACK to
  where it came from. It holds the volume steps for now, laid out from
  measured text like the rest of the UI; later settings go there too.
  Pausing quiets the world's sounds.
- Android: stop all sound when the app goes to the background.

### Built (2026-11-01)

- `src/audio/audio.{h,cpp}`: opened in `main.cpp`; plays each sound clear
  or muffled with its own six voices (raylib sound aliases), volume, pan
  and pitch. Where there is no device (the tools), nothing plays.
- MASTER and SOUNDS only, 0–10, saved in `data/settings.txt` (ignored by
  git). MUSIC came with the music (music_plan.md).
- `ConfigPanel` (`src/scenes/config_panel.cpp`) is the CONFIGURATION
  menu: `ConfigScene` on the main menu under OBITUARY, and over the
  pause menu, between RESUME and MAIN MENU. A change plays a pop.
- Pausing simply stops new sounds; nothing loops yet, so nothing needs
  quieting. Not done: stopping sound when the Android app goes to the
  background (untried on the phone).

## 2. Noises from the sim

- `World` keeps a per-tick list of `Noise {kind, cell, who, loud}`,
  appended where things happen (a step, a door, a hit, a trap, a sip, a
  flush, a cry). Cleared each frame by the view. Nothing in the sim
  reads it, so the soak is unchanged; a check in `make check` confirms
  it.
- Speech already reaches the view through bubbles; voices read those.

### Built (2026-11-01)

- `World::noise()` keeps the newest 256 in `World::noises()` with a count
  of all ever made; `Hearing` plays those it hasn't yet. Made at: a key
  unlocking (door, gate, airlock), an airlock pushed open and falling
  shut, a slam gate locking, spikes, darts fired and striking, a blow
  landing (`takeHit`), a death (`die`). `make check` passes unchanged.
- Footsteps and pops don't need noises: the view hears a foot come down
  in the walk or run art (`footfalls()`: walk frames 1 and 3, run 0 and
  2, so the step keeps time with the drawing; at first it was one per
  cell entered, which the user heard out of step), and a new line over
  a head.

## 3. The sounds themselves

Made by synth (user's decision), in the spirit of the
art drawn in code:
- `data/sounds.txt`: each sound as a few lines of synth parameters
  (wave, pitch slide, envelope, noise, crush), like sfxr. F5 reloads it.
- `make soundsheet` renders every sound to `build/sounds/*.wav` to
  listen to, and `cmp` before and after a refactor.
- Small random pitch and volume wobble each play, from a view-only Rng,
  so 30 footsteps don't sound like a machine gun.

### Built (2026-11-01)

- `src/audio/synth.{h,cpp}`: square, triangle, saw, sine and noise layers
  sliding in pitch under an envelope, with low and high pass, repeats and
  wobble; then the crush, 5 bits at 8 kHz by default. The file's header
  explains the format.
- `make soundsheet` (`tools/sound_sheet.cpp`) writes each sound clear and
  muffled to `build/sounds/` and prints its length.
- `data/sounds.txt` ships in the APK beside the other data files.

## 4. First sounds

What the player most needs to hear, by group:
- **Bodies:** footsteps (floor kind, walking or running, shoes or bare
  feet, dry, wet or bloody feet), breath when winded, eating, sipping, snoring, retching.
- **Things:** door unlock and open, key picked up, cake lifted, bottle
  filled and poured, toilet flush, vending machine paying and dropping,
  slam gate shutting, spikes, darts.
- **Violence:** punch, hit landing, fall, death, bleeding drips?
- **Places (loops):** water lapping, server room hum, flies near smell,
  the room tone of each kind of place (rooms, plazas, mazes).

### Built (2026-11-01): the first sounds

`step`, `run`, `step_wet` (fresh piss or poured water), `step_blood` (wet
blood), `step_filth` (fresh feces), `pop`, `cry`, `unlock`,
`airlock_open`, `airlock_shut`, `slam`, `spikes`, `dart_fire`,
`dart_hit`, `hit`, `death`. Still to come: breath, eating, sipping,
snoring, retching, keys and cakes picked up, bottles, flushes, vending,
and the place loops.

Later, `knob`: a shut door's handle rattling when a human tries it
(`tryDoor`).

### Survey (2026-11-01): what still makes no sound

An agent went through the code for everything the player can see happen
without a sound. Not built; the user chose to plan them first. **Sim**
needs a new `NoiseKind` and a `world.noise()` call where it happens;
**view** is for `hearing.cpp` to tell from state frame to frame, as
footsteps and pops are. New to this plan unless marked (planned).

The ten that matter most, in order:
1. Blood spraying from a blow, louder the more blood it throws
   (`spill`, human_blood.cpp). Sim, or a wet layer folded into `hit`.
2. Bleeding drips, and a body pooling (human_blood.cpp drip loop). Sim
   per drop, or view from `bleeding()` (planned, with a question mark).
3. Feeding on a body: each bite and its blood (human_flesh.cpp). Sim.
   The wettest thing in the game.
4. A punch swung: a whoosh as `punchLanded` turns true (human_fight.cpp
   `landPunch`); `hit` plays over it when it lands, so a lone whoosh is a
   miss. View (planned as "punch").
5. Pissing and shitting, a splash or a plop by where it lands (toilet,
   floor, water), and wetting or soiling itself (human_relief.cpp
   `startRelieving`, `accident`, `letGo`). Sim.
6. Flies over rotting bodies and fresh filth (`smellStrength`, and the
   freshness `stepSound` already uses). View loop (planned).
7. Gulping from water, a basin or a toilet (human_thirst.cpp, a timer
   per gulp: sim) and sipping from a bottle (view) (planned).
8. Eating a cake (`startEating`, human_fight.cpp). View (planned).
9. Panting while winded (planned), snoring (planned), a thud on
   collapsing from exhaustion (`startSleeping` collapsed), a gasp on a
   rough waking (`wakeUp`). View, the gasp perhaps sim.
10. The server room hum (the sim's own `humShare`, which humans already
    hear) and water lapping at pools and basins: view loops (planned);
    then the vending drop and pickups below.

The rest:
- **Bodies:** sick from rotten flesh or toilet water (human_flesh.cpp,
  human_thirst.cpp): retching is planned, but nobody retches in the game
  yet, so it waits for that. Washing (human_blood.cpp). Rummaging while
  looting, stashing or robbing a sleeper (the Search pose: view).
- **Things:** a key or cake picked up (human_walk.cpp, and piles in
  world.cpp; planned), a bottle put down (`World::putDown`); a vending
  machine dropping goods and an empty one clunking (`World::vend`: sim,
  planned as "paying and dropping"); a dispenser's glug as a bottle is
  filled from it or poured into it (human_dispenser.cpp); typing at a
  terminal (view) and its refusal beep (human_computer.cpp, sim).
- **The toilet flush in step 4 is wrong:** the game has no flush on
  purpose ("FOUND NO FLUSH"). The plop or splash in 5 above takes its
  place.
- **Places:** a buzz near a flickering light (world/lighting.cpp); the
  room tone of each chunk kind (Rooms, Plaza, Maze, WaterMaze,
  DataCentre; planned), a dark dead wing near silent.
- **Guns:** the Shoot, Reload and Rack poses are drawn but `poseOf`
  never returns them, so there is nothing to hear until humans use guns.
- **UI:** nothing clicks but the CONFIGURATION panel's pop. One hook in
  `ui::button` and `ui::arrowButton` (pixel_ui.cpp) covers every button:
  the menus, HUD (LOG, MAP, NEXT, zoom, speed, pause), the pause menu,
  the obituary. Worth sounds of their own: pause, WAKE UP, NEXT after
  your human dies, CREMATE.
- Left out: greeting and giving (their speech already pops); machines
  restocking (unseen).

## 5. Where sound is heard from

- Volume falls with distance from the followed human's ears,
  out to about one chunk.
- Walls between muffle (a low-pass and quieter), so a slam two rooms
  away is a thud.
- Left–right pan from the screen position, gentle on the narrow phone.
- At most N voices at once; the quietest and oldest give way; urgent
  ones (cries, slams, deaths) always play.
- At high speed sounds thin out so X16 isn't a roar; cries, slams and deaths always play.

### Built (2026-11-01)

`src/audio/hearing.cpp`, from the followed human's cell: each sound
fades out at its own reach (steps 9 cells, a slam 30), each wall on the
line between keeps 45% of it, muffled, and past 3 walls nothing is
heard. Asleep it hears 8% (urgent ones 30%). Pan up to 0.3 either side
over 12 cells. At speed xN, steps play one in N and anything not urgent
is quieter by the square root of N. At most 6 sounds start a frame,
loudest first, besides the urgent ones (cries, slams, spikes, deaths).

### Denoiser (2026-11-01, user's ask)

`src/audio/denoise.cpp` sits on raylib's final mix
(`AttachAudioMixedProcessor`), so every sound, voice and the music pass
through it last. A fourth-order low-pass at 3800 Hz smooths the hiss of
the crush's stair-steps (just under half its 8000 Hz) and keeps the grit;
a gentle gate fades the mix out under 0.004, where only the last flicker
of crushed tails is left (opens in 2 ms, closes over 120 ms). It assumes
the device mixes at 48000 Hz in stereo, as it does on this machine
(PulseAudio) and on Android. A sound given a crush rate above 8000 in
`data/sounds.txt` loses what it gains above 3800 Hz: raise `CUT_HZ` if
that is wanted.

## 6. Voices

- A soft pop as each bubble opens (user's decision), pitched a little
  per human from its seed so a crowd isn't one note.
- Urgent moments (`talk::isUrgent`) get their own louder cry instead,
  cutting in like their bubbles.

Voiced since 2026-11-01: see [voice_plan.md](voice_plan.md). At X1
every line in earshot is said in the speaker's own voice, replacing the
pop and the cry; faster, the pop and cry below still play.

### Built early (2026-11-01)

A pop as a bubble the player can see opens, pitched by the speaker's id.
A line in the Fear tone is a `cry` instead, heard bubble or not, through
walls. The violent urgent moments (hit, darts, spikes) already sound
through their noises.

## 7. Mood

No music yet (user: made later, in its own plan). The sounds follow the
followed human instead: the waking fog (muffled, far), dread (thinner,
louder heartbeat-near sounds), settling (calmer), sleep (only the room
tone and breath), and death (silence).

## 8. Later

- `make sounds`: a sound editor page beside the lines and sprite
  editors, playing each sound as it is changed. Built: see
  [sound_editor_plan.md](sound_editor_plan.md).
- Humans hearing the mall (a sim feature, see the ground rules).

## Build order

1, 2, 3 together with a handful of sounds from 4 (footsteps, door, hit,
slam), then 5, then the rest of 4, then 6, then 7. 8 when wanted.

## Open questions

All answered on 2026-11-01 (see "User's decisions"). One thing to settle
when footsteps are built: every human wears shoes today
(`human_look.cpp` always draws them), so bare-feet steps only play once
the look can roll bare feet, which is an art change of its own. Until
then, shoes it is.
