# Water bottles

Thirst made water matter by keeping it **fixed in place**: the thirsty go
to it. A bottle lets a little of it travel. Every human wakes with a full
one, so the first thirst is no longer a race to the spawn basin, and water
gets a second life as a *thing*: carried in plain sight, run dry, refilled,
fouled by what it was filled from, poured out, taken off the dead, snatched
in a fight, handed over, sold for a cake, and lied about.

This reverses the thirst plan's "water is never carried, for now".

## User's decisions (2026-10-30)

- **Everyone** wakes with one full bottle, not only the player's human.
- A full bottle is **a whole thirst bar**: drinking all of it takes thirst
  from 1 to 0.
- Drinking from it is **automatic**, decided by the human like everything
  else.
- **Refilling** at any water it finds; parched or erratic, at a toilet too.
- **Separate animations** for drinking from the bottle and filling it;
  actions must stay readable.
- **An object in the world**, though for now bottles only come with humans
  (none lie about). They can be looted, traded, snatched, given and lied
  about.
- **Dirty water carries over**: the bottle holds water as foul and as red
  as what it came from, and drinking it does what drinking that water
  would. Topping up mixes by volume.
- **The bottle is transparent**: anyone who sees it sees how full and how
  dirty it is. A human may **pour it out** to refill it with cleaner water,
  leaving a **puddle that dries out**.
- **Carried on the back, coming out of a pocket**. Its water is the same
  colour as the world's water, clouded the same red and brown.
- **Partial levels show.**

## Calls made while building (for the user to check)

- **Two bottles at most** (BOTTLE_MAX). Taking one more works the same
  way as taking a cake: you take it while you have room. Only the fullest
  one shows in the pocket.
- **Giving and trading pour water**, from the giver's bottle into the
  other's, mixed like any topping up. Only a human with no bottle at all is
  handed one, and only a spare.
- **The lie**: asked for a drink, a cunning human who won't share says its
  bottle is empty. The bottle is see-through, so whoever asked sees the
  water in it and knows it lied.

## 1. The bottle itself

- `Bottle` (src/human/bottle.h): water 0..1, foul, red. `Human::bottles`,
  a public vector like `cakes`, and visible to everyone who sees the human.
- Spawned with one, full and clean.
- Mixing: `Bottle::add(water, foul, red)` keeps foul and red as
  volume-weighted averages.

## 2. Drinking from it (State::Sipping, Action::Drink)

- Thirsty, it drinks from the bottle where it stands, unless water it would
  drink is within BOTTLE_WALK steps (then it walks there and saves the
  bottle). With no water to be had, it drinks from the bottle wherever it
  is.
- A gulp every DRINK_GULP_TIME, each the same DRINK_GULP of thirst and of
  bottle, until quenched or dry.
- It won't drink what it would refuse at a pool (too foul, too red), unless
  parched, erratic or craving. Foul and red bottle water hurts, sickens and
  scares exactly as the water would (one shared `swallow()`).
- Pose: standing, bottle raised to the mouth, tipped up, head back.
- Log: "DRANK FROM MY BOTTLE", "MY BOTTLE RAN DRY"; lines: sip, bottle_dry.

## 3. Filling it (State::Filling, Action::Fill)

- After drinking at water, or beside water it would drink, it fills the
  emptiest bottle that isn't full, a share every FILL_TIME.
- Not thirsty, with the bottle under BOTTLE_LOW and water known: Want::Fill
  sends it to water to fill up, more wanted the emptier.
- From a toilet only when parched or erratic, as for drinking.
- Pose: kneeling at the water, the front arm reaching down, the bottle on
  its side in the water.
- Log: "FILLED MY BOTTLE"; line: fill_bottle.

## 4. Pouring out, and puddles (State::Pouring, Action::Pour)

- At water it would drink, holding a bottle it wouldn't drink from (too
  foul or too red for it, parched aside), or clearly worse than the water
  in front of it, it pours it out first, then fills.
- The water lands as a puddle (MessKind::Water) the colour of what was
  poured, spreading, then drying to nothing over PUDDLE_DRY seconds. A foul
  puddle disgusts a little, by its foulness, while wet.
- Pose: standing, the bottle held out upside down, a stream falling.
- Log: "POURED OUT MY BOTTLE"; line: pour_out.

## 5. Bottles change hands

- **Looted**: going through a body, it takes the bottles while it has room.
  A body's bottles are in plain sight, like its cakes (worthLooting counts
  them when it has none, or only empty ones).
- **Snatched**: thirsty, with no water to drink in its bottle and none it
  can get to, it goes for the nearest human it saw carrying water, as a
  hungry human goes for cakes; each blow that lands takes their fullest
  bottle while there is room.
- **Asked, given, sold, lied about**: thirsty with nothing to drink, it asks
  someone close by whom it saw with water (ask_drink). The asked one:
  - friend, one of its group, or sociable and not greedy, and not thirsty
    itself: pours some into the asker's bottle (give_drink), or hands over
    a spare bottle to one with none;
  - greedy, asker holding a cake: pours it for a cake (sell_drink);
  - cunning, not a friend: says its bottle is empty (bottle_lie). The asker
    sees the water through the glass: "@ SAID THEIR BOTTLE WAS EMPTY. IT
    WASN'T", trust down as for a lie;
  - otherwise refuses (refuse_drink).

