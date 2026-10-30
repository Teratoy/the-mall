# Blood plan (2026-10-29)

Decisions (user): punches bleed only when many land in a short time; darts
and spikes pierce, spikes bleed a lot; slashing comes later. Every wound is
its own, with a place on the body. Bleeding drains health and can kill,
proportional to the damage that made the wound. Spikes no longer kill on the
spot, but almost. Bleeding fades as the wound heals; healing time is
proportional to damage. Stain size follows bleed intensity. Everything
stains. Water washes blood off and takes a faint tint that fades in a
minute. Wet blood makes footprints; blood darkens as it dries, and dry blood
stains nothing. Humans pick up blood from their own wounds, from hitting a
bleeding human, from spray, and from walking through it.

Behaviour: bloody and not hungry, not fighting or fleeing, humans go wash at
water they've seen. A healthy human with food meeting a wounded, bloody one
escorts them to the nearest water either of them knows. Blood on someone
with no wound is suspicious: others keep away, unless very hungry and the
bloody one has food. A stain next to a trap gives it away. Humans with few
resources track blood trails, those with many avoid them. Pools cause fear.

## 1. Wounds and bleeding (no visuals yet) — DONE 2026-10-29
Built as planned in `human/wound.h` and `human.cpp`. Final numbers: Blunt
0.02 and Pierce 0.04 health/s per damage, 60 s healing per damage, spikes
leave ≤0.25 health with a 0.35–0.55 wound bleeding ×1.5 (a headless test
had 35/40 dying 10–13 s later), darts' bleeding stops at the dart floor
(open question 1 taken as yes).

### Data (`human/wound.h`)
- `enum class WoundKind : uint8_t { Blunt, Pierce, Slash }`. Slash is in the
  enum but has no source yet.
- `enum class BodyPart : uint8_t { Head, Torso, FrontArm, BackArm, Legs, Feet }`
- `struct Wound { WoundKind kind; BodyPart part; float damage; float bleed; float healLeft, healTime; }`
  - `bleed` is health lost per second, starting at `BLEED_PER_DAMAGE[kind] * damage`.
  - `healTime = HEAL_PER_DAMAGE * damage`; bleed fades linearly to 0 as
    `healLeft` runs out, then the wound is gone.
  - Total health lost is about `bleed0 * healTime / 2`, which is proportional
    to damage², so one heavy wound bleeds you far more than several small ones.
- `Human::wounds` (`std::vector<Wound>`), plus `bleeding()` (sum of bleed)
  and `wounded()`.

### Sources
- **Punches**: `takeHit` adds damage to a `fight_.beating` total that drains at
  `BEATING_DECAY` per second. Once it passes `BEATING_BLEED` (about 3 punches
  in 5 s), a Blunt wound opens on the Head with damage = the total. Further
  punches while over the threshold deepen that wound instead of opening a
  new one.
