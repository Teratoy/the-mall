# Blame plan (2026-10-30)

Witnesses never put "that one was bloody" together with "here is a fresh
body". A headless run (world seed 5×7919, crowd 4) had Wollen see Moller
"BLOODY WITHOUT A WOUND", find Schman's fresh, half-eaten body 48 s later
("IT COULD STILL BE HERE"), and stay near Moller until Moller killed it.
Gwenge, same world, heard Moller call Schman "TASTY", saw Moller bloody,
then found Schman and said "WHOEVER DID THIS IS CLOSE".

This builds on blood_plan.md step 6 (suspects, `judge`, `keepAway`) and the
body sighting in `observe()`. A witness uses only what it saw itself, or
was told: never `deaths_.killer`.

## User's decisions (2026-10-30)

1. **When the two are tied together**: by personality. Some tie a body to
   whoever they saw bloody before it, some tie a bloody human to a body
   they found before, some both. Within 60 s (`BLAME_WINDOW`) either way,
   and only fresh bodies (not rotting, not bones). Built as:
   - curious ≥ 0.6: both ways (it puts things together);
   - otherwise timid-ish (brave < 0.5): the bloody one seen before, then
     the body (it remembers who frightened it);
   - otherwise: the body first, then a bloody one seen within 10 cells of
     it (`BLAME_NEAR`; it looks out for whoever did it).
2. **What it does about them**: by bravery, as with seeing someone eat a
   body. Trust drops to a foe's. The timid run from them (`fight_.feared`);
   a brave friend of the dead goes for them; anyone else keeps away (the
   suspect keep-away). The erratic only blame, they don't run or fight.
3. **Warning others**: it shouts it at once to anyone in earshot, naming
   them ("MOLLER DID THIS!"). Hearers who don't distrust the shouter (trust
   ≥ 0) and aren't the one named take them for a suspect too, for longer
   and harder the more they trust the shouter. Afterwards, being a foe, the
   blamed one is passed on by the usual gossip as a warning.
4. **How long**: if the dead was someone it knew (met, or knew the name
   of), for good. A stranger's death makes a long suspicion
   (`BLAME_TIME`, 300 s), after which only the low trust is left.

## Steps

### 1. Linking — DONE 2026-10-30
- `Suspect` keeps `seen`, the clock time it was last seen bloody.
- `Gore::fresh`: the fresh bodies it found, with where and when, kept for
  `BLAME_WINDOW`. `Gore::blamed`: who it blames, for whose death.
- On first finding a fresh body (not its own kill): `foundFresh` records
  it and, for those who tie that way, blames the suspect seen bloody most
  lately within the window.
- In `judge`, for those who tie that way: someone seen bloody, without a
  wound, within `BLAME_NEAR` of a fresh body found within the window, is
  blamed for it.

### 2. Acting on it — DONE 2026-10-30
`blame(o, victim)` (`human_blood.cpp`): logs "@ MUST HAVE KILLED #",
trust to below `TRUST_FOE`, suspicion for good or for `BLAME_TIME`, the
reaction by bravery ("RAN FROM @, A KILLER" when timid, once per fright, the existing
"WENT FOR @ FOR WHAT THEY DID" when avenging), dread, and the shout
(`talk::Moment::Blame`, to nobody, about them, fear tone).

### 3. Hearing it — DONE 2026-10-30
`heardBlame` in `human_blood.cpp`, called for each hearer when a Blame
line is said: logs "# SAYS @ IS A KILLER", suspicion for
`BLAME_TIME × (0.5 + trust/2)` and a trust drop, once per one named
(`Gore::toldKiller`), unless it distrusts the shouter, trusts the one named
more, or is them. Told is not seen: a suspicion from hearsay never ties a
body to them later. The shout cuts in ahead of anything still to be said,
like a trap's cry.

### Built (2026-10-30)
Soak (6 worlds, 15 min, crowd 6): 26 blames, 5 shouts believed by
hearers; no new stuck humans or loops. The timid running and the brave
going for them are rare there, so the scenarios cover them.

### 4. Tests
Scenarios `blame-before` (a timid human sees a bloodied stranger, then
finds a fresh body, runs and shouts; a bystander hears it) and
`blame-after` (a human finds a fresh body, then a bloodied stranger walks
up).

## Open questions
- Should blame be passed on in chat as its own episode ("I THINK {WHO}
  KILLED SOMEONE")? For now it rides on the Foe episode and gossip.
- Should the blamed be able to talk their way out (washed, a friend
  vouching)? For now nothing clears it but time, for strangers.
