# Music

The Mall has sounds (`AI/sound_plan.md`) but no music. On 2026-11-01 the
user said "we will make the music later, in its own plan": this is that
plan, for the **main menu only** for now. The menu is THE MALL over an
empty mall scrolling slowly south, with ENTER THE MALL, OBITUARY and
CONFIGURATION. Its music should say what the game is before anything
happens: a mall that has been playing to nobody for a very long time, and
you, waking in it with no memory.

It ties into what is already there:
- The synth (`src/audio/synth.{h,cpp}`): layers of square, triangle, saw,
  sine and noise under an envelope, crushed to 5 bits at 8 kHz. The music
  is played through it, so music and sounds are one sound.
- `src/audio/audio.{h,cpp}`: the device, the MASTER and SOUNDS volumes,
  `data/settings.txt`, F5 reloading `data/sounds.txt`.
- CONFIGURATION (`src/scenes/config_panel.cpp`), shown from the main menu
  and the pause menu.
- The scenes: `MenuScene`, `ObituaryScene`, `ConfigScene`, and
  `WorldScene` while choosing the sleeper (REFRESH, WAKE UP).
- The sound editor (`./sounds.sh`, `AI/sound_editor_plan.md`).
- The APK (`android/apk.mk`), which ships the data files as assets.

## Ground rules

