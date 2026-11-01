# Waking talk

The first minute of a life is the player's first minute of the game, and
today it is nearly silent: the human lies 3–6 s, sits 2–4 s, says one
`[wake 0.8]` line, and then only the `first_*` remarks (key, cake, door,
water, human) break the quiet. The spawn room holds the player's human
alone, with a cake on a table, a half-full bottle and maybe a statue, so
nobody else is there to talk to. Every other human wakes the same way,
alone, in its own chunk.

This plan makes coming to in the mall feel like something: a short inner
monologue that follows the body (eyes shut, sitting up, standing), the
room being noticed through a fog, the quiet of the place, a first call
into the corridors, and the first sense that it goes on and on. All of
it is muttering to nobody, which escape_plan.md already allowed for
waking (user, 2026-10-29).

It ties into: the daze (`escape_.daze`, `dazed` condition), the hunch and
the urge to get out (escape_plan.md), hope and dread, the `Woke` episode
that chat_before talks about, dreams of before (sleep_plan.md), and the
`first_*` moments.

## User's decisions (2026-10-31)

- Memory: pure blank. It knows its own name and nothing else; no hints
  at a life before (step 2).
- Atmosphere: no preference on what the empty mall sounds like. Keep
  `[quiet]` lines to what is surely there (silence, its own steps and
  breath) and name no hum, music or flicker until one is decided.
- Calling out: yes, built as the last step, after the words-only steps
  have been heard in play (step 5).
- Muttering to nobody is drawn in a dimmer bubble of the same size as
  speech, so it reads as thinking aloud.

## What it should feel like

- Slow, broken, then clearer: lines while dazed are short and halting
  ("COLD...", "WHERE..."); as the daze wears off they become whole
  thoughts. The wording carries the daze, not a new mechanic.
- Never a running commentary. A few lines spread over the first minute or
  two, with silence between, ending once it is clear-headed or has met
  someone. The player should want to hear the next one.
- Voice lines give the feeling, never the event (CLAUDE.md rule): not "I
  SEE A STATUE", but "WHY IS EVERYTHING SO BIG?".
- Traits colour it: the timid frightens itself, the curious wonders, the
  brave makes a plan, the homebody wants its bed, the loner is glad of
  the quiet, the sociable longs for a voice.

## Steps

### 1. The coming-to monologue

Three beats instead of one line, each its own moment, each said to nobody:

- `[wake_stir]` while lying, eyes shut, near the end of the lie: the body
  before the mind. "MMH...", "COLD.", "MY HEAD...", "FIVE MORE MINUTES"
  (homebody), "WHO TURNED THE LIGHTS ON?".
- `[wake]` on sitting up: as now.
- `[wake_stand]` on getting up out of the waking sit: steadying itself.
  "OKAY. OKAY.", "LEGS... WORK.", brave "RIGHT. LET'S GO.", timid "I
  DON'T WANT TO GO OUT THERE."

Log: unchanged ("WOKE UP, REMEMBERING NOTHING"); add "GOT UP, STILL IN A
FOG" on standing. Lying and sitting costs are already there; no new
Action.

### Built (2026-10-31)

Moments `WakeStir` and `WakeStand` at the end of `talk::Moment`;
`[wake_stir 0.7]` and `[wake_stand 0.6]` in lines.txt either side of
`[wake]`. The stir is queued once WAKE_STIR_AT (1 s) into the lie, eyes
still shut (`escape_.stirred`); the stand in `startRising()` while
`escape_.comingTo` is still set, with the log line "GOT UP, STILL IN A
FOG". The three beats fall within about the first 10 s, with the bottle's
remark often between the sit and the stand. Scenario `waking-alone`
(seed 9700, as in play).

The new line rolls shifted later dice, so three scenarios needed new seeds
(bottle-refuse 15084, stash-robbed 18007, parched-door 17626), and the soak
showed a wash/follow loop that was already there: bloody feet over the
wash bar one step from the water, under it on arrival. Fixed with
WASH_KEEP (human_blood.cpp): set on washing, it keeps at it down to half
the bar.

### 2. Reaching for memory

While dazed and still in the spawn chunk (or its own waking chunk), once,
it tries to remember and can't: `[blank]`.

