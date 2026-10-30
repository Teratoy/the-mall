# Remembering foul water

A test pass over six headless worlds found Peda (a craver, World seed
1*7919, Rng(1), crowd 4) drinking at the same foul basin four times,
each time noting "DRANK FOUL WATER (x2/x3)" and saying "WHAT'S IN THIS
WATER?" as if for the first time.

Why: drinking stored a FoulWater episode, but choosing water only looked
at the foulness it could see right now (`seenFoul`), and that was nothing
at all to the parched, the erratic and anyone craving flesh. Water out of
sight counted as clean. Each gulp noted the foulness and queued a
surprised line.

The flow "DRANK WATER" then "DRANK FOUL WATER" isn't a per-gulp bug:
kneeling notes "DRANK WATER", then each gulp tastes the water. The xN was
each gulp noting it again.

## User's decisions (2026-10-30)

1. Remembered foul water counts like foul water it sees: as more walking
   (FOUL_COST per foulness) and refused from FOUL_REFUSE, unless parched.
   So it goes to other water it knows first.
2. The memory doesn't fade with time: it believes the water as foul as it
   last found it until it sees that water again.
3. Those with a taste for flesh don't mind water fouled with blood, but
   do mind feces and piss in it. The erratic mind neither. (Before, both
   minded no foulness at all.)
4. One note and one line per drink, not per gulp. The first time it
   tastes foul water there, "DRANK FOUL WATER" and the surprised lines;
   when it knew, "DRANK FOUL WATER AGAIN" and knowing lines.

## Built (2026-10-30)

- `Mess` keeps how much of each cell's foulness came from blood
  (`bloodFoul`), seeping and fading with the rest. `World::filthFoul` is
  the rest. Total foulness is unchanged.
- `Human::water_.foul`: per water cell, the filth and blood foulness it
  last found there. Written on tasting (the cell drunk from and the water
  it sees round it), refreshed whenever it sees a remembered cell (cleared
  water is forgotten), so only seeing changes it.
- `knownFoul` replaces `seenFoul`: what it sees, else what it remembers,
  as far as it minds (parched: nothing; erratic: nothing; craving: the
  filth only). Both the search cost and the refusal use it.
- `drink()` notes and speaks once per drink. Water remembered foul at
  kneeling makes it knowing: note "DRANK FOUL WATER AGAIN", moment
  `FoulAgain` ([foul_again] in data/lines.txt).
- Scenario `foul_water_memory`: a human who tasted foul water and knows a
  clean source goes to the clean one; a parched one with only the foul
  water drinks it, knowingly.

## Open questions

- Should humans tell each other which water is foul (a water tip that
  warns)? Not built.
