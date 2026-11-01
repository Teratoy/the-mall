# Sound editor

A local web page for tweaking the synth sounds in `data/sounds.txt`, shaped
like the lines and sprite editors (`AI/lines_editor_plan.md`,
`AI/sprite_editor_plan.md`): stdlib Python in `tools/`, a page in
`tools/sound_editor/`, a launcher `sounds.sh`, `make sounds`. Step 8 of
`AI/sound_plan.md`.

## What the user asked (2026-11-01)

A sound editor for tweaking the synth of each sound: simple editing, and
easy access to all the sounds.

## Proposal

- **All sounds in one list**, down the left, grouped by the file's
  sections (Feet, Voices, Doors, Traps, Violence), each with a play
  button. Clicking a name opens it; space plays it again.
- **One sound open at a time**: its gain, bits and rate at the top, then a
  card per layer with its wave, from/to Hz and every key (gain, delay,
  attack, hold, decay, lowpass, highpass, duty, repeat, gap, fade, wobble)
  as a slider with a number beside it. Letting go of a slider plays the
  sound. A layer can be added, copied, muted while listening, or removed.
- **Heard exactly as in the game**: the server renders each sound with the
  game's own synth (`src/audio/synth.cpp`, built into a small program
  without raylib that runs on the host) and sends back a .wav. No second
  synth in JavaScript that could sound different.
- **A picture of the wave** under the sound, so a click or a tail cut short
  shows.
- **Saving** writes only that sound's block back into `data/sounds.txt`,
  keeping the comments, refused if the block changed underneath (another
  session or a hand edit), as the other editors do. F5 in the game reloads
  it.

## User's decisions (2026-11-01)

1. Saving: a SAVE button, and REVERT back to what is saved. Edits to
   several sounds are kept on the page until saved; the list marks them.
2. Beside PLAY: THROUGH A WALL (muffled, as one wall keeps it), FAR OFF
   (half its reach) and ASLEEP (as a sleeper hears it).
3. Only tweak the sounds the game plays: no new sounds, no renaming.
4. No VARY button: hand edits only.

### Built (2026-11-01)

- `./sounds.sh` (or `make sounds`) opens http://localhost:8767. sounds.sh
  first builds `build/sound_sheet` in the toolbox; it now links without
  raylib and has a `--one [LOWPASS]` mode (a sound on stdin, a .wav on
  stdout), so `tools/sound_editor.py` runs it on the host for every play.
- The list down the left, by the file's `# ---` sections; ▶ plays one,
  clicking a name opens and plays it, ↑ ↓ step through them, space plays
  the open one, ctrl+s saves.
- The whole sound's gain, bits and rate, then a card per layer: wave, from
  and to Hz (sliders in pitch), every key as a slider with a number;
  duty shows only on a square, gap and fade only when it repeats. Letting
  go of a slider plays it. MUTE and SOLO are for listening only; COPY,
  REMOVE and + LAYER change the sound.
- THROUGH A WALL, FAR OFF and ASLEEP use hearing.cpp's numbers, copied at
  the top of the page (WALL_KEEP and the 450 Hz muffle, a quarter as loud
  at half reach, ASLEEP_KEEP or ASLEEP_URGENT_KEEP for cry, slam, spikes
  and death): change them there too if hearing.cpp's change.
- The wave is drawn under the buttons, with the true peak and how many
  samples are cut off at full loudness, which the server counts from the
  synth's own samples (slam, hit and death clip today).
- Saving writes only the sound's block, as the file spelt it plus what was
  moved: untouched sounds save byte for byte as they were. A block changed
  underneath is refused; the page keeps its edits, and SAVE again puts them
  over it. Edits to several sounds wait on the page, marked ● in the list.
