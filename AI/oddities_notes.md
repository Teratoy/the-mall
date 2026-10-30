# Smaller oddities (not yet planned)

Found in a test pass on 2026-10-30 that read every life log of six headless
worlds (`build/lifelogs 6 25 4 10`, tool in `tools/lifelogs.cpp`; World seed
`s*7919`, `Rng(s)`, crowd 4, as in `tests/stuck_soak.cpp`). Each was too small
for its own plan. Kept here until the user decides on them.

- **Sleeping while bleeding to death.** Seed 2, Ivo 1021-1031: darts, then
  spikes, "I NEED TO STOP THIS BLEEDING", then "FELL ASLEEP", "*YAWN*", "BLED TO
  DEATH". Zemiluorron (seed 2, 849-862) washed off blood while bleeding out.
  Seed 3 Hady fell asleep after "I NEED TO STOP THIS BLEEDING" and survived.
- **Following a corpse.** Seed 4, Rapple 310-324: "FOLLOWING Sury, WHO HAS A KEY"
  while Sury dies at 313. It keeps following for 11 s while a stranger next to
  it says "THIS JUST HAPPENED", and only logs "FOUND Sury DEAD" at 324.
- **Hunger lines that contradict.** Seed 2, Fous: "STARVING" / "NEVER AGAIN.
  I'M KEEPING CAKES." at 1100, then "I'M NOT THAT HUNGRY." at a body 7 s later.
- **Asking a name already told.** Seed 4, Heri says "I'M HERI! AND YOU?" two
  seconds after Sury gave her name; the same again at 115 with Wuerenna.
  The "AND YOU?" line isn't conditioned on the name being known.
- **Contradictory lines in the same breath.** Seed 5, Dad 543-549: "IT'S
  FRESH. IT COULD STILL BE HERE." about a body 240 s old, then "THEY'RE DEAD!"
  then "HA. WHATEVER." Also "IT'S FRESH" said of bodies 70 s or more old.
- **Need-to-go line, then going at once.** Seed 1, Aun 335: "NOT HERE.
  SOMEWHERE QUIET." and "PISSED" in the same second.
- **Company flapping in the log.** Seed 1, Peda: "KEEPING WITH Aun (x8)", and
  "LOST Cote" with "KEEPING WITH Cote" in the same second.
- **Cake changing hands on every hit.** Seed 2, Pechman and Ivo 276-284: "HIT
  ... AND TOOK A CAKE" alternates about 10 times in 3 s. Ivo logs "STARVING"
  while holding the cake.
- **Spotted suspicion repeated.** Seed 1: Borst and Gilla log "Falampo HAD
  DRIED BLOOD ON THEM" 4 and 7 times without ever meeting Falampo.
- **A body left unlooted.** Seed 3: Hut found Tac's body (with a key and 3
  cakes) and took nothing, then was shut in a slam room without a key 56 s
  later. Looting happens only when hungry or wanting the item.