## 6. Drawing it

- src/render/bottle_art: the bottle in three turns (upright, on its side,
  upside down), its water filling from the bottom in steps, coloured
  as the world's water with the same red and brown clouds.
- Each sprite frame carries where the bottle goes and which way it is turned:
  the back pocket while standing, walking, crouching or sitting; in the hand
  for sipping, filling, pouring and offering. Lying down, it isn't shown.
- A body's bottles lie beside it, on their side.
- HUD: the carried bottles beside the cake and key counts, each drawn with
  its level.

## Build order

1. Bottle, sipping, filling: the core, and the soak.
2. Pouring and puddles.
3. Sprites, HUD.
4. Loot, snatch, ask/give/sell/lie, with scenarios for each.

## Open questions

- None blocking. The three calls above are the user's to change.

### Built (2026-10-30, all steps)

- `src/human/bottle.h` (the Bottle), `src/human/human_bottle.cpp` (sipping,
  filling, pouring, asking, giving, selling, the lie), `swallow()`,
  `wouldSip()`, `toPour()` in human_thirst.cpp; snatching and looting in
  human_fight.cpp. New states Sipping, Filling, Pouring; goals Fill, Ask;
  actions Fill, Pour (rows in all three need tables); Want::Fill.
- Changed while building:
  - Pouring is judged against the water's true dirt, and only by those who
    mind it. Judging by what they *saw* made the erratic pour and refill
    for ever.
  - Asking reaches only 3 cells, so a human sees someone with water up to
    12 cells off and walks up to them first (Goal::Ask). Asked from
    farther off, the holder walked out of earshot before hearing it.
  - A thirsty human tries asking before it fights for water.
  - A kind friend may lead a bottle-less, parched friend to water instead
    of handing over a spare. Both are fine.
- Art: `src/render/bottle_art`, `src/render/water_art` (one water colour
  for pools, basins, bottles and puddles). `make bottlesheet` draws every
  pose with bottles at several levels and dirt, to `build/bottle-sheet.png`.
- Puddles: MessKind::Water, drying in 120 s.
- Scenarios: bottle-pour, bottle-give, bottle-spare, bottle-sell,
  bottle-lie, bottle-refuse, bottle-snatch. lie-water and the guarded-water
  ones give their thirsty humans empty bottles.
- Soak (6 seeds × 15 min): 127 drinks from a bottle, 69 fills, 16 bottles
  run dry, 14 bodies stripped of bottles, 1 snatched. **No one died of
  thirst** (before: thirst was the top killer). Tune from play if thirst
  should bite again (a smaller bottle, slower filling, or fewer basins).

## One bottle, carried on the cakes (2026-10-30)

User's decisions:

- **Every cake carried piles up in the back hand**, not one shown for all.
- **One bottle at most** (BOTTLE_MAX = 1), standing on top of the cakes;
  with no cakes, in the back hand. The back pocket is gone.
- **Bottle to bottle**: holding one and finding another (on a body, lying
  in a pile, snatched in a fight), it pours the emptier into the fuller and
  keeps the fuller. **Only with clean water** (both under POUR_CLEAN foul
  and red; an empty bottle is clean). A dirty bottle is left where it is.
- **No spares, so a friend gives its only bottle**: asked for water by a
  friend with no bottle, a kind human not thirsty itself hands its own
  over. Never sold.

Built:

- `roomForBottle()`, `wouldTakeBottle()`, `takeBottle()` in
  human_bottle.cpp; looting, piles and snatching all go through them. Its
  emptier bottle is left on the body or the pile; in a fight it falls to
  the floor. Log "POURED MY BOTTLE INTO A FULLER ONE" / "LEFT MY EMPTY
  BOTTLE FOR A FULLER ONE", line swap_bottle. Pouring a pile's bottle into
  its own fuller one stays "FILLED MY BOTTLE FROM ONE LYING THERE"; off a
  body, "TOOK SOME WATER OFF @'S BODY".
- Spare-bottle stashing is gone (a stash never takes the bottle).
- Art: `SpriteFrame::held` (the back hand, or the floor behind when
  seated) replaces `pocket`; cakes are drawn over the frame at run time,
  HELD_CAKE_H rows apart, the eaten one left off; the "carry" textures are
  gone but for the curled sleeper, who hugs one cake.
- Scenarios: bottle-spare became bottle-only; new pile-pour-over,
  pile-top-up, pile-dirty; pile-water checks the swap.
- A human holding an empty bottle won't take a full but dirty one, even
  parched: that follows "only clean water", and the user may want an
  exception there.

## Found, not carried; water is scarce (2026-10-30)

Nobody wakes holding a bottle any more. A bottle lies in the room where
each human wakes, and it has to go back for it. The first time it runs its
bottle dry or fills it, it sees that water is scarce and means to keep it.

User's decisions:

- **One bottle each**, lying a few cells from where each human wakes (one
  human wakes per chunk, the player's in the spawn room). **Half full.**
- **Seen and remembered**: it notes the bottle and says so, and it goes in
  its `Memory` like any pile. It doesn't pick it up then: it leaves it
  until it needs it, thirsty or knowing water to fill it at, and then goes
  back for it.
- **Gone when it comes back**: at first only a loss ("MY BOTTLE IS GONE").
  Once it has learnt the thirst lesson (`lessons.nearWater`, from going
  parched), it is treated as stolen, like a robbed stash: it blames whoever
  it last saw beside it.
- **Water is scarce, once**: the first time it empties its bottle or fills
  it, "WATER IS SCARCE. I'M KEEPING THIS BOTTLE." and a line. From then on
  it won't hand its only bottle to a friend.
- **Holding on hard** once it has learnt the thirst lesson: it pours or
  sells water only for a friend, and never leaves its own bottle in a swap
  (the other is poured into its own instead).
- Wording: my proposal below; the user edits `data/lines.txt`.

Steps:

1. World: `layBottle()` puts a half-full bottle on a free floor cell 2–3
   cells from each human as it is placed by generation (the spawn and every
   chunk), before the human first looks round. `Human()` no longer gives
   one.
2. Seeing it: bottle-less and awake, the first look at a pile holding a
   bottle notes "FOUND A BOTTLE LYING THERE" (moment `see_bottle`); that
   cell is `water_.found`, the bottle it means to come back for.
3. Needing it: bottle-less, a lying bottle (not someone's stash) is taken
   only when thirsty, or when it has drunk somewhere (knows water to fill
   it at). Knowing water and not thirsty, `fillDrive()` sends it back for
   the bottle as for an empty one (Want::Fill, Goal::Pick).
4. Gone: bottle-less, seeing `water_.found` with no bottle there: "MY
   BOTTLE IS GONE" (`bottle_gone`); with the thirst lesson, "SOMEONE TOOK
   MY BOTTLE" and "@ MUST HAVE TAKEN IT" for whoever it last saw beside it,
   trust down as for a robbed stash (`bottle_stolen`).
5. Scarce: `water_.scarce`, set the first time the bottle runs dry or is
   filled; note and `scarce` line in place of that moment's own line.
   `keepsBottle()` stops giving its only bottle; `holdsBottleHard()` (the
   thirst lesson held) narrows pouring and selling to friends and keeps its
   own in a swap.
6. Scenarios: spawn-bottle (seen, left, fetched when thirsty),
   bottle-gone and bottle-stolen, scarce-once, scarce-keeps.

### Built (2026-10-31)

All six steps, as planned.

- `World::layBottle()` (world.cpp): a half-full bottle (`WAKE_BOTTLE`) on
  floor 2–3 steps' walk from each human the generator places, walked out
  so it is never behind a wall or table. The cell is picked with dice of
  its own (from the human's id and cell), so the rest of the mall comes out
  exactly as before.
- `water_.found`, `foundNear`, `scarce`; `spotBottle()`, `bottleGone()`,
  `needsBottle()`, `bottleToFetch()`, `keepsBottle()`, `holdsBottleHard()`,
  `sawScarcity()` in human_bottle.cpp. `wantsFromPile` leaves a lying
  bottle until `needsBottle()`; `fillDrive`/`seekFill` send a bottle-less
  human that has drunk somewhere back for one within 30 cells.
- Changed from the plan: "found" is the first bottle seen lying while it has
  none, its own or one in someone else's room, so a passer-by may also find
  a bottle "gone" once its owner takes it.
- Log: "FOUND A BOTTLE LYING THERE", "MY BOTTLE IS GONE", "SOMEONE TOOK MY
  BOTTLE", "@ MUST HAVE TAKEN IT", "WATER IS SCARCE. I'M KEEPING THIS
  BOTTLE". Moments see_bottle, bottle_gone, bottle_stolen, scarce.
- Scenarios: wake-bottle, bottle-gone, bottle-stolen, scarce-once,
  scarce-keeps, hard-no-sell. The other scenarios start as before (nothing
  lying, everyone handed a full bottle) unless marked `asInPlay`.
- Soak (6 × 15 min): 79 bottles found, 24 picked up, 38 found gone, 1 blamed
  theft, 24 scarcity moments. **Thirst kills again: 17 died of thirst
  (was 6), 1 starved (was 11).** No loops; target flips 1 → 3 (Feed,
  Explore, OpenDoor, none in bottle code). Tune from play if thirst now
  bites too hard (a fuller bottle, or taking it at once).

### Changed (2026-10-31): parched, an empty bottle for a dirty one

User's decision: the exception is wanted. A human whose own bottle is
empty takes a full dirty one if it would drink it (parched, erratic, or
craving blood): `wouldTakeBottle` allows it, and the swap is logged "LEFT
MY EMPTY BOTTLE FOR ONE OF DIRTY WATER" (moment swap_bottle). Dirt still
never mixes: a bottle with any water in it takes only clean water.
Scenario pile-dirty-parched. The soak doesn't reach it (unchanged).
