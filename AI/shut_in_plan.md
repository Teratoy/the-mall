# Remembering the shut in

A human shut in a slam room with no key cries for help (`human_rescue.cpp`).
Until now, a hearer weighed letting them out only at the instant of the cry:
with no key right then, it forgot about them. A test pass saw Gilla hear Borst
at 1068 s, pick up a key at 1192 and wander off; Borst stayed shut in to the
end.

The user asked: "make people remember that they heard shut in people when
they find keys and when they meet other people (obviously greedy and erratic
people wont care)".

## What it ties into

- The cry and the rescue: `callForHelp`, `hearHelp`, `startRescue`, `letOut`
  in `human_rescue.cpp`; `Want::Rescue` in `choose()`.
- News at a greeting: `newsFor`, `tell`, `deliver` in `human_social.cpp`.
  News someone lacks also makes a human go over and greet them
  (`wouldGreet`), so word spreads the way key and cake tips do.
- Memory: humans know only what they heard or were told. A hearer knows the
  gate only if it has seen that tile; otherwise it knows where the voice came
  from.

## User's decisions (2026-10-30)

1. The memory lasts **10 minutes** from when it last heard of them.
2. Hearing the cry again, or being told again, **refreshes** it.
3. A caring human with no key **goes for a key it knows of** on purpose,
   and **may go to the voice and call back** through the wall.
4. The chance to go is **the same as today**: one roll each time it hears
   them, comes by a key, or is told (about 15%, swayed by generous, brave
   and sociable; x3 for a friend, x1.5 for a known name or a spare key).
   "Greedy" is greed of 0.67 or more (the stingy third), as for sharing.

## Steps

1. **Remember the cry.** `rescue_.cries`, by caller: where (the gate when it
   knows that tile, else where the voice came from), whether that is the
   gate, when it last heard of it, and who told it (or -1, heard itself).
   Everyone keeps it, so news isn't told twice; only those who care act on it.
2. **Weigh it again on getting a key.** Whenever its keys go up (picked up,
   looted, given), it weighs each cry it remembers, as `hearHelp` does now.
   Same refusals: erratic, greedy, hunting, starving, parched, foe,
   suspicious.
3. **Find the gate.** Going with only the voice to go on, it makes for the
   known ground nearest the voice. A locked door it knows near the voice is
   taken for the gate. Got there and none seen: it gives up and forgets.
4. **Pass it on.** At a greeting, a caring human tells someone who hasn't
   heard: "someone's shut in, that way". The listener keeps it as told
   (hearsay), and, with a key, weighs going at once.
5. **Forget it.** When it sees the gate open, sees the caller dead, greets
   the caller, or the memory goes stale.
6. Lines, notes and three scenarios: heard-then-key, told-with-key, and a
   greedy or erratic hearer who does neither.

## Open questions

All answered above.

## Built (2026-10-30)

All six steps, in `human_rescue.cpp` (state in `rescue_`), with the tell in
`human_social.cpp`.

- `rescue_.cries` keeps each caller; `recallCries()` runs every update,
  forgets the over and stale, and weighs again when keys go up.
  `rescue_.out` keeps who it knows got out, so nobody re-tells it.
- `weighRescue()` is the old roll, now also for the keyless (to call back,
  once per cry). It no longer needs the gate's tile known: with only the
  voice it goes to the voice.
- `startRescue()`: to the gate if it knows a way; else, with a key, to the
  edge of what it knows (a frontier) nearest the gate or voice, no farther
  than 12 cells from it, and looks; a locked door it then knows within 8 of
  the voice is taken for the gate. Nothing left to look at: "COULDN'T FIND
  WHERE @ IS SHUT IN", and it forgets them. Keyless, to the known ground
  nearest the voice, where it calls back; the one shut in, in earshot,
  hears it and takes a little hope.
