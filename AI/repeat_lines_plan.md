# Repeat lines: what humans say about what keeps happening

Status: built (2026-11-01): tally, milestones, moments, conditions, body dread and starting lines. Line-writing still to come.

## The idea

Most moments are said once per event, or once ever (`First*`). Nothing
notices that the same thing has now happened for the tenth time. A human
who has eaten its fifteenth cake might remark that they always taste the
same and never go stale; one who has found its fourth body grieves, or no
longer feels anything, depending on who it is. These lines give a long life
a sense of time passing and of the human being worn down, or hardened, by
the place.

## What happens again and again

Counted with `tools/repeat_survey.py` over the life logs `build/lifelog`
prints: 75 lives in four 30-minute worlds (seeds 1–4, crowd 6) and 98 lives
in two 90-minute worlds (seeds 5, 6, crowd 4). Below, from the 90-minute
worlds: how many lives had it 5 or more times, the most in one life, and how
many lives had it at all. The log keeps 300 entries, so the longest lives
are undercounted.

| Event (log line) | Lives 5+ | Most | Lives | Notes |
|---|---|---|---|---|
| ATE A CAKE | 41 | 17 | 82 | Has no moment of its own at all |
| PICKED UP A CAKE | 39 | 19 | 89 | Always off a table, always the same cake |
| DRANK FROM MY BOTTLE | 29 | 19 | 69 | `Sip` each time |
| PICKED UP A KEY | 28 | 30 | 75 | Keys everywhere, doors everywhere |
| ALONE IN THE QUIET | 22 | 11 | 79 | `Quiet`: the same silence again |
| DRANK WATER | 21 | 13 | 73 | Basins, all alike |
| FILLED MY BOTTLE | 18 | 14 | 54 | |
| WASHED OFF BLOOD | 15 | 74 | 47 | One human washed 74 times |
| A DART HIT ME | 14 | 17 | 57 | Caught again and again by the same trick |
| PISSED / TOOK A SHIT | 12 / 7 | 7 / 6 | 68 / 50 | Never a flush, never a sink that works |
| OUT OF BREATH | 11 | 22 | 24 | |
| UNLOCKED A DOOR | 11 | 11 | 65 | Each door opens onto more of the same |
| HIT ME / HIT SOMEONE | 7 / 7 | 11 / 11 | 21 / 16 | Fights, robbing and being robbed |
| MY BOTTLE RAN DRY | 7 | 8 | 60 | |
| RAN FROM SOMEONE | 6 | 10 | 24 | |
| LET THE DOUBLE DOORS BE | 5 | 13 | 15 | |
| DRANK FROM A TOILET | 4 | 13 | 12 | |
| CAUGHT UP WITH / KEEPING WITH | 4 | 8 | 30 | The same few faces |
| PARCHED | 3 | 10 | 35 | |
| ATE FROM A ROTTEN BODY / SICK | 2 | 6 | 5 | Eaters only |
| FOUND SOMEONE DEAD | 1 | 5 | 44 | Rarer, but heavy: 2–5 bodies is a lot |
| FELL ASLEEP / WOKE UP | 1 | 5 | 60 | ~20 min awake, so long lives only |
| TURNED DIRTY, WASHED CLEAN | 0 | 4 | 64 | |
| SPOTTED HIDDEN SPIKES / DARTS | 1 | 6 | 58 | |
| SHUT IN A SLAM ROOM | 1 | 5 | 20 | |
| PAID KEYS AT A MACHINE | 1 | 5 | 11 | |

Not in the log, but just as repeated: passing table after table, statue
after statue, the same rooms and plazas (`NeverEnds` notes this once), and
meeting no one new.

## Candidates, grouped, with what the lines could feel like

1. **Cakes** (eaten, picked up). Always the same taste; never stale however
   long they lie; who bakes them; sick of them; the greedy still glad; the
   erratic craving something else. *Most common of all.*
