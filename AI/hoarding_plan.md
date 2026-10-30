# Hoarding

Today a human can only carry what it can carry: a few cakes, up to two
bottles. Going hungry teaches it to carry more (the hoard lesson), but
nothing can be put down, so nothing can be kept for later beyond that.
Hoarding lets humans **put cakes and bottles of water down in a stash**
and come back to them. A stash is the first thing in the mall that belongs
to someone without being on them. It makes scarcity social: a full stash
while others starve, thieves, tips about where food is kept, grudges, and
groups taking turns guarding what they have.

Builds on bottles ([bottle_plan.md](bottle_plan.md)), so it goes on top of
the `bottle` branch.

## User's decisions (2026-10-30)

- **Stashes in the world**: humans put items down in a spot they remember
  and come back to.
- **What**: cakes and bottles of water. Not keys.
- **Who**: everyone, by degree, from traits and lessons: the greedy and
  those who went hungry hoard more, the generous hardly at all.
- **Where**: an out-of-the-way floor cell, chosen like a sleep spot
  (corners, dead ends, nobody it sees watching). Settlers stash at home.
- **Plain sight**: a stash is items lying on the floor, seen by anyone who
  sees the cell, like cakes on a table.
- **Art**: the items themselves, a small heap of cakes and bottles, as many
  as are there, each bottle showing its level and dirt.
- **What others do**: steal from it; talk about it (tell, sell, lie);
  resent the hoarder.
- **Groups guard their stash in turns**: a group shares one stash, and one
  member stays by it while the others go out, like the keep-watch rota.
  The guard drives off or fights thieves.

Second round (2026-10-30):

- **Carry full, stash the overflow**: a hoarder fills its pockets as today
  (carryMax cakes, BOTTLE_MAX bottles) and only stashes what it can't
  carry.
- **Moving**: once robbed, or knowing it was told of, the owner carries
  what it can to a new spot over a few trips.
- **How many**: a group has one stash. A human alone has one or more, by
  how far it ranges: a new stash once it is far from its others.
- **No spoiling**: stashed cakes keep, as cakes on tables do.

## 1. Items on the floor

- World keeps cakes and bottles lying on any floor cell, not only cakes on
  tables. A cell holds a few (STASH_MAX_CAKES, STASH_MAX_BOTTLES); a stash
  can spread to the cells beside it.
- `putDown` / taking: a human kneels to put down or pick up
  (State::Stashing, Action::Stash, rows in all three need tables).
- Memory: cakes it saw lying (already) and bottles lying, with how full and
  dirty they looked (the glass is see-through).
- Walking over loose items no longer picks them up by itself if it knows
  they are someone's stash (that is stealing, step 4).
- Art: cakes and bottles drawn in a small heap where they lie; bottles on
  their side, each showing its water.

### Built (2026-10-30, step 1)

- `Pile` (src/world/pile.h): cakes and bottles on a floor cell.
  `World::piles()`, `putDown`, `takePileCakes`, `takePileBottle`,
  `pourFromPile`. `Memory::piles`, kept up to date by observe().
- src/human/human_stash.cpp: State::Stashing (kneeling, Pose::Search),
  Action::Stash, Goal::Pick. `startPutting` puts cakes and bottles down where
  it stands; `takeFromPiles` (a reflex in choose()) kneels at a pile in
  reach and takes cakes while it has room, bottles with water it would
  drink while it has room (any bottle when it has none), then tops its own
  up from what is left. `pilePath` makes piles count as known food
  (seekFood, Want::Fetch) and water (seekWater, before far water).
- Changed from the plan: no limit per cell yet (a pile holds whatever is
  put on it); the heap shows up to five cakes and two bottles. Walking
  over a pile doesn't take from it: only kneeling does.
- Art: small cakes heaped two by two, the bottles on their side in front,
  end to end. `make shot SHOT_ARGS="--pile --reveal"` shows one.
- Log: "PUT DOWN …", "PICKED UP … LYING THERE", "FILLED MY BOTTLE FROM ONE
  LYING THERE"; lines put_down, pick_up. Scenarios pile-cakes, pile-water,
  put-down. Nothing in normal play puts things down yet, so the soak is
  unchanged.

## 2. Making a stash

- **Hoarding drive**: greedy, plus the hoard lesson, minus sociable;
  stronger when settled or gloomy (no way out, so keep what you have).
  Below a floor it never stashes.
- **When**: its pockets full (carryMax cakes, BOTTLE_MAX bottles) and
  knowing of more it can't carry, a hoarder fetches it to a stash
  (Want::Stash): carries a load to its nearest stash, or makes one if it
  has none within STASH_RANGE, puts it down, and goes back for more. What
  it carries stays on it.
- **Spot**: like seekSleep: the fewest open cells round it, no one it sees
  watching, near where it keeps (home if settled, else where it last
  slept or drank). A human alone makes a new one once its others are all
  farther than STASH_RANGE, so a wanderer leaves a trail of them; a group
  keeps one.
- **Fetching for it**: a strong hoarder fetches cakes it knows of even when
  full, to stock the stash (extends Want::Fetch).