- "WHAT WAS I DOING?", "WHAT DAY IS IT?", "WHO AM I... {ME}. I'M {ME}."
  (it knows its own name; it is all it knows), "THERE WAS SOMEONE... NO.
  GONE."
- Dreams of before (sleep_plan.md) are the later echo of this; this step
  only adds the waking side.
- Open question 1: should these be pure blank, or may they hint at a life
  before (a kitchen, a child, a job)? Hints would be the same for no two
  humans and true for none: nothing in the world backs them.

### Built (2026-10-31)

Moment `Blank`, `[blank]` in lines.txt after `[wake_stand]`, always said.
A fog remark (step 3's rules: dazed, alone, silent, 6 s after the last),
once it is up (`!comingTo`), the daze down to BLANK_DAZE (0.75, about 10 s
in) and still in the chunk it woke in (`chunksIn` of one). Log: "TRIED TO
REMEMBER ANYTHING FROM BEFORE. NOTHING". Lines miss a life before without
hinting at one ("I HAD A HOME. DIDN'T I?", "{ME}. I'M {ME}. THAT'S ALL.").
`waking-alone` checks the log line; parched-door moved to seed 17696.
The soak's unmoved-30 s count went 2 → 5, all in the crowded spawn's group
greeting and keeping company, which fog remarks don't touch.

### 3. Seeing the room through the fog

While dazed, the first sight of each thing in the spawn room gets a
remark, at most one every DAZE_REMARK_GAP (~6 s), picked by what it sees
nearest, and only things it has eyes on (Memory, never the world's
truth). New moments, once each per life:

- `[first_statue]` a giant household thing: "WHY IS IT SO BIG?", "AM I
  SMALL?", curious "IS THAT... A SPOON?" ({THING}, the statue's name, as in
  chat_here).
- `[first_bottle]` its own bottle beside it: "IS THIS MINE?", thirsty
  "WATER. SOMEONE LEFT ME WATER.", timid "WHO PUT THIS HERE?". (`see_bottle`
  stays for later bottles.)
- `[first_table]` a table with nothing or a cake on it: "SOMEONE SET THIS
  OUT.", "LIKE IT WAS WAITING FOR ME."
- `[empty_room]` once it has looked all round and found only walls and one
  door: "JUST... WALLS.", "NO WINDOWS."

The existing `first_cake` and `first_door` gain more dazed lines.

### Built (2026-10-31)

Moments `FirstStatue`, `FirstTable`, `EmptyRoom` at the end of
`talk::Moment`, remarked in `observe()` through `fogSight`, with their own
bits (`escape_.fogSights`) and last time (`escape_.fogAt`). Only while daze
is at least DAZE_REMARK_MIN (0.3, the `dazed` condition), past the lie,
DAZE_REMARK_GAP (6 s) after the last one, and sharing one remark a look
with the `first_*` sights.

Changes from the plan:
- No `first_bottle`: `[see_bottle]` already fires for the bottle beside
  it, so it got `dazed` lines instead ("IS THIS MINE?"), and `[first_cake]`
  some too.
- A fog remark only fills a silence while it is alone: nothing else
  queued to say, and nobody alive in sight. Without that, a thirsty human
  busy remarking on the table never asked for water in time and grabbed
  the bottle instead (bottle-only).
- `[empty_room 0.8]` comes once daze is down to EMPTY_ROOM_DAZE (0.6,
  about 16 s in) if it has never seen anyone alive (`social_.lastSeen`).