2. **Bodies** (found dead, bones). By traits: the timid and sociable grieve
   more each time; the brave and loners harden ("ANOTHER ONE."); the
   hopeless see themselves; the curious count. A friend's body breaks
   through any hardening.
3. **Water** (drinking at basins, sipping, filling, running dry). Always the
   same tepid water; where does it come from; counting sips.
4. **Keys and doors**. Another key, another door, more of the same behind
   it; the curious still hopeful, the hopeless numb.
5. **Darts and traps**. Caught again by the same trick: angry at itself,
   resigned, or paranoid.
6. **Blood and washing**. Washing again; it never comes off; the blood
   isn't even its own.
7. **Fights** (hit, been hit, robbed). Getting used to it, or tired of it;
   the brave hardening, the timid breaking.
8. **Going** (pissing, shitting, toilets without flush). Undignified;
   used to it now.
9. **Sleep** (falling asleep, waking). Waking up here again; it hoped it
   was a dream.
10. **The quiet, the sameness** (`Quiet` again, new places). The silence;
    everything looks alike; days lost.
11. **Faces** (meeting the same few again, losing companions again).
12. **Rotten flesh, toilet water** (eaters, the parched). The body stops
    rejecting it; it has sunk this low again.

## Proposed shape (for the user to confirm)

- A per-human tally (`repeat_` struct in `human.h`): one count per kind of
  repeated thing, bumped where the event happens, alongside its `note()`.
- At milestones (say the 4th, 8th, 15th and 30th time) the human may say a
  new moment for that kind, to nobody, with a chance. One new moment per
  group above (`CakesAgain`, `BodiesAgain`, ...), added at the end of
  `talk::Moment` with lines in `data/lines.txt`.
- New conditions so lines can tell early from late: `again` (a few times)
  and `often` (many), so early lines say "AGAIN?" and late ones "I'VE LOST
  COUNT".
- A `note()` per milestone in plain words ("ATE MY 15TH CAKE"), as the life
  log may say it plainly, so the rule that every behaviour logs holds.
- A scenario in `tests/scenarios.cpp` that sets the tally high and checks
  the lines come.

## Orchestration

1. One agent, in its own worktree, builds the tally, milestones, moments,
   conditions, a starting set of lines and the scenario, then `make check`
   and `make golden` (chance rolls change the dice, so the soak moves).
2. Then line-writing agents, one per group, each drafting 10–20 lines by
   trait and condition; their lines merged into `data/lines_drafts.json`
   (one writer to that file) for the user to accept or reject in
   `make lines`.

## User's decisions (2026-11-01)

1. **All twelve groups** in the first batch.
2. **Milestones**: the 4th, 8th, 15th and 30th time, each with a chance.
3. **Bodies change dread, for now**: each body found after the first, the
   hardened (brave, loners) dread a little less, the timid and sociable a
   little more; a friend's body is never softened. Lines otherwise.
4. **Both kinds of lines**: lines marked `again` (early milestones) or
   `often` (late ones), and plain lines in each moment's pool that fit any
   milestone.

### Built (2026-11-01)

- **Tally**: `Human::repeat_` (`Repeats`, in `human.h`), one count per
  `Human::Repeat`: Cakes, Bodies, Water, Doors, Traps, Blood, Fights, Going,
  Sleep, Quiet, Faces, Low. Logic in `src/human/human_repeat.cpp`
  (`repeated()`, `fought()`, `bodyDread()`). Bumped beside the existing
  notes:
  - Cakes: `endEating` ("ATE A CAKE").
  - Bodies: a body it didn't kill, bones too (`seenDead` insert in
    `human.cpp`), with the body's id as `about`.
  - Water: `startDrinking` (basin, dispenser; a toilet too, as it notes
    "DRANK WATER" there) and `startSipping` (bottle).
  - Doors: "UNLOCKED A DOOR", and the door to the humming machines.
  - Traps: a dart, spikes, shut in a slam room.
  - Blood: washing when bloody ("WASHED OFF BLOOD").
  - Fights: each landed blow calls `fought()` on both; it counts only when
    the other is someone new or the last blow between them was more than
    `REPEAT_FIGHT_GAP` (30 s) ago.
  - Going: "PISSED", "TOOK A SHIT".
  - Sleep: every `wakeUp` (not the first waking in the mall, which is
    another path).
  - Quiet: each `Quiet` moment.
  - Faces: a greeting with someone already met ("CAUGHT UP WITH"), about them.
  - Low: rotten flesh (about the body), a drink from a toilet, foul water
    noted as foul (`FoulWater`, `FoulAgain`, `FoulWarned`).