- New `Goal::Rescue` for that walk ("GOING TO HELP @" in the HUD).
- `Tell::Kind::ShutIn` at greetings, first of any news, from carers only,
  to anyone who hasn't heard and doesn't know they got out. Like a route,
  it carries the teller's way toward the gate or voice, so the listener can
  find it. News of it makes a human go over to greet, as tips do.
- Keyless carers who remember a cry want Fetch more (+0.8, keys first).
- Moments at the end of `talk::Moment`: TellShutIn, CallBack, HeardBack,
  KeyForShutIn, LostShutIn.
- Scenarios: `shut-in-key`, `shut-in-told`, `shut-in-greedy`.

Not done: the chat topics don't bring it up; the greeting tell carries it.
In 6 crowded 25-minute worlds it was told 3 times and sent 2 humans for
keys, but no roll came up to go: the odds are as they were.

## Crying out to steps (2026-11-01)

A test pass saw a shut-in human hear steps outside while eating a cake and
say nothing until its next timed cry, by which time the walker had gone.
Now, in `callForHelp`, steps out of sight within 9 cells (`STEPS_HEARD`, as
far as the player hears them) bring a cry at once, whatever it is doing, no
sooner than 10 s after the last such cry (`STEPS_AGAIN`). The cry is the new
moment `HelpSteps` (`[help_steps]`), urgent so it cuts in over a line being
read; the log says "HEARD STEPS OUTSIDE. CRIED OUT". The timed `Help` cry
also goes ahead of small talk now, instead of being dropped when the queue
is full. Scenario: `shut-in-steps`.

## Banging on the door (2026-11-01)

The user asked: "make humans stuck in rooms bang door too".

User's decisions:
- **Who:** anyone shut in a slam room with no key, and a parched human boxed
  in, calling through a locked door (AI/parched_lost_plan.md).
- **When:** each time it cries out (the timed cry, or one to steps heard
  outside) or calls through the door, it walks to the gate and pounds on it
  with its fists a few seconds, then goes back to what it was doing.
- **Heard:** farther than a cry, so people farther off learn someone is shut
  in and may come.
- **Cost:** tiring only: hunger, thirst and tiredness, more than walking and
  less than a fight. No bloody knuckles.

Built (`human_rescue.cpp`):
- A cry or a call sets `rescue_.bangDoor` (`wantBang`). The next `choose()`
  takes it as a reflex (`bangOnDoor`), after a fight or bottle rage but
  before anything it means to do: beside the door it bangs, else it walks
  there (`Goal::Bang`). A wish left 10 s untaken (`BANG_STALE`) is let go,
  and so is one with a key in hand or the door seen open.
- `State::Banging`, `Action::Bang` (hunger 1/400, thirst 1/320, tiredness
  1/900 a second): 2 to 4 s, a blow every 0.4 s, each a `NoiseKind::Bang`
  noise at the door (`bang` in `data/sounds.txt`, heard 22 cells). Drawn as
  the punch's guard and blow by turns, facing the door's side. Broken off
  when something new is seen, the door opens, or a blow lands on it.
- Anyone within 20 cells (`BANG_RADIUS`, a cry carries 12) who can't see it
  hears it as a cry for help with the gate's cell (`hearHelp(..., true)`):
  "HEARD SOMEONE BANGING ON A DOOR", the voice unknown even to a friend.
- Log: "BANGED ON THE GATE" (slam room) or "BANGED ON THE LOCKED DOOR"
  (parched). Moments `BangDoor` (`[bang_door 0.7]`) and `HearBang`
  (`[hear_bang]`, half the time, the first time from each).
- Scenarios: `shut-in-bang` (a hearer 16 cells off hears the banging, not
  the cry); `parched-lost` now checks the banging too. `shut-in-told` hushes
  the caller after its first cry, and its teller is a new seed, since the
  banging otherwise reached the listener, or sent the teller to call back.

In the soak, a shut-in human bangs about every 10 to 20 s, since each cry
to steps brings a bout too. A door above or below is pounded with the
side-on punch: there is no punch drawn up or down.