- `[first_statue]` passes the statue as a Here topic, so `{THING}` and
  `statue.*` work as in chat_here ("WHO MADE A PAINTED ALARM CLOCK THAT
  SIZE?"); a topic makes the wording fixed, with no dice.
- Two lines dropped that hinted at a life before (pure blank memory).

Scenarios: `waking-alone` now runs 40 s (seed 9720) and also checks the
table and empty-room remarks; the stand line has to come within 4 s of
getting up. `waking-statue` stands a clock statue in the spawn
(`SoakProbe::placeStatue`). New seeds again for dice that moved:
bottle-sell and hard-no-sell 13014, stash-robbed 18014.

### 4. The quiet of the place

Atmosphere that comes from no single thing: after it has been up and alone
for a while (no one in sight, nothing new for ~15 s), `[quiet]`, at most
once a minute, only in its first few minutes or when alone again for long.

- "IT'S SO QUIET.", "IS ANYONE ELSE HERE?", "HOW LONG HAS IT BEEN EMPTY?",
  "I CAN HEAR MY OWN HEART.", loner "AT LEAST IT'S QUIET.", hopeless
  later on "SAME WALLS. SAME QUIET."
- Open question 2: what does the mall sound and feel like? A hum from the
  lights, far-off music, nothing at all? Lines will mention only what the
  user decides is there.

Small effect: `[quiet]` nudges dread up a little for the timid and
sociable, down for the loner. Or none (open question 3).

### Built (2026-10-31)

Words only: no dread from the quiet yet (open question 1 stays open).
Moment `Quiet`, `[quiet]` in lines.txt after `[blank]`, always said, with
the log line "ALONE IN THE QUIET". `Human::hearQuiet(alone)` in
human_hope.cpp, called at the end of each look: nobody alive in sight, up
and not talking, nothing new seen and nobody seen for QUIET_AFTER (15 s).
In its first QUIET_EARLY (180 s) no oftener than QUIET_AGAIN (60 s);
after that only once alone for QUIET_LONELY (300 s), no oftener than
QUIET_LONELY_AGAIN (240 s). Exploring keeps finding new floor, so it
comes seldom: once in three minutes in the `quiet` scenario. Lines name
only silence, its steps, breath and heart, as decided.

### 5. Calling out

The first time it steps through a door out of its waking room, alone,
`[call_out]`: "HELLO?", "IS ANYBODY THERE?", brave "HEY! ANYONE?", timid
says nothing. Shouted: heard further than speech (a CALL_RADIUS, through
walls as smells and cries already are).

Anyone who hears a call from someone it can't see gets `[heard_call]`:
"DID YOU HEAR THAT?", "SOMEONE ELSE IS HERE.", timid "STAY QUIET...
STAY QUIET.", sociable "HELLO? WHERE ARE YOU?" (an answer, which the
caller hears in turn: `[call_answered]` "THERE'S SOMEONE!").

Hearing a call is news: it remembers the rough direction, as it does for
a shut-in's cry (shut_in_plan.md). The sociable and curious may walk that
way (a weight on the WANTING row for meeting), the timid steer away. This
gives first meetings a lead-in and is the one step that changes behaviour,
so it will move the golden output.

Open question 4: build calling at all, or keep this plan to words only?

### Built (2026-10-31)

New file `human_call.cpp` and state `call_`. Changes from the plan:
- The trigger is the first door it comes to, open or locked, not the
  first door out of its room: the spawn room is the whole inside of its
  chunk, and its only doors are the locked ways out, so "out of its room"
  would wait for the first key. It calls through the locked door to
  whoever may be beyond. Only alone, up, and in its first CALL_EARLY
  (300 s) of life; once each. The timid keep quiet. Log "CALLED OUT INTO
  THE MALL"; `[call_out]`.
- Said, a call or an answer carries CALL_RADIUS (24) cells through walls
  to everyone who can't see the speaker (`shout`, from `speak()`), and cuts
  in ahead of small talk still queued, as trap cries do, so it is heard
  when it happens.
- Hearing one (`hearCall`), it notes "HEARD SOMEONE CALL OUT" and keeps
  where it came from for CALL_STALE (90 s). The timid are shaken (dread
  0.1) and hush (`[heard_call]` "STAY QUIET... STAY QUIET."); the sociable
  (0.55+), calm and not needy, call back once (`[call_answer]`, log
  "CALLED BACK"), and the caller hearing it notes "MY CALL WAS ANSWERED"
  (`[call_answered]`); the rest wonder (`[heard_call]`).
- Going toward it: new Want::Voice and Goal::Voice ("GOING TOWARD A VOICE"
  in the HUD), wanted while the voice is fresh and its owner unmet and
  unseen, by all but the timid, with the same wanting-out push as
  exploring (OUT_EXPLORE), so it is exploring with a direction: the
  curious and sociable go, an incurious loner explores as before. It
  makes for the edge of what it knows within 10 cells of the voice, else
  the known ground nearest it; "WENT TOWARD THE VOICE", then "GOT AS NEAR
  AS I COULD TO THE VOICE". Seeing the one it heard, it drops it.