- **Milestones**: `REPEAT_MILESTONES` = 4, 8, 15, 30. Each notes plainly
  ("ATE MY 15TH CAKE", "FOUND MY 4TH BODY", "HAD MY 8TH DRINK OF WATER",
  "UNLOCKED MY 4TH DOOR", "CAUGHT BY A TRAP THE 4TH TIME", "WASHED OFF
  BLOOD THE 8TH TIME", "IN MY 4TH FIGHT", "WENT FOR THE 4TH TIME", "WOKE UP
  HERE THE 4TH TIME", "ALONE IN THE QUIET THE 4TH TIME", "SAW A FACE I KNEW
  THE 4TH TIME", "SANK THIS LOW THE 4TH TIME"), then with
  `REPEAT_SAY_CHANCE` (0.7, through `rng_`) queues the group's moment to
  nobody. The milestone line is queued after the event's own line (e.g.
  `FoundBody`, `Sip`), not instead of it.
- **Moments** (end of `talk::Moment`, in `Repeat` order): `CakesAgain`
  `cakes_again`, `BodiesAgain` `bodies_again`, `WaterAgain` `water_again`,
  `DoorsAgain` `doors_again`, `TrapsAgain` `traps_again`, `BloodAgain`
  `blood_again`, `FightsAgain` `fights_again`, `GoingAgain` `going_again`,
  `SleepAgain` `sleep_again`, `QuietAgain` `quiet_again`, `FacesAgain`
  `faces_again`, `LowAgain` `low_again`. Lines in the new "The same again"
  section at the end of `data/lines.txt`, 6–11 each, with header chances
  0.4 (going, faces) to 0.8 (bodies, traps).
- **Conditions**: `again` (the 4th or 8th time) and `often` (the 15th or
  30th, `OFTEN` = 15 in `talk.cpp`). `Saying::repeat` carries which time it
  is; `say()` sets `repeat_.saying` only while picking that line
  (`Human::repeatSaid()`), so the conditions never hold elsewhere and plain
  lines fit any milestone. Documented in the lines.txt header and
  `tools/lines_editor/conditions.txt`.
- **Body dread**: each body's scare is multiplied by `bodyDread()`:
  `1 - BODY_DREAD_STEP (0.1) * hardness * bodies found before`, clamped to
  `BODY_DREAD_MIN` 0.6 .. `BODY_DREAD_MAX` 1.4, where hardness =
  (brave - 0.5) + (0.5 - sociable), -1 to 1. So a brave loner dreads each
  body about 8% less than the last, down to 0.6 of the first; a timid,
  sociable one more, up to 1.4; a brave but sociable one about the same. A
  friend's body is never below 1. Bodies it killed itself aren't counted.
- **Scenarios**: `repeat-milestones` (tallies at 3: eats its 4th cake and
  finds its 4th body, both noted), `repeat-harden` (a brave loner's dread
  at a body after 5 is under 0.9 of its first), `repeat-grieve` (a timid,
  sociable one's over 1.1).

## For later: other effects of repeats

Not built now; the user wants them noted:
- Cakes: sick of them, eating them a little less gladly (or later).
- Darts: caught often, warier of table cakes for longer.
- Fights: the often hit grow quicker to run, or harder to scare.
- Sleep and waking: waking here again weighs on hope.
- The quiet and the sameness: dread or hope drifting with time alone.
- Washing: the modest washing again and again, the rest giving up on it.
