# Thirst

Hunger made cakes matter: scarce, portable, fought over, shared, lied
about. Thirst should make *water* matter the other way round: plentiful
where it is, but **fixed in place and never carried**. A cake goes where
its holder goes; water makes the human go to it. That one difference is
what ties thirst into everything else: humans gather at the same pools,
settle by them, guard them, wash blood into them and drink each other's
blood out of them.

Today water blocks walking, is where humans wash and escort the wounded,
gets tinted red by washing (Blood::tintWater), and is one of the two
places a despairing human settles (human_hope.cpp). Spawn chunks are dry
(user's call), and only Rooms and WaterMaze chunks have water.

## User's decisions (2026-10-29)

- **Basins:** a small square drinkable basin tile goes in every spawn room
  (this overrides the earlier "spawn stays dry" for drinking only: no
  pools or canals there) and in plazas.
- **Water is never carried**, for now.
- **All extras in:** drinks may be traded for cakes, very bloody water
  sickens, feeding on flesh quenches a little thirst.
- **Rate:** start at the same per-action rates as hunger; tune after
  watching it play.

## 1. Thirst itself

- `Human::thirst` 0..1 next to `hunger`, starting at 0–0.3
  (START_THIRST_MAX). Grows by its own per-action table,
  THIRST_PER_SECOND, beside HUNGER_PER_SECOND, so every action keeps a
  cost for both (the user's every-action-costs rule). Starts as a copy of
  hunger's rates (user), except Eat a little higher (cake is dry) and the
  new Drink 0; tuned later from play.
- THIRSTY 0.5, PARCHED 0.8 (the thirst twins of HUNGRY and STARVING).
  At 1 it loses health at PARCH_RATE 1/40 per s, faster than starving
  (1/60): thirst kills sooner than hunger, as it should.
- Healing (HEAL_RATE) also needs it not to be thirsty; parched wounds heal
  at STARVE_HEAL_MUL, like starving ones.
- New `State::Drinking`, `Action::Drink`, `Goal::Drink`: walk to a floor
  cell beside known water (the same `water(cell)` lookup startWash uses),
  kneel, drink 2 s per gulp, each gulp −0.35 thirst, until under 0.1 or
  interrupted. Sprite: the washing kneel with the head down.
- Log: "GOT THIRSTY" is left out like "GOT HUNGRY" (it would flood);
  "PARCHED", "DRANK", "DIED OF THIRST" go in.
- HUD: a THIRST bar beside HUNGER in the bottom strip (measured, inside
  the strip); status "DRINKING", "PARCHED".

### Built (2026-10-29, build step 1)

`Human::thirst`, THIRST_PER_SECOND beside HUNGER_PER_SECOND (hunger's
rates; Eat 1/300, Drink 0), THIRSTY 0.5, PARCHED 0.8, START_THIRST_MAX
0.35, PARCH_RATE = STARVE_RATE (both at full stack). No healing while
thirsty; parched wounds heal at STARVE_HEAL_MUL. Log: "PARCHED" (hope
−0.1), "DRANK WATER", "DIED OF THIRST". State::Drinking (Pose::Wash),
Goal::Drink, Action::Drink, Want::Drink (base 2 + thirst drive, after
Food in ties); choose() drinks at once when thirsty beside water. Code in
src/human/human_thirst.cpp (seekWater, waterBeside, startDrinking,
drink: 0.35 a gulp every 2 s down to 0.05). Water first seen while
thirsty is news (observe), so an explorer turns to it. Door urge counts
thirst like hunger. Washing, escorts and settling all take basins as
water (`drinkable()` in chunk.h). HUD: three equal bars HP / HUNGER /
THIRST, "DRINKING", "GOING TO DRINK".

Tile::Basin: a 2×2 square of water in a stone rim (world_view.cpp
drawBasin), blocks walking, not sight, takes tint like water. One in
every spawn room 4–6 cells from the waker (farther and the daze hid it);
1–2 in plazas without a fountain (World::placeBasin, floor ring all
round). The soak (tests/stuck_soak.cpp) now prints parched spells,
drinks and deaths by cause.

First soak (30 seeds × 20 min, 166 humans): 359 drinks, 60 parched
spells; deaths 31 thirst, 22 starved, 19 bled, 6 other (78, vs 55 in the
looting soak before thirst). Thirst is now the top killer at hunger's
rates; no new stuck/loop flags (0.00% blocked). Tune after play.

The thirst explore bias (below) is not built: with Want::Drink at 2+ a
known water always wins, and with none known Explore already runs.

## 2. Wanting water

- `Want::Drink` in WANTING with Food's shape: base 2.0, thirst drive adds
  up to 1 more. When both are pressing, whichever drive is higher wins by
  score; no special casing.
- Reflex in choose(): thirsty and standing next to water → drink before
  anything else that isn't a fight (like `hungry() && cakes > 0`).
- No water known: thirst adds to Want::Explore (THIRST_EXPLORE), and
  Explore's goalCost favours cells next to unknown tiles in Rooms/WaterMaze
  chunks it has seen (it knows water lives there once it has seen water
  once). Thirsty key holders get a door urge like hungry ones
  (doorUrge takes max of the two).
- Lesson `lessons.nearWater`: going parched with no water known teaches
  it to stay near water: Wander radius centres on the nearest known water,
  and an unsettled human with the lesson weighs Explore down when the
  nearest known water is far (WATER_TETHER cells). Paid off (+0.1) when it
  drinks within a minute of getting thirsty.

## 3. Blood in the water

This is where thirst meets the blood plan.

- Drinking from a cell with Blood::tint ≥ RED_WATER (0.15): refused unless
  parched, or the drinker has a taste for flesh (`mind_.appetite` ≥ 0.4), who
  drinks it gladly. A parched human who drinks red water gets dread
  (0.15), a log line "DRANK BLOODY WATER" and a `[bloody_water]` line.
- Very red water (≥ 0.5) makes the drinker sick (reuse `mind_.sick`, a
  milder hit than rotten flesh; the flesh-tasters too).
- Tint already clears over TINT_CLEAR_TIME (blood.cpp); check it's slow
  enough that a fouled pool stays undrinkable for a few minutes.
- Washing in the water others drink from is a social act: anyone in sight
  who knows that water as theirs (a settler whose home it is, or anyone
  thirsty) thinks worse of the washer (−0.1 trust, −0.2 if they're a
  settler there), log "FOULED MY WATER", `[fouled]` line. Washers who
  know it's someone's home go to other water when they can.
- Corpses next to water (a body within 2 cells of the drinking cell) put
  most humans off that spot like fresh pools do; the brave and the
  parched drink anyway.

### Built (2026-10-29, build step 2)

Most of the fouling was built with relief step 3 (blood fouls water,
foul water hurts, very foul water sickens). Added here, in
human_thirst.cpp:
- wouldDrink(from, water) is the one test for both choosing water
  (seekWater) and drinking at water already beside it (choose()'s
  reflex, which used to drink anything): refuses water it sees is foul
  ≥ 0.3 or red ≥ RED_WATER 0.15, or with a body it knows within 2 cells,
  unless parched; those craving flesh (appetite ≥ 0.4) and the erratic
  mind neither foulness nor red, the brave (≥ 0.6) don't mind a body.
- Drinking red water: "DRANK BLOODY WATER" and dread 0.15, or, craving,
  "DRANK BLOODY WATER, GLADLY" and no dread; [bloody_water] line.
- Washing (blood or feces) where someone sees it gives offence, once a
  minute per washer (offendDrinkers, called from startWash): −0.2 trust
  from a settler living within 6 cells of that water ("@ WASHED IN MY
  WATER"), −0.1 from anyone thirsty ("@ WASHED IN THE WATER I DRINK"),
  and a [fouled] line to the washer. Washers avoid water a settler they
  see lives by (OTHERS_WATER_COST 400 in human_blood.cpp's startWash).
Test (scratchpad red.cpp): a red basin: the thirsty refused it, the
parched drank it shaken, the craving gladly; a bloody washer at a
settler's basin got one "WASH SOMEWHERE ELSE!" and −0.2 (without the
once-a-minute limit the restarting wash took trust to −1). Soak (30 × 20
min): drinks, parched and thirst deaths unchanged (390 / 58 / 27), no
stuck or loops.

## 4. Water as place: settling and guarding

- Settling (human_hope.cpp) already picks water or a table. Thirst makes
  water the stronger pick: settle prefers water first, table only if no
  water is known.
- A territorial settler at a pool now guards the *drinking cells*: the
  thirsty who come close get a `[my_water]` warning, then guardHome's
  drive-off. A friend is let drink; a stranger who pleads (`plead()`
  extended to water) may be let drink by the sociable; a foe never.
- Hungry and thirsty at once, a settler at water with a cake to spare is
  a natural trading post: a cake (or a tip) in return for a drink. That's
  `Gift::Pay`-shaped: a guard may let a thirsty non-foe drink for a cake
  (greedy guards ask, sociable ones waive it for friends); the drinker
  pays at the end of its drink, and one who drinks and won't pay becomes
  a foe.

### Built (2026-10-30, build step 3)

- Settling (human_hope.cpp brood): home by water if any is known within
  HOME_FARTHEST, else by a table; log "...MADE A HOME BY THE WATER".
- guardHome never counts those it walks with (inGroupWith) or anyone it
  let drink as intruders. A thirsty newcomer to a territorial settler's
  place with water within 2 cells of home gets askedToDrink
  (human_thirst.cpp): not a foe and the guard sociable (≥ 0.45), or the
  newcomer parched and the guard not greedy → let drink 90 s, "LET @
  DRINK MY WATER", [water_free]; guard greedy (≥ 0.5) and the newcomer
  has a cake → let drink for a cake, [water_price], water_.owes; else
  turned away, [my_water] (instead of the plain [guard] line), and the
  newcomer keeps off water within 6 of that home for 300 s unless
  parched (water_.refused, in wouldDrink, which every gulp now re-checks,
  so one turned away mid-drink stops), then guardHome's grace and
  drive-off as before.
- payForWater once quenched: the cake goes over (both +0.1 trust, "PAID
  @ A CAKE FOR WATER"), unless it has none left, or is greedy ≥ 0.67 and
  brave ≥ 0.5 and walks off (50 %): "DRANK @'S WATER AND DIDN'T PAY"; a
  guard who sees it −0.4, [water_cheat], and goes for them.
- talk: my_water, water_free, water_price, water_cheat; condition
  they.parched.
Test (scratchpad guard.cpp, 8 guards of the spawn basin): greedy guards
took a cake from strangers who had one (paid each time), sociable ones
let them drink free, the rest turned them away, and those stopped
drinking. Soaks (30 × 20 min, 10 × 60 min): nobody settled at all, so
guarding never came up there; drinking, deaths and stuck flags unchanged.

## 5. Telling and lying about water

- New `Tell::Kind::Water`: the nearest water the teller knows that the
  hearer doesn't, plus the route, as tile memory only (like door
  telling). Given in newsFor() to a thirsty hearer, first ahead of a cake
  tip when they're more thirsty than hungry.
- Withheld by greedy ≥ 0.67 from non-friends, sold by greedy ≥ 0.5 like
  trap knowledge, never to foes.
- Cunning lie: a "route to water" over a spike plate, built like
  lieFor()'s Route. A told water that isn't there costs the teller −0.15
  like a missing cake.
- Escort: leadEscort/followEscort already lead the bloody to water; a
  parched human with a friend who knows water gets led the same way.
  Small change: Escort gets a reason (wash or drink).

### Built (2026-10-30, build step 4)

- Tell::Kind::Water (human_thirst.cpp waterRoute/waterFor): the way from
  the teller to the nearest water the hearer has never seen, delivered
  like a Route (tiles along it and round it, the water included). In
  exchange(): to a thirsty hearer at least as thirsty as hungry, ahead of
  newsFor's tips; otherwise after them. Free to friends and to those it
  walks with (inGroupWith); kept back by greedy ≥ 0.67 from anyone else;
  sold by greedy ≥ 0.5 to anyone else holding a cake (paid like a trap
  tip, social_.owes); to a thirsty foe, the cunning (greedy ≥ 0.5, brave ≥ 0.4)
  tell a way over a spike plate the foe doesn't know, on to real water
  ("TOLD @ A WAY TO WATER OVER SPIKES"), exposed by learnTrap as before.
- Groups: a thirsty human that knows no water it would drink asks the
  nearest it walks with, every 30 s ([ask_water]), and the first in its
  group in sight who knows water it hasn't seen tells it the way, no
  greeting needed (askGroupForWater).
- Fixed in deliver (affects door-route lies too): route cells never seen
  are marked toldBy before any tiles are revealed; before, seeing round
  one cell hid the next, so the spike plate on a lie was never marked and
  walking onto it never exposed the liar.
- talk: tell_water, sell_water, lie_water, ask_water.
- Escorting the parched to water (built the same day, after the user
  asked): an escort now has a reason (escorting_.drink), and leadEscort waits
  at the water for drinking instead of washing when leading to drink.
  wouldLeadToDrink: not a loner (≥ 0.3 sociable), health ≥ 0.5, not
  hungry, not busy; leads a friend or one it walks with, within 6 cells,
  who is parched, or thirsty and not already going to water. Seeing one
  adds LEAD_PARCHED 1.5 to Want::Escort, which now tries offerEscort
  (wash) then offerDrinkEscort (to water it would drink itself). Not the
  same one again for 30 s. Trust +0.3 on arriving, as for washing. Log
  "TOOK @ TO WATER TO DRINK"; talk [lead_drink], [led_drink]. Test
  (scratchpad lead.cpp, a dazed parched friend 30 cells from the spawn
  basin): led once and drank, 10 of 10. Soak: 4 led to drink, nothing
  stuck, parched 49, thirst deaths 23.
Test (scratchpad tip.cpp): friend and plain stranger told free, greedy
teller sold to a hearer with a cake and told nothing to one without,
stingy kept it back, cunning sent a foe over a plate (now marked), group
member asked and was told. Soak (30 × 20 min): 1 tip at a greeting, 20
group asks (most groups know the same water), parched 58 → 52, thirst
deaths 27 → 23 (within noise), nothing stuck. Tips at greetings are rare
because the thirsty don't start greetings (needy).

## 6. Talk

Partly built 2026-10-29 with relief step 6: [parched] and [foul_water]
(said to nobody), conditions thirsty / parched / they.thirsty. The rest
below waits for the thirst steps they belong to.

New moments in data/lines.txt: `[thirsty 0.4]` (muttered alone allowed,
like waking/despair), `[parched 0.8]`, `[drink 0.3]`, `[bloody_water]`,
`[fouled 0.6]`, `[my_water]`, `[tell_water]`, `[beg_water]`, and
conditions `thirsty` / `they.thirsty` for existing moments (a parched
human's [meet] asks for water first).

## 7. Hope, dread and the rest

- Going parched: lift(HOPE_PARCHED −0.1), like starving. Finding new
  water while thirsty: a small lift (+0.05).
- Drinking calms: dread −0.05 per drink.
- Cannibalism: feeding takes a little thirst off (−0.1), grim but true,
  and makes parched humans with appetite more likely to snap.
- Wakers wake mildly thirsty, so the first thing a human does after
  coming to is often go looking for water.

### Built (2026-10-30, build step 5)

- Hope and dread: thirsty, first sight of water it hadn't seen lifts hope
  0.05, at most once a minute (foundWater, from observe); each drink
  (startDrinking) takes 0.05 dread off. Going parched still costs 0.1.
- Flesh (human_flesh.cpp): a meal of flesh takes 0.1 thirst off; parched
  pushes the mind toward erratic at half starving's rate, times (1 + 2 ×
  appetite), so the parched with a taste for flesh snap sooner.
- Wakers already wake with up to 0.35 thirst.
- Lesson `nearWater` (Lessons, fading like the others, NEAR WATER row in
  the stats menu, whose label column now fits it): learnt hard (+0.5,
  "LEARNT TO KEEP NEAR WATER", [learn_water]) on going parched with no
  water it would drink in reach (gotParched); paid off (+0.1) by drinking
  within 60 s of getting thirsty. From 0.3, strolls start from the water
  it last drank at (strollFrom, chance = the lesson); past 30 cells from
  that water, Explore is wanted up to 0.6 × lesson less (waterTether).
  Groups need nothing more: followers go where the one they keep with
  goes, so a leader's lesson keeps the group near water.
- talk: [thirsty] on getting thirsty, [drink] on starting to drink,
  [learn_water].
Soak (30 × 20 min): 37 learnt to keep near water; parched 48 → 43,
thirst deaths 20 → 21 (no clear effect yet); nothing stuck.

### Changed (2026-10-30): filth round the drinking spot

User asked whether humans avoid drinking near blood, corpses, feces and
piss: only bodies and foul/red water were refused; filth beside the spot
was ignored, and on it only made the walk dearer. Now wouldDrink also
sums filthAround the spot (3×3, only what it knows): Mess::disgust of
piss and feces plus blood (a covered cell 1 wet, a third dry), and goes
elsewhere from 0.25 × (1.5 − modesty): a fresh pile anywhere round puts
everyone off, a fresh puddle the modest, a dry pile the more modest. Not
the parched or the erratic; those craving flesh don't count blood. Test
(scratchpad filth.cpp): feces on two sides of the spawn basin → drank
on a clean side 8 of 8; all sides → nobody drank; all sides, parched →
drank anyway 8 of 8. Soak: parched 47, thirst deaths 22, nothing stuck.

### Added (2026-10-30): drinking from toilets

User asked. With no water it would drink in reach (seekWater finds
none), the parched and the erratic go to a toilet they know and drink
from it (seekToilet; water_.drinkAt is the toilet). Its water is as foul as
0.5 × the toilet's dirt (hurts, and from 0.5 sickens, "THE TOILET WATER
MADE ME SICK"); a clean toilet is safe. It stops once no longer thirsty
(the erratic drink on); see the 2026-11-01 note below. Log "DRANK FROM A TOILET", [toilet_drink];
anyone not erratic who sees it thinks 0.1 less of it (half in its group)
and notes it; the cruel (greedy ≥ 0.6, sociable ≤ 0.4) say [toilet_mock].
Test (scratchpad tdrink.cpp): parched by a dirty toilet, no water known:
drank one gulp, got sick, stopped at 0.58 thirst; a cruel onlooker said
"THAT'S DISGUSTING." Soak: nothing stuck.

## 8. Worldgen and tuning

- New `Tile::Basin`: a 1-cell (or 2×2) square stone basin, blocks
  walking, not sight, drinkable from the 8 cells round it like a table.
  Humans may wash in it too (user), which fouls it like any water: a
  small basin reddens fast, so a bloody washer at spawn costs everyone.
  One per spawn room, on floor with a free ring like tables; one or two
  per plaza. Gets tinted like water. New art in src/render/ (basin_art).
- Soak: add thirst deaths and "time parched" to tests/stuck_soak.cpp
  output, run 30 seeds × 30 min before/after; target thirst deaths ≤
  starvation deaths and no new stuck/loop flags (crowds at pools are the
  new stuck risk: GiveWay should handle the queue at a drinking cell).

## Build order

1. Thirst value, cost table, death, HUD bar, Drink state/goal,
   Want::Drink (+ explore bias), and Tile::Basin in spawn and plazas.
   Soak.
2. Blood in the water (red-water refusal, sickness, fouling).
3. Settling at water + guarding drinking cells + drinks for cakes.
4. Tell::Kind::Water, selling, lying, escort to drink.
5. Talk lines, hope/dread/flesh hooks, nearWater lesson.

### Changed (2026-11-01): toilet drinkers drink enough to last

A test run of five worlds at X16 found humans stopping at a toilet after one
gulp (0.35), as soon as they were no longer parched, so they were parched
again a minute later and looped back, some 8 or 9 times, and one died of
thirst two minutes after a toilet drink. User: only the parched kneel at a
toilet, but once there they drink until no longer thirsty (below THIRSTY,
about two gulps). Scenario `toilet-parched` now checks thirst gets below
THIRSTY.

## Open questions

None yet (basin washing: allowed, user 2026-10-29).