- The timid don't steer away; they only hush and are shaken.

Scenarios `call-answered`, `call-follow`, `call-timid`: the caller wakes
beside a locked door out of the spawn chunk, the hearer in the next chunk.
In the soak a voice is gone to 0.1% of the time. New in the soak: a
29-step pacing of Wander in a cramped spot before sleep (s2, flagged as a
loop) and one more 30 s greeting stall (s4); neither is calling.

### 6. It goes on

The first time it crosses into a new chunk while still near its first
minutes: `[it_goes_on]` "MORE OF IT.", "HOW BIG IS THIS PLACE?", "IT
JUST KEEPS GOING.", sure "THIS WAY. IT HAS TO BE THIS WAY." (its hunch).
The third new chunk with no way out yet: "IT NEVER ENDS, DOES IT?" with a
small hope cost (the hope system already has costs for dead ends).

### Built (2026-10-31)

Moments `ItGoesOn` and `NeverEnds`, `[it_goes_on 0.8]` and
`[never_ends 0.8]` in lines.txt after `[quiet]`, said from
`enterChunk()` (human_hope.cpp). It goes on: the second chunk it ever sets
foot in, log "LEFT THE PLACE I WOKE IN. THERE'S MORE OF IT". Never ends:
not the third chunk with a hope cost as planned, but the first new place
whose lift has worn down to nothing (the 6th new one; `later` reaching
HOPE_NEW_CHUNK / HOPE_SAMEISH), log "ANOTHER PLACE LIKE THE LAST. IT NEVER
ENDS". Words only: the hope system already stops lifting there and wears
hope down after, so no extra cost. Scenarios `it-goes-on` and `never-ends`
(`SoakProbe::beenIn` fakes five places already seen); bottle-refuse moved
to seed 15119.

The soak turned up a stall that isn't this step's: in s5 at ~690 s a
watcher stands 30 s telling a sleeping, parched companion to come to water
until it wakes (Idle/Escort). Fixed the same day: nobody is led to drink
(`wouldLeadToDrink`) or to wash (`wouldEscort`) while asleep. Scenarios
`lead-drink` and `lead-sleeper` (the helper tired, so it gives up any
watch, as in the soak; it led the sleeper before the fix).

### 7. Pacing and the scenario

- One shared gap for all of steps 1–6: a muttered line no sooner than
  MUTTER_GAP (~6 s) after the last thing it said, so beats never pile up.
  A dropped beat is dropped, not queued.
- All of it stops the moment it meets someone: then it has someone to
  talk to (meet, chat_before).
- Deterministic: picking a line follows the existing rules; any chance
  (e.g. `[wake_stir 0.7]`) goes through `rng_`.
- New moments at the end of `talk::Moment`, lines in `data/lines.txt`
  under "Waking", each with a `note()` where something happens (standing,
  calling, hearing a call).
- Scenario `waking_alone`: one human in a fresh spawn room for 90 s; checks
  it stirs, wakes, stands, remarks on its bottle, and (step 5) calls out
  past the door. A second, `heard_call`: two humans a chunk apart, one
  hears the other and turns towards it if sociable.

## Build order

1, 3, 2, 4, 6 are words only and can go in one at a time without changing
what anyone does (the golden output changes only by the new log lines).
5 changes behaviour and goes last, after the user has heard the rest in
play.

## Open questions

1. Should the quiet move dread at all, or stay words only?
2. Should it also stretch the waking itself (longer lie and sit, so the
   monologue has room), or keep 3–6 s and 2–4 s?
3. What the empty mall sounds like, if anything beyond silence (asked,
   no preference yet).

## The first lines go first (2026-11-01)

The user: the first lines at spawn shouldn't be interrupted by the one
noticing the bottle. Walking up to the bottle (or anything first seen), its
remark used to go to the front of the queue, so it came out before the
getting-up line still waiting, and that one often never came. Now
`sayHere()` puts a find's remark behind any line from coming to
(`talk::isWaking`: stir, wake, getting up, reaching back) still to say, and
the human stands at the find until both are said. It still walks to the
find as before: holding it back instead had it wander off and remark on the
bottle from afar. `water-refused` wakes the stranger 8 cells off, not 3, so
the settler has come to before the stranger is at its water.
