# Wronged company: leaving the one it hit or robbed

A test pass over six headless worlds (seed 3, crowd 4) found Hut keeping
with Shane, asking to share Shane's cakes, hitting Shane for one, then
logging "LOST Shane" and crying "DON'T LEAVE ME!". Only the victim ever
turned against the aggressor: company and following dropped someone only
when the human itself distrusted them, and the one who did the harm never did.

## User's decisions (2026-10-30)

1. **What it does.** It depends on traits. The cunning stay along as if
   nothing happened, but only when the victim never knew it was them (robbed
   asleep, unnoticed). Anyone else, and the cunning too when the victim knows,
   leaves them with a line.
2. **Voice lines never state the obvious.** No literal account of what just
   happened ("PARTED FROM @, THEY WON'T TRUST ME NOW"); "THEY WON'T TRUST ME
   NOW" is enough. The life log may say it plainly. (Also noted in CLAUDE.md
   and the header of data/lines.txt.)
3. **How long.** Its own trust in them drops, and comes back slowly, slower
   than trust fades. It keeps away from them for as long as that lasts.
4. **What counts.** A landed blow (with or without a cake); robbing them
   asleep, woken or not; a cake handed over under threat.
5. **Following for keys** counts too: it stops following, and won't take up
   following, anyone it wronged.

## Built (2026-10-30)

- `Human::wrong(victim, known)` (human_social.cpp), called from
  `landPunch`, `rob` and `give(Gift::Yield)`. Hitting back the one who last
  hit it, or harming a foe, doesn't count.
- `social_.wronged[id]`: `guilt`, the trust held down (TRUST_WRONGED, -0.3
  each time, up to 1), and `shuns`. `trust()` subtracts the guilt, so it is
  read everywhere trust is. Guilt fades at WRONGED_FADE (a third) of the
  trust fade: 0.3 comes back in about 13 minutes for an average human.
- `shuns` is set unless it is cunning and the victim didn't know. While
  shunned, they aren't picked for company or followed for keys, and if they
  were, it leaves at once: "PARTED FROM @ AFTER WHAT I DID", and a
  `PartWronged` line said to nobody about them.
- A cunning thief whose sleeper never woke keeps with them; the trust drop
  alone may still part them in the usual way.
- Scenario `rob-companion`: a hungry, sociable, uncunning human keeping with
  a greedy one robs them; it must part, and log no LOST / KEEPING WITH /
  FOLLOWING line about them afterwards.

## Open questions

- Should the guilt show in its talk or mind beyond the parting line (hope,
  lines about them later)?
- Should a cunning thief who stays act differently near its victim?