- **Music is view only**, like sound: nothing in the sim hears it, and
  `make check` passes unchanged. If the mall's muzak should one day be
  something humans hear and remark on, that is a sim feature with its own
  plan (see sound_plan.md's ground rules).
- **From data**, like the sounds, sprites and lines: `data/music.txt`,
  reloaded by F5, rendered by a sheet tool in `make`.
- The scenes are made anew each time (`MenuScene` is rebuilt on every
  return), so the music lives in `audio`, owned by the app, not in a
  scene. A scene only says what it wants to hear.

## User's decisions (2026-11-01)

1. **A pattern file played through the synth:** `data/music.txt`, like a
   tiny tracker. Instruments are synth layers written as in
   `data/sounds.txt`; sections are notes in rows, a column per channel;
   a tempo. Played through the same synth and crush; F5 reloads it.
2. **Mood: muzak gone wrong.** The mall's own music: soft, cheerful lounge
   chords on a warm square and a sine bass, a little slow and detuned, as
   if it has played to nobody for years.
3. **A loop with variations:** a few written sections; each pass a
   view-only Rng picks which comes next and drops or adds a channel, so it
   never repeats exactly but stays the written tune.
4. **Fades on WAKE UP:** it keeps playing through ENTER THE MALL and
   choosing the sleeper (REFRESH and all), then fades out as the human
   wakes and the camera eases out, leaving the mall's own sounds.
5. **A MUSIC row in CONFIGURATION**, under SOUNDS, 0–10 like the others,
   saved in `data/settings.txt`; default 6, a little under the sounds.
6. **OBITUARY quieter:** the tune carries on unbroken across the main
   menu, CONFIGURATION and OBITUARY, but ducks low in OBITUARY, the list
   of the dead, and comes back up on leaving it.
7. **An editor: a page in the sound editor.** `./sounds.sh` gets a music
   tab: instruments edited like sounds, a pattern grid per section,
   played through the game's synth.
8. **Android: ship it and pause in the background.** `data/music.txt`
   goes in the APK assets beside `sounds.txt`; the music pauses when the
   app goes to the background and resumes on return. The sound plan's
   open background step is done with it.

## 1. The music file and its rendering

`data/music.txt`, its header explaining the format as `sounds.txt` does.
A first sketch, to be settled as it is built:

```
crush 5 8000

# An instrument: layers as in sounds.txt, written at A4 (440 Hz) and
# moved to each note's pitch. hold stretches to the note's length.
instrument epiano gain 0.5
    square 440 duty 0.2 attack 0.004 hold 0 decay 0.6 lowpass 1800
    sine 880 gain 0.15 decay 0.3

instrument bass
    sine 440 attack 0.005 decay 0.4

instrument brush
    noise 6000 gain 0.2 decay 0.05 highpass 3000

# A song: its tempo, rows a beat, how far it drifts out of tune (the old
# tape: a slow wander in pitch, in cents), and its channels.
song menu tempo 84 rows 4 wow 12
    channels epiano epiano epiano bass brush

# A section of the song: one row per line, a note per channel (C-4, F#3),
# --- held, . nothing, == let go.
section menu a
    C-4 E-4 G-4 C-2 x
    ---  ---  ---  ---  .
    ...

# What may follow what, by weight, and how often a channel drops out.
flow menu a b 2, a c 1, b a 1, c a 1
vary menu drop 0.25 keep bass
```

- `src/audio/music_data.{h,cpp}`: the parser and the renderer, without
  raylib, beside `synth.cpp`, so the game, the sheet tool and the editor's
  server all render the same samples. Each section renders once, every
  channel's notes through `synth::render` (uncrushed) and mixed, then the
  mix crushed once, the way an old console's chip would. Channel by
  channel is kept too, so a variation can drop one without rendering
  again.
- `make musicsheet` (`tools/sound_sheet.cpp --music`, or a tool of its
  own) writes each section to `build/music/SONG-SECTION.wav`, and a few
  minutes of the song as the Rng would play it to `build/music/SONG.wav`.

### Built (2026-11-01)

- `src/audio/music_data.{h,cpp}`, no raylib: `parse`, `render` (a section,
  each channel apart, uncrushed, with its tails) and `Tape`, which plays a
  song on and on (flow, vary, wow, then the crush on the mix, a sample at a
  time). The synth gained `synth::layers` (the layers before the crush),
  `synth::Crush` and `synth::readLayer`, split out of `render` and `parse`:
  every sound renders byte for byte as before.
- Changed from the sketch: notes are written with flats only (`Db4`), as
  `#` starts a comment; `.` alone holds (no `---`); a channel can be a few
  cents out of tune of its own (`epiano:7`); `vary` keeps by instrument
  name. The song starts at its first section as written, and only then do
  channels drop out.
- `make musicsheet` (`build/sound_sheet --music`) writes each section alone
  and three minutes of each song as played to `build/music/`;
  `--music-one` renders one song from stdin for the editor.

## 2. The player

- `src/audio/music.{h,cpp}`: `music::want("menu", level)`, called by a
  scene each frame or on entry, and `music::update(dt)` from the main
  loop. Wanting the song already playing changes nothing, so a rebuilt
  `MenuScene` doesn't restart it. Wanting none fades it out.
- Played through a raylib `AudioStream`: sections queued one after
  another without a gap; the next is picked by a view-only Rng from the
  `flow` weights, with `vary` dropping a channel now and then (never the
  kept ones).
- The tape's `wow`: a slow wander in pitch over the whole mix, so it
  sounds worn.
- The level: MASTER × MUSIC (heard as the square, as the other volumes),
  times the scene's level, eased so ducking and fading are smooth.
- A MUSIC row in `ConfigPanel` under SOUNDS, default 6, in
  `audio::Settings` and `data/settings.txt`. Moving it is heard on the
  music itself, which plays in CONFIGURATION.
- F5 reloads `data/music.txt` too; the song carries on from the section
  it was in.

### Built (2026-11-01)

`src/audio/music.{h,cpp}`: `music::want(song, level)` each frame from the
scenes, `music::update(dt)` from the main loop, `music::reload()` on F5 (the
main menu has F5 too, to hear a change there), `music::close()` before the
device. A raylib `AudioStream` at 22050 Hz, fed 4096 samples at a time. A
song fades out over 3 s (`FADE_OUT`) and rises or ducks over 1 s (`FADE`).
The tapes' dice are view only, seeded from the clock. MUSIC (default 6) is
in `audio::Settings`, `data/settings.txt` and `ConfigPanel`.

## 3. The menu tune

Written into `data/music.txt` in the mood the user chose: lounge chords
(major sevenths, ninths) on the warm square, a walking sine bass, a soft
brush on the off-beats, slow (about 80 a minute), a little out of tune
between voices and wandering with the wow. Three or four short sections
(8 or 16 bars), so the variations have room. The user listens and it is
changed from what they say.

### Built (2026-11-01)

`song menu` in `data/music.txt`: 80 a minute, two rows a beat, wow 14
cents. Three epiano voices (a thin square with a sine tine, a little out of
tune with each other), a sine bass with a square an octave up so small
speakers carry it, a vibraphone tune on a triangle, a brush on two and
four. In F major, four sections of eight bars (about 24 s each): a, the
tune (I–vi–ii–V); b, the bridge; c, the tune gone wrong, its chords sliding
a half step off (Eb, Db, Gb major sevenths); d, the chords alone, no tune or
brush. Flow: a goes to b mostly, else c or d; vary drops a channel 20% of
the time, never the bass. Not yet heard by the user: change it from what
they say.

## 4. The scenes

- `MenuScene`, `ConfigScene`: the menu song at full level.
- `ObituaryScene`: the menu song ducked low (default 35%).
- `WorldScene` while choosing the sleeper: full level. On WAKE UP it fades
  out over the camera easing out (default 3 s).
- Back to the main menu (from the pause menu, or after the obituary that
  follows a death): the song starts again from its first section, fading
  in.
- The pause menu's CONFIGURATION over the world has no music (there is
  none in the world yet).