- **Using it**: hungry or thirsty with nothing on it, its own stash counts
  as known food and water, first if near.
- Log: "STASHED A CAKE", "STASHED A BOTTLE", "TOOK A CAKE FROM MY STASH";
  lines: stash, from_stash.

### Built (2026-10-30, step 2)

- `hoardDrive()`: 0.7 greedy + 0.5 hoard lesson − 0.3 sociable, + 0.2 if
  settled, + 0.2 gloom. From 0.3 on it keeps stashes and wants 1 to 8 cakes
  in one (`stashTarget()`).
- Want::Stash (WANTING row, plus the drive): pockets full and knowing of
  cakes it can't carry, with its nearest stash under target, or holding a
  spare bottle at least half full. Goal::Stash walks to its stash within
  30 cells, or to a new spot: floor within 10 of where it keeps (home, its
  sleep spot, where it last drank, else here), not by a door, not on
  anything, nobody it sees watching (`watchedAt`), the fewer open cells
  round it the better. It stashes all but one cake, and the spare bottle.
- Once it has a stash, a hoarder wants Fetch more, by its drive, so it
  refills its pockets and comes back. Before its first stash, fetching is
  as before (giving the bonus earlier changed who fights whom over cakes).
- From its own stash it takes only a cake when hungry with none, or a
  bottle (or water) when thirsty with nothing to drink. Other piles as in
  step 1. A stash seen gone is forgotten.
- Changed from the plan: no new-stash trail yet beyond "none within 30
  cells: make one", which is the same thing.
- Log: "MADE A STASH", "STASHED …", "TOOK … FROM MY STASH"; lines stash,
  from_stash. Scenarios stash, from-stash. bottle-spare's holder now has
  a spare under half full, as a gloomy settler stashes one over half.
- Soak (6 × 15 min): 11 stashes made, 10 bottles and 3 loads of cakes
  stashed; 3 stashed loads picked up by others, as nobody knows yet whose
  they are (step 3). No new stuck humans or loops.

## 3. Whose stash is it

- The world knows who made each stash; a human only knows a stash is
  someone's if it saw them put things there, was told, or (being in their
  group) shares it.
- Items lying with no owner known are just cakes on the floor: anyone takes
  them, as today.
- Owners check their stash: coming back to it, they see what is missing and
  blame whoever they saw near it (or last saw near it), as a sleeper blames
  for a missing cake.

## 4. Stealing, and resentment

- **Stealing**: taking from a stash it knows is someone else's. The hungry
  or thirsty, the greedy and the bold more; never from a friend's unless
  starving; the guilty-minded less. Seen doing it, trust falls as for
  robbing a sleeper.
- **Resentment**: seeing a big stash (or someone stashing) while hungry
  and owning none lowers trust in the owner, more the bigger the stash and
  the hungrier it is. A hungry group with a resented hoarder in sight may
  go for the stash together, or for the hoarder.
- Log: "TOOK A CAKE FROM @'S STASH", "@ TOOK FROM MY STASH",
  "@ HOARDS WHILE WE STARVE"; lines: steal_stash, stash_robbed, resent.

## 5. Talking about it

- Tell::Kind::Stash: where a stash is and whose. Told to friends and the
  group freely (warning or sharing), sold by the grasping, kept back by
  hoarders about their own.
- Lies: the cunning send a foe to a stash that isn't there, or to their
  rival's stash to set them against each other; asked about their own
  stash, hoarders deny it.
- Owners whose stash was told of may move it.
- Log and lines: tell_stash, sell_stash, lie_stash, deny_stash.

## 6. Groups guard in turns

- A group shares one stash, made by whichever member starts one (or the
  leader's, if two exist). Members put extra into it and eat from it
  freely.
- **Guard rota**: while the stash holds anything, one member stays by it
  (Goal::Guard, sitting or standing near), the most rested and least
  needy; the others go out. Tired, hungry or after a turn, the guard hands
  over to the next who comes back, as keep-watch hands over.
- The guard warns off anyone not of the group who comes near, and fights a
  thief who takes something, calling the others if they can hear.
- A guard alone may steal from the stash itself, if greedy and not fond of
  the group (as a watcher may rob a sleeper).
- Log and lines: guard_stash, warn_off_stash, caught_thief, take_turn.

## 7. Tests

- Scenarios: stash (a greedy human with spare cakes stashes them),
  from-stash (hungry, eats from its stash), steal-stash (a hungry stranger
  takes from a known stash and is blamed), resent (a starving human sees a
  big stash), tell-stash, lie-stash, guard-stash (a group keeps a guard,
  hands over, drives off a thief).
- `make coverage` should reach every new log line between soak and
  scenarios.

## Build order

1. Items on the floor, the heap art, putting down and picking up.
2. Making and using a stash (one human).
3. Ownership, checking, blaming.
4. Stealing and resentment.
5. Talking about stashes.
6. Groups and the guard rota.

## Open questions

- None blocking. Tuning (the drive's weights, STASH_RANGE, how often a
  guard hands over) comes from watching it play.
