# Voices

Humans speak in bubbles today, heard as a soft `pop` (and a `cry` in the
Fear tone). This plan gives them voices: the babble of Loudworks' voice
generator (`~/Trinkets/Loudworks/loudworks/voices/`), made in C++ in the
game so it runs on the phone too. It extends step 6 of
[sound_plan.md](sound_plan.md).

## What Loudworks does

Loudworks has three text-to-voice synths. Each reads the text letter by
letter; the same text and settings always give the same samples.

- **Villager** (`villager.py`, from KaTuN): Animal Crossing babble. Every
  letter is a short pitched blip, ~85 ms at speed 1: vowels are sines at
  the vowel's formants (F1–F3) under a glottal pulse, each vowel a little
  higher or lower (i high, u low); consonants are the same noisy
  square-wave click, only its pitch changing by letter; spaces and
  punctuation are pauses (a long one at `.!?`). Each blip's pitch wobbles
  at random, seeded from the text and a seed.
- **Formant** (`formant.py`): cartoon babble whose letters can be
  followed. A buzzy saw plus noise through moving vowel resonances, and a
  recipe per consonant class: a burst for p/t/k/b/d/g (its pitch tells
  which), hiss bands for s/sh/f/z/v/th, a nasal hum for m/n, a rolled r,
  glides for w/y, a breath for h; formants glide between letters;
  phrase fall, a rise before `?`.
- **Princess** (`princess.py`, `english.py`): real English words through
  a Klatt synth. Far bigger (letter-to-sound rules, a lexicon), and made
  for one elegant character: not for a crowd of strangers.

What makes one voice differ from another is a **personality** (base
pitch, speed, wobble, chirp, brightness, formant size: peppy 520 Hz and
quick, normal 340, lazy 240 and slow, cranky 180, snooty 420 and steady,
jock 300, goblin 270 and fast) and the knobs on top: pitch ×, speed ×,
size (formant scale: above 1 a smaller creature), brightness, variation,
seed. Mood in Loudworks (`goblin/mood.py`) is the music mixer, not the
voice: a mood in the voice is a choice of these knobs (the Voice page's
presets: "anxious", "delighted", "displeased" ...).

Goblin Engineering already ships a C++ port of villager and formant
(`GoblinEngineering/extension/goblin_physics/src/voice_synth_core.cpp`,
no Godot types in it), matched to the Python. The Mall takes that port
over (villager and formant only), in its own style, in `src/audio/`.

## Ground rules

- **View only.** Voices are made and played by the view (`Hearing`),
  from what is over a head. Nothing in the sim changes; `make check`
  passes unchanged.
- **Never the sim's dice.** A human's voice is grown from its name and
  look, through its own hash; the wobble of each line from the line's
  text. Same human, same line, same sound.
- **Crushed like everything else**: through the same low-bit step as
  `data/sounds.txt`.
- A `make` tool renders sample voices to .wav (`make voicesheet`). The
  knobs that grow a human's voice are named constants at the top of
  `src/audio/human_voice.cpp` for now; a `data/voices.txt` reloaded by
  F5 can come if they want tuning by hand.

## User's decisions (2026-11-01)

- **Formant**, after hearing both crushed (`make voicesheet`). The
  villager voice is ported too and stays in `voice.cpp` for the sheet,
  but no human speaks with it.
- **Babble from the text**, letter by letter, as Loudworks reads it.
- **Look and traits** give each human its voice: a bigger body is lower
  and its throat bigger, a beard lower still; brave steadier and lower,
  sociable quicker and brighter, greedy harder, curious more sing-song.
  A little seeded wobble on top, from a hash of its name and id (never
  the sim's rng).
- **Tone and mood** change it. Fear: higher, faster, shaking, louder.
  Rude and Threat: lower, harder, louder. Plead: slower, more rise and
  fall, a little higher. Warm: softer, a little higher. Dread makes it
  tremble; lost hope (gloom) makes it slow, low and quiet.
- **It replaces the pop and the cry.** A Fear line is the voice
  frightened, heard through walls as the cry was (urgent).
- **Anything in earshot** is voiced: every line the followed human could
  hear, seen or not, quieter the farther, muffled through walls, next to
  nothing asleep.
- **A VOICES row** in CONFIGURATION (MASTER, SOUNDS, VOICES), saved in
  settings.txt.
- **At speed: the pop above X1.** Faster than life, lines go back to
  the pop (bubbles the player sees) and the cry (Fear), thinned as
  today.
- **No limit** on how many talk at once.

## Steps

1. `src/audio/voice.{h,cpp}`: the port of Loudworks' formant (and
   villager) voice from Goblin Engineering's C++ copy, without its
   parameter tables, plus a shake (tremble) for fear and dread.
   `synth::crush` split out of `synth::render`, so a voice is crushed as
   every sound is.
2. `make voicesheet` (`tools/voice_sheet.cpp`): sample lines in each
   kind and by a few kinds of speaker to `build/voices/*.wav`, and the
   voices of a few humans grown as the game grows them, in each tone.
3. `src/audio/human_voice.{h,cpp}`: a human's voice from its look,
   traits and name, shaded by a line's tone and its dread and gloom.
4. `Hearing` voices each new line in earshot at X1 (the pop and cry
   above it); `audio::speak` plays a voice's samples at a volume and
   pan, clear or muffled, under the VOICES volume.

## Built (2026-11-01)

- `src/audio/voice.{h,cpp}`: `voice::speak(Voice, text)` gives samples
  at `synth::SAMPLE_RATE`. The formant voice and the villager blips are
  ported from Goblin Engineering's `voice_synth_core.cpp` (itself matched
  to Loudworks' Python): same letter recipes, filters and timing, its own
  PCG dice seeded from the line and the voice's seed. New: `shake`, a
  9 Hz tremble in pitch (up to 1.4 semitones) and loudness. About 7 ms to
  make a 3 s line on the PC.
- `synth::crush` is split out of `synth::render`; `audio::speak` crushes
  a voice there (5 bits, 8 kHz, muffled under 450 Hz through a wall) and
  plays it as its own raylib Sound, let go once finished, under MASTER
  and VOICES. Any number talk at once.
- `src/audio/human_voice.{h,cpp}`: `voice::of(human, tone)`. 330 Hz for
  the smallest body, 6% lower per sprite pixel of torso or leg above the
  smallest, 18% lower with a beard; the throat 1.16 down by 0.035 a
  pixel. Brave lowers it (up to 16%) and steadies it; sociable speeds
  and brightens it; greedy hardens and brightens it; curious lifts its
  phrases. Then ±0.2 octave and ±10% speed from a hash of name and id.
  The tone table (`TONE_SHADES`) shades it; dread adds up to 0.35 shake,
  gloom slows it 25%, lowers it 12% and quietens it 30%.
- `Hearing::voices`: at X1 every new line the followed human could hear
  is voiced: loud 0.8 out to 16 cells, Fear 1.0 out to 22 and urgent (it
  carries through walls and sleep as the cry did). Above X1 the pop and
  the cry as before. A few lines are made a frame (3), the rest the next
  frame; the last 48 made are kept, so a line said again in the same
  voice isn't made again.
- CONFIGURATION has a VOICES row (saved as `voices` in settings.txt);
  changing it says "HELLO?" at the new loudness.
- `make voicesheet`: `build/voices/formant_*.wav`, `villager_*.wav` (the
  sample lines by a small, middling and big speaker) and
  `human_N.wav` (`VOICESHEET_ARGS="N"` humans grown as the game grows
  them, two lines in every tone; it prints who each is and its pitch).
- `make check` passes unchanged: nothing in the sim reads a voice.