### Built (2026-11-01)

As planned: `MenuScene` and `ConfigScene` want the menu song, `ObituaryScene`
at 35% (`MUSIC_LEVEL`), `WorldScene` while choosing, and none once awake, so
it fades over 3 s from WAKE UP. `tryOut` (Play from genview) starts awake,
with no music.

## 5. Android

- `data/music.txt` in the APK's assets (`android/apk.mk`, the list beside
  `sounds.txt`).
- On going to the background the music stream and all sounds pause, and
  resume on return. Needs trying on the S25 Ultra (android_plan.md step 4).

### Built (2026-11-01), untried on the phone

`data/music.txt` is in the APK's assets. Pausing needed no code: raylib's
loop on Android stops while the app is in the background (it waits in
`PollInputEvents`), so nothing feeds the stream, which plays out what it
holds (under 0.4 s) and falls silent; coming back, it carries on from the
same place in the tape. Sounds already playing finish (they are short), and
no new ones start.

## 6. The editor: a music tab in the sound editor

`./sounds.sh` gets a MUSIC tab beside the sounds:
- The instruments, each edited with the same layer cards and sliders as a
  sound, played as a note or a chord.
- The song's tempo, rows, wow and channels, and the flow and vary lines.
- A section as a grid: rows down, a column per channel, a note typed or
  picked in each cell; play the section, or the song from here with its
  variations.
- Heard through the game's own renderer (`build/sound_sheet --music`), as
  the sounds are. Saving writes only what changed in `data/music.txt`,
  refused if the file changed underneath, like the other editors.

Its own small plan section (or `AI/sound_editor_plan.md`) when it is
built: the format should settle first, from steps 1–3.

### Built (2026-11-01)

`./sounds.sh` has SOUNDS and MUSIC tabs (`tools/sound_editor/music.js`,
served by `tools/sound_editor.py`). The music list shows each song, its
instruments (♪) and sections (§), each with ▶.
- **The song**: tempo, rows, wow and gain as sliders; channels, flow and
  vary as text. PLAY plays 60 s as the game would.
- **An instrument**: gain, detune, sustain, and the same layer cards as a
  sound (the sound page's `layerCards`, now shared). PLAY NOTE plays the
  note typed beside it.
- **A section**: the grid, beats shaded and bars ruled, the comment column
  for chord names; arrows and Enter move; bad cells show red. PLAY (the
  section alone), PLAY FROM HERE (the song from it), + BAR, − BAR, COPY
  SECTION (a new section after the last; it plays in the song once a flow
  names it).
- STOP (Esc), SAVE (ctrl+s) and REVERT are for the whole song. SAVE sends
  the song's block rebuilt from its parts: the lines of a part not changed
  are kept as they were, comments too, so an unchanged song saves byte for
  byte. Refused if the song changed in the file meanwhile (SAVE again keeps
  the page's), or if the synth complains of it. Muting a layer is for
  listening only, as on the sound page.
- Not done: adding instruments or songs, or removing sections, from the
  page (edit the file for those).

## Later (not in this plan)

- **In-game music**: whether the world has music at all, and if so when
  (the mood of sound_plan.md step 7: waking fog, dread, settling, sleep,
  death), the muzak heard from speakers in some places, humans hearing it
  (a sim feature). Its own steps or plan, once the menu music is done.
- Music for the obituary of one's own human, after a death.

## Build order

1, 2 and 3 together (the format, the player and a first tune, heard in the
menu with the MUSIC row), then 4, then 5, then 6. All built on 2026-11-01.

## Open questions

Defaults to confirm, now built as written here, so the user can hear them:
- The fade on WAKE UP: 3 s, over the camera easing out.
- OBITUARY's duck: 35%.
- The crush on the mix rather than per note, and the same 5 bits at 8 kHz
  as the sounds.
- Back to the main menu: starting the song again from its first section
  (rather than where it left off, which needs it kept playing silently).
