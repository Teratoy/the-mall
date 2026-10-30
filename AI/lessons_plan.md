# Lessons learnt once

Self-lessons (`learn()` in `src/human/human.cpp`) are what going without
teaches a human: keep more cakes, take keys first, keep near water, find a
safe place to sleep in time. A test pass over six headless worlds found
them announced over and over, and the water lesson said at the wrong time.

## What was seen

- The note and the line come on every hard lesson, though the strength
  caps at 1. Seed 4, Hah: "LEARNT TO KEEP NEAR WATER" five times in ten
  minutes; seed 5, Warbacurn, "LEARNT TO TAKE KEYS FIRST" twice.
- The water lesson is learnt the moment it goes parched with no water it
  can reach. So "I THOUGHT I'D DIE OF THIRST." is said while it is still
  dying (seed 1, All, said it and died of thirst two minutes later), and
  humans that never found water at all learn to keep near it.
- The lesson only does anything once it has drunk (it keeps near the
  water it last drank from), so for those it is an empty promise.

## User's decisions (2026-10-30)

- Q1 Repeats: lessons keep reinforcing and slowly fading, but slower: all
  of one in an hour, not half an hour. The first time (or once all but
  forgotten) it is "LEARNT TO ..." with the lesson's own line, as before.
  A repeat while it still holds the lesson gets an "again" note and line;
  the hard lesson that takes it to full strength gets a line of its own.
- Q2 Water: still learnt and said when it goes parched with no water it
  can reach. The survival lines ("I THOUGHT I'D DIE OF THIRST.") move to a
  new moment at its first drink after thirst has hurt it (health lost to
  thirst, not just being parched).
- Q3 No water known: the lesson does nothing then, as now (it only keeps
  near water it has drunk from).

## Steps

1. Repeats. `learn()` looks at the lesson before the hard blow: below
   `LESSON_HELD` it is learnt anew ("LEARNT TO KEEP NEAR WATER", the
   lesson's moment); reaching full strength, "LEARNT FOR GOOD TO ..." and
   `LearnMax`; otherwise "LEARNT AGAIN TO ..." and `LearnAgain`. The two
   new moments' lines are shared by all four lessons. `LESSON_FADE` halves.
2. Quenched. Thirst taking health sets `water_.thirstHurt`; its next drink
   (a toilet counts) notes "DRANK, AFTER NEARLY DYING OF THIRST" and says
   `Quenched` in place of `Drink`. The timid line "I THOUGHT I'D DIE OF
   THIRST." moves there from `learn_water`.
3. Scenarios: a lesson learnt twice is announced as decided; the water
   line comes only after it drinks.

## Open questions

None.

### Built (2026-10-30)

All three steps, as planned. A full lesson fades a little between blows,
so "for good" is only said when it was more than `LESSON_HELD` short of
full; nearer than that, it is "again". Scenarios `lesson-again` and
`quenched`. The soak's first-time lesson counts drop (seed 1, eight water
lessons to three); the slower fade shifts seed 4 a little.