- **Darts**: `hitByDart` opens a Pierce wound with damage = dart damage, on
  Torso, FrontArm or BackArm (darts fly chest high, and the side depends on
  which way the human faces versus the dart's direction).
- **Spikes**: `impale` no longer kills. It drops health to `SPIKE_LEFT`
  (≈0.1) and opens a Pierce wound on Legs/Feet with heavy damage and
  `BLEED_PER_DAMAGE[Pierce]` boosted (`SPIKE_BLEED_MUL`).

### Live loop (`Human::live`)
- Health -= `bleeding() * dt`. At 0 the human dies from bleeding.
- `HEAL_RATE` (health regen) is paused while any wound still bleeds.
- Wounds heal on their own whatever the hunger level (starving → slower,
  `STARVE_HEAL_MUL`).
- The dead keep bleeding at a fast fade (`CORPSE_BLEED_FADE`), so a pool
  forms under the body and then stops.
- New hunger-table action: `Wash` (step 5). Bleeding itself costs nothing.

Starting numbers (to tune): `BLEED_PER_DAMAGE` Blunt 0.04, Pierce 0.08;
`HEAL_PER_DAMAGE` 30 s; dart (0.16) → ~5 s bleeding, ~0.03 health;
punch wound (0.3) → ~9 s, ~0.05; spike (0.6 ×2 bleed) → ~18 s, ~0.8, i.e.
almost always fatal. See open questions.

## 2. Stains on the world (floor first) — DONE 2026-10-29
Built in new files so the other session's work in human.cpp stayed clear:
`world/blood.{h,cpp}` (`Blood`, owned by `World`, `world.blood()`),
`human/human_blood.cpp` (`Human::spill`: drips and the sprays queued by
hits), `render/blood_art.{h,cpp}` (`drawStain`). Drops land on Floor and
DoorOpen only; anything else is lost until step 3. Humans already remember
the stains they see (`Memory::blood`), which the renderer uses for cells
out of sight, so the memory half of step 6 is done. Corpses pool round the
body (drops 1.3× wider, spread over the body's length). Drip rate is 1.5 +
80×bleed per second; drop radius 0.4 + 45×bleed px, at most 4.

### Data (`world/blood.h`)
- Sparse, per cell, at pixel resolution: `struct Stain { uint8_t amount[CELL_PX*CELL_PX]; float wet; }`.
  `wet` is 1 when fresh and 0 when dry. One value per cell: a new drop on
  a dry stain wets the cell again.
- `World` keeps `std::unordered_map<Vec2i, Stain, Vec2iHash> stains_` plus
  `wetCells_`, a list of cells still drying, so the drying pass skips
  everything already dry.
- API: `World::bleed(Vec2 worldPx, float size, Vec2 dir)`,
  `World::splash(Vec2i from, Vec2 dir, float strength)`,
  `const Stain* stain(Vec2i)`, `bool wetBlood(Vec2i)`, `float bloodAt(Vec2i)`.
- Stains are permanent, except on water (step 3).

### Emission
- **Drops**: a bleeding human drops blood at a rate ∝ `bleeding()`, each drop
  a blob of radius ∝ bleeding (1 px trickle → 3–4 px splotch), under the
  wound's part (feet cell, offset). Standing still builds a pool. Walking
  leaves a trail.
- **Spray** at the moment of a wound-causing hit: drop count and size ∝
  damage, thrown 0–2 cells along the hit direction. Punch: away from the
  attacker. Dart: along its flight. Spikes: all round. Spray lands on
  whatever is there: floor, wall, table, statue base, another human (step 4).
- **Drying**: `wet` falls to 0 over `DRY_TIME` (≈90 s).

### Rendering
- A pass after the floor and wall patterns, before traps, keys and cakes.
  Colour goes from `BLOOD_WET` (≈150,12,18) to `BLOOD_DRY` (≈70,22,16) by
  `wet`, and alpha ∝ amount, so thin smears read lighter. Dimmed like
  everything else when only remembered.
- Out of sight, the viewer sees stains as they remember them (step 6).

## 3. Stains on walls, tables, statues and water — DONE 2026-10-29
Spray now stops at the first non-walkable cell on its way, so it never
passes through walls. A drop on a wall cell with walkable ground south of
it is squashed into the 2 px face rows; wall cells with nothing in front
lose it. Table stains draw after the tables, raised `TABLE_RISE_PX`;
statue base stains draw right after their statue in the depth-sorted pass.
Water keeps `Blood::tint` (per cell, +4/36 per thick pixel, clearing over
60 s) drawn under the glints, in sight only. `Blood::tintWater` is ready
for washing.
- Walls: spray hitting a wall cell stains its south face. The pixel layer
  sits under the face rows.
- Tables and statue bases: the cell's stain is drawn over the art at that
  cell. Statue bodies that rise above the base aren't stained (they'd need
  the stain in statue-art space); later, if wanted.
- Water: blood landing on water doesn't stain. It adds to a per-cell
  `waterTint_` (0..1) that fades to 0 over `WATER_TINT_TIME` = 60 s.
  `WATER` blends toward a dull red by tint, and glints still play over it.

## 4. Stains on humans — DONE 2026-10-29
`SpriteFrame` is now a struct: `px` plus, per pixel, `rest` (index into the
standing frame, -1 off the body) and `part` (a `BodyPart` or `NO_PART`),
filled by `HumanLook::draw` through `Paper::body(part, mapping)`. Arms map
by distance from the shoulder (hand to hand), legs bottom-aligned (kneeling
folds from the top), seated legs along their length, upper body by the
vertical shift. `Human::stains` (`human/body_blood.h`) holds amount and
wetness on the standing pose. `HumanSprites::bloody` redraws the cached
frames when `stains.version` changes; `WorldScene::spritesFor` calls it.
Sources: hits (2 + 15×damage px at the wound), drips (30 ink at the wound),
spray stopped by a bystander (their upper body), punching a wet-bloody foe
(the front hand), stepping into a wet stain of total ≥ 0.04 (feet, then 6
fading footprints; footprints are too thin to wet anyone's feet). Fresh,
thick blood runs one pixel down every 0.4 s; it dries in 90 s.

### Sprite mapping
- `HumanLook::draw(stance)` also outputs, for every pixel, which rest-pose
  (idle) pixel it is, or none (cake, background). Arms moving in a punch
  carry their stains with them.
- `Human::blood`: `std::array<uint8_t, SPRITE_W*SPRITE_H>` amount plus a
  `wet` value per pixel (or per part, if memory gets tight), in rest-pose
  space.
- `HumanSprites` re-bakes the frames with blood when it changes, at most a
  few times a second. Same wet→dry colours as the world.

### Sources
- Own wounds: blood appears at the wound's part and runs down the rest-pose
  columns over time, ∝ bleeding (head wound → face, shirt; leg → shoes).
- Spray from someone else's hit lands on bystanders in its path.
- Punching a human with wet blood on them stains the fist (FrontArm hand pixels).
- Stepping on a cell with wet blood: wet feet. Each step after leaves a
  footprint (2–3 px) on the floor, weaker each time, for up to
  `FOOTPRINT_STEPS` (≈6). Dry blood does nothing.
  Each footprint wipes 30% of the wet blood off the feet. The feet have
  their own, much higher wash threshold (`WASH_FEET_MIN`, measured on the
  feet alone by `BodyBlood::feet()`; the rest uses `WASH_MIN` on
  `total(false)`), so humans don't wash them right away: otherwise walking a
  trail meant wash → walk back through the trail → wash forever. Bloody
  feet still count toward suspicion.
- Human blood dries on the same clock. Dry blood on a human transfers to nothing.

## 5. Washing — DONE 2026-10-29
`State::Washing`, `Goal::Wash`, `Action::Wash` (1/400 hunger a second),
`Pose::Wash` (the crouch frame). `choose()` tries `startWash` first in the
not-hungry branch, before greeting, when the body's blood totals ≥ 0.004
and it isn't afraid or fighting. It walks to the nearest known floor cell
with water beside it (4 directions). Every 0.5 s it takes 1200 ink off, at
most 40 off any pixel, head first, and tints the water it faces (half as
much to the water around it). It looks round each tick, so anything new
sends it back through `choose()`. A fully bloody body takes about 6 s.
Wounds are untouched (open question 3 still stands). Code in
`human_blood.cpp`; hooks in `human.cpp` (state switch, action, choose,
arrive), `world_view.cpp` (pose), `world_scene.cpp` (labels).
- `Goal::Wash`, `State::Washing`, `Action::Wash` with its own hunger cost
  (≈1/400 s), added to `HUNGER_PER_SECOND`.
- In `choose()`: when the human has any blood on them (wet or dry), isn't
  hungry, and isn't fighting or fleeing (`fight_.foe`, `fight_.fear`), it walks to the
  nearest known floor cell next to known water. Priority: after food and
  fights, before greeting and exploring.
- Washing takes time ∝ blood amount and removes blood a bit at a time.
  The water cells next to the washer gain `waterTint_` ∝ what came off.
- Washing doesn't touch wounds. A bleeding human who washes keeps bleeding.

## 6. Seeing blood — DONE 2026-10-29
In `human_blood.cpp`, hooked from `observe()`, `choose()` and `search()`:
- `noticeBlood`: a seen stain of total ≥ 0.02 teaches every trap within one
  cell. A stain ≥ 0.35 is a pool: added to `Memory::pools`, and it and its
  8 neighbours to `Memory::shunned`, which `bloodCost` makes cost 4 extra
  straight steps in `search()`. A wet pool first seen within 6 cells scares
  it 6 cells off for 12 s (not when starving).
- `judge`: someone seen with blood ≥ 0.01 and no open wound is suspicious for
  30 s after last seen so. Suspects aren't greeted, met, followed or fed,
  and their cakes only tempt the starving (`nearestHolder`).
- `keepAway` (in `choose()` right after fights): walks (`Goal::Avoid`,
  "KEEPING AWAY") off from the scary pool, or from a suspect within 5 cells
  to 6, unless starving and the suspect carries cakes.
Note: a fist bloodied by punching a bleeding foe makes the puncher a suspect.
- Fresh vs dry (user, 2026-10-29): the blood on a suspect has a weight
  0..1 = max(freshness by ink, amount ramp from `DRY_MIN` 0.03 to
  `DRY_FULL` 0.15). A spot of dry blood weighs nothing (under
  `SUSPECT_WEIGHT` 0.1, not suspicious); lots of dry blood weighs like
  fresh. Weight scales the trust hit, dread and suspicion time; at
  ≥ `ACCUSE_WEIGHT` 0.5 it is said to their face. Logs "WAS BLOODY WITHOUT
  A WOUND", or "WAS CAKED IN DRIED BLOOD" for heavy dry blood. A known
  suspect seen with blood weighing `HEAVIER` (0.3) more than last time is
  news again.
- By place (user, 2026-10-29): the amount is `BodyBlood::telling()`, total
  weighted by part via `TELLING` (head 3, arms 2, torso and legs 1, feet
  0.25), so one meal's dried mouth and hands (≈0.087) stay mildly
  suspicious, and shoes wet from a trail (≈0.009) fall under
  `SUSPECT_BLOOD`. Freshness is still by ink, unweighted.

### Memory
- `Memory::blood`: a sparse map from cell → a copy of the stain as last
  seen, plus when it was seen. Kept only for cells with blood. The renderer
  uses this for remembered cells.
- What humans see of each other: `wounded()` (an open, bleeding wound
  shows), and how bloody they look (`bloodAmount()`), as with cakes and starving.

### Reactions
- **Traps**: seeing a stained cell within 1 cell of a trap's trigger cell
  teaches that trap, like spotting it (`learnTrap`). Spike and dart victims
  bleed right there, so the first victim gives the trap away to everyone
  after.
- **Pools** (cell amount over `POOL_AMOUNT`): `search()` treats known pool
  cells and their neighbours as very costly, so they're avoided unless
  there's no other way. Seeing a fresh (wet) pool also makes the human
  leave the spot (a short flee from the cell, `POOL_FEAR_TIME`), and it
  won't wander or idle near it.
- **Suspicion**: seeing a human with blood on them but no open wound →
  `gore_.suspects[id]` with a timer, refreshed every time they're seen still
  bloody. Suspects are treated like the feared: kept away from (no
  greeting, sharing or following), but without running. Exception: starving
  and the suspect carries cakes → go for the food as now (fight). Once
  they're seen clean, the suspicion runs out on its timer, so washing
  clears you. Side effect: a healed human who hasn't washed looks just as
  suspicious, which is fine and emergent.
- **Fright**: fresh pools and suspects in sight should also raise
  `Human::fright()` (0–1, today only from `fight_.fear`/`fight_.feared`). A frightened key
  holder may make for a locked door before exploring (`doorUrge()`, added
  2026-10-29), and picks one not lying toward the danger, so the door search
  in `choose()` should keep away from pools and suspects too.

## 7. Escort and trails — DONE 2026-10-29
In `human_blood.cpp`. Escort: a helper (health ≥ 0.7, not hungry, ≥1
cake, calm) offers it in `choose()` before washing to someone within 3
cells who is wounded, bloody, calm, not hungry and not a suspect. The
water is whichever of the two knows nearer; when it's the wounded one's,
the helper learns their way. Helper `Goal::Escort` waits whenever they fall
more than 3 behind; wounded `Goal::Escorted` keeps within 1. At the water
the helper lets go, shows them the water, and waits while they wash, then
leaves (open question 4: wait, then leave). It ends if either gets
hungry, fights, is scared, or dies. Washing now skips spots others stand
on. Trails: hungry with no cakes, `track()` (after `seekFood` fails) walks
to the freshest stain in sight that is fresher than any by its feet, up
to 20 cells off. Carrying ≥2 cakes, remembered wet stains cost 3 extra
steps in `search()` (open question 5: resources = cakes). Tested
headless: escort over 9 cells and the wash after it; a clean 14-cell trail
followed to its fresh end, not followed with 3 cakes. The rich detour
isn't checked by a test.

### Escort
- A helper (health ≥ `HELPER_HEALTH`, not hungry, carrying ≥1 cake) who
  meets (sees within `GREET_RADIUS`) a human who is `wounded()` and bloody
  offers an escort: target = nearest water known to either of them (the
  union of both memories' water cells; the wounded one effectively tells).
- Helper: `Goal::Escort`, walks there at the wounded one's pace and waits
  for them every few cells. Wounded: `social_.leader = helper` with a
  `Goal::Escorted` that reuses the Follow logic.
- On arrival the wounded one washes. The escort ends if either is attacked,
  gets hungry, or the wounded dies.

### Trails
- A trail is a run of known wet stain cells. Direction comes from wetness
  (the freshest end is where the bleeder went).
- Few resources (0 cakes and hungry): `Goal::Track` follows the trail
  toward its fresh end. At the end the usual logic takes over: a bleeder is
  `weak` prey, and a corpse has dropped cakes.
- Many resources (≥2 cakes): trail cells get the pool treatment (costly in
  `search()`), so they steer clear.
- Everyone in between ignores trails.

## Order
1 → 2 → 4 → 5 is the playable core (bleed, stain, get bloody, wash). 3 can
go any time after 2. 6 needs 2 and 4; 7 needs 6. Every step touches
`human.cpp`/`human.h`, and 2–3 touch `world.cpp`/`world_view.cpp`, so
check for other sessions' edits first.

## Open questions
1. ~~Darts "never kill"~~: decided yes, their bleeding stops at the floor.
2. Spikes leave ~0.1 health plus a heavy wound, and nothing stops bleeding,
   so spike victims will nearly always bleed out within ~20 s. Is that the
   intent, or should some survive (more health left, or a random spread)?
3. Should washing a wound (or being escorted) slow its bleeding? Otherwise
   the escort only cleans, it doesn't save anyone.
4. ~~Helper while they wash~~: decided wait, then leave.
5. ~~"Resources" for trails~~: decided cakes only.
