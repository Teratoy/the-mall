# Talking

Humans say short lines at the moments they deal with each other. Each
line is picked from `data/lines.txt` by the moment, how the speaker is
doing (hunger, fear, health) and its personality (traits), and how it
sees the one it talks to.

## User's decisions (2026-10-29)

- Moments: greetings and tells, cakes and fights, death and blood. No
  muttering alone (yet).
- Shown as a bubble over the speaker's head for a few seconds, and written
  to the life log of the speaker, the listener and anyone overhearing.
- Lines carry effects (not flavour only):
  - pleading moves givers: a hungry human begs a cake holder in sight; the
    holder may be moved (likelier the more sociable, less the greedier) and
    then shares as with a friend, even with someone only hungry;
  - fear is catching: frightened lines scare listeners who don't distrust
    the speaker, more the more they trust them;
  - threats: brave/greedy humans threaten before a cake fight; the timid
    (and the frightened) may hand a cake over, everyone else thinks worse
    of the threatener and may answer back;
  - tone moves trust: warm lines raise the listener's trust a little,
    rude ones lower it.
- Lines may name people when the speaker knows the name: {NAME} the one
  spoken to, {ME} the speaker, {WHO} a third (the dead, the one gossiped
  about). Without the name the line isn't picked.
- Overhearing: anyone within earshot (HEAR_RADIUS cells) who can see the
  speaker hears; they log it, catch fear from it, think worse of
  threateners, and learn any name said aloud whose owner they can see.
- Lines live in an editable text file, loaded at start (F5 reloads it).

## Shape

- `src/human/talk.h/.cpp`: Talk::Moment, Talk::Tone, the line book
  (loaded from data/lines.txt), and picking a line for a speaker.
- Human: `talk(moment, to, about)` queues something to say; it is said at
  the speaker's next update (`speak()`), so callers without the world
  can talk. Saying picks a line, puts it in the speech bubble queue, logs
  it, and every hearer `hear()`s it, which applies the tone's effects.
- `src/render/speech_view.cpp`: bubbles drawn over the world in canvas
  space, stacked so they don't overlap, kept between the HUD strips.

## Built (2026-10-29)

All of the above. Tuning constants are in human_tuning.h (HEAR_RADIUS,
SPEECH_*, TRUST_WARM/RUDE, DREAD_TALK, BEG_*, PLEA_*, THREAT_*, YIELD_CHANCE).
Accusations are only made to suspects within SUSPECT_DIST, at 60%: before
that they were a third of everything said and doubled the foes made.

Headless check, 6 seeds x 20 min, 6 extra humans at spawn, per human:
foes made 0.46 -> 0.62, friends 0.02 -> 0.05; about 9 lines said each; 6
begs, 19 threats (1 cake handed over). Begging is rare: it needs a hungry
human with no cake known lying about and a holder within earshot. Stuck
soak 30 x 20 min: 0% blocked, 0 give-ups, 0 frozen, 1 loop, 4 flips.

## Pacing (2026-10-29)

User asked whether dialogue happens at a realistic pace. A headless probe
(6 seeds x 10 min, 6 extra humans) found partners talking over each other
(80% of replies overlapped the line they answered, 39% started in the same
frame), every queued line said in one tick with its effects (trust, fear,
names) applied up to 5 s before its bubble showed, and greeters walking
off mid-sentence (8% of lines shown to someone out of earshot).

Now: one line at a time per human. `speak()` says the next saying only
when its own bubble is gone, SPEECH_PAUSE (0.35 s) has passed, and the
one it talks to (or anyone talking to it) isn't mid-line or just done;
having spoken last, it lets the other answer first. Picking, logging and
hearing happen as the bubble shows. A line kept waiting SPEECH_PATIENCE
(5 s) by others, or to someone out of earshot, goes unsaid. Cries (Hit,
Ouch) skip turns but only fill a silence (CRY_PATIENCE 0.5 s, and only
into an empty queue). Greeters stay in Greeting while either still talks,
up to GREET_LONGEST (10 s).

After: replies start 0.4 s after the line they answer (median), effects
land with the bubble, no lines shown out of earshot; the only overlaps
left are fight cries. Bubbles still show ~2 s (about 8 letters/s).

## Readable bubbles (2026-10-31)

The user saw a waking human's first line ("WHERE AM I?") replaced at
once by its line for the bottle lying beside it. Nothing overwrote it:
lines already waited in turn. But a short line showed 1.5 s (1.2 s +
0.05 s a letter) and the next followed 0.35 s later, too fast to read.
And with longer bubbles, more lines would wait and go unsaid after 5 s.

### User's decisions (2026-10-31)

- Each line shows 2.5 s + 0.06 s a letter ("HELLO?" ~2.9 s, 40 letters
  ~4.9 s).
- Urgent lines (blows and cries: hit, ouch, darts, spikes, slam; and the
  shout of who killed) may cut in over a line still showing, but only once
  it has shown a minimum (1.5 s). Ordinary lines wait for it to end.
- Without an emergency, lines are kept until they are said: nothing goes
  unsaid for having waited. When an emergency comes, the newest wins: the
  ordinary lines still waiting are dropped, and the urgent one goes first.

### Built (2026-10-31)

- SPEECH_TIME 2.5, SPEECH_PER_LETTER 0.06, SPEECH_MIN 1.5
  (`human_tuning.h`); `talk::isUrgent()` is the cries plus Blame.
- `sayLater()`: an urgent saying goes to the front and drops the ordinary
  ones waiting (an awaited plea or threat among them stops being awaited);
  an older blow still waiting is dropped for the newer.
- `speak()`: an urgent saying at the front ends a line that has shown
  SPEECH_MIN, and is said at once, skipping the pause and the turns. A
  blow no longer goes unsaid after 0.5 s: it waits out the minimum at most,
  and a newer blow replaces it.
- Ordinary lines no longer go unsaid from waiting. Still dropped: a line
  to someone gone out of earshot or dead, and a new one when SPEECH_QUEUE
  are already waiting. A long SPEECH_STUCK (30 s) remains only so a
  speaker can never stand forever awaiting its turn.
- GREET_LONGEST 10 -> 20 s: with lines about twice as long, greeters
  walked off before word was said (lies and sales failed in scenarios).
- `parched-door` runs 160 s, not 100: the keeper's dice per call heard
  fell later once the rolls shifted.
- Soak after: 0 loops, 0 blocks >= 5 s; standing still 30 s went 1 -> 7,
  all Greeting in the crowded spawns: a human greets one partner, then
  the next who walks up, without moving. A line can now be said well after
  its moment (a bottle remarked on 13 s after it was found, the greetings
  in between), as the user chose.
- Scenario `wake-first`: a human waking as in play says its waking line,
  and only then, after the waking bubble's time, the bottle line.

## Not done / later

- Muttering alone (hungry, scared, sitting, a trap going off).
- Accusations spreading suspicion to overhearers.
- Lies about people, replies chained into conversations.
