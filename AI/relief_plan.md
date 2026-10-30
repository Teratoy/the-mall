# Poop and piss

What goes in comes out. Hunger and thirst give humans a reason to go to
cakes and water; relief gives them a reason to go *away*: somewhere out
of sight, not by their own water, not by their home. What they leave
behind stays in the world like blood does, and everyone who comes across
it learns something from it.

## User's decisions (2026-10-29)

- **Not toned down.** It is as dirty as it really is. Humans feel shame,
  so they avoid going in front of others (the erratic don't care); alone
  and unwatched they just go, and it's dirty.
- **Toilets, but rare**, and they get dirty too. Without one known, a
  human goes somewhere that *feels* isolated without walking far for it
  (a maze dead end, a corner, behind a statue).
- **Modesty is derived** from the four traits, no new rolled trait.
- **Stays for good**, drying slowly like blood: feces darken and grow
  less disgusting, piss stains fade and grow less disgusting. Feces are
  more disgusting than piss. Nobody is put off by the mess while doing it
  themselves.
- **Fresh feces stepped in** dirty the feet and leave a trail, as blood
  does. Disgusting to everyone but the erratic. It can be washed off,
  but that fouls the water with feces, and drinking fouled water damages
  health.

## 1. Bladder and bowels

- Two new 0..1 values on Human: `bladder` and `bowels`.
- **Filled by what goes in**, not by time: each gulp of water adds to
  the bladder (BLADDER_PER_GULP ~0.12), each cake adds to the bowels
  (BOWELS_PER_CAKE ~0.25), flesh more (0.35), and both creep up slowly
  on their own. So someone who never finds water never needs to piss,
  and a cake hoarder who eats well needs to go often.
- Thresholds like hunger's: NEED 0.6 (wants to go), URGENT 0.85 (goes
  now, wherever), 1.0 = an accident.
- Going costs hunger and thirst like every action: new `Action::Relieve`
  in both tables (about Rest's rate).
- Does not kill or hurt. It only presses, embarrasses and leaves a mark.

### Built (2026-10-29, build step 1)

`Human::bladder`, `bowels` (human_relief.cpp): +0.18 per gulp, +0.2 per
cake or bite of flesh (takeIn), and creeping up 1/1800 and 1/3600 per s
(press, from live()). needBar() = 0.6 + 0.15 × modesty(); bursting at 1.
modesty() = (sociable + 1 − brave) / 2, shown as MODEST under the traits;
BODY section (BLADDER, BOWELS) in the stats menu. Action::Relieve costs
Rest's hunger and thirst. Want::Relieve (base 0.8 + up to RELIEF_DRIVE 2
toward bursting), Goal::Relieve, State::Relieving (piss 3.5 s standing,
Pose::Piss; feces 6 s squatting, Pose::Squat, also empties the bladder).
Bursting: goes where it stands (choose() reflex). Needing to go makes it
"needy" like hunger (no greeting).

seekRelief: any cell within 24 cells (8 when bursting) not by water, not
by its home if settled, and that no human it sees could see (watchedAt;
the erratic don't care), ending cost by how many of the 8 cells round it
are open (unknown counts as open; × modesty, less when urgent) plus
others' mess there. Faces a wall beside it if any. Log "PISSED" / "TOOK
A SHIT"; HUD "PISSING" / "SHITTING" / "FINDING SOMEWHERE TO GO".

Mess (world/mess.h): every Dropping kept for good with kind, cell, pixel
spot, size, time, who, seed; freshness dries over 2 h (feces) / 30 min
(piss); disgust() ready for step 2 (feces 1 → 0.3 dry, piss 0.3 → 0.1).
Memory::mess remembers what was seen. Art in render/mess_art.cpp: ragged
yellow puddle that spreads over 2.5 s, rim and glint, fading; feces a
coiled heap (small ones a centred mound, user), dark edge, wet shine,
damp, smear and flecks, going near black-brown when dry (user).

Soak (30 × 20 min, before rebalancing gulp/cake fill): 195 goings, only
2 in anyone's sight (both bursting or erratic), no stuck/loop flags.
Feces outnumbered piss 4:1, so gulps now fill more and cakes less.
After (same soak): 144 pissed, 86 shat, 230 goings, 2 seen by someone
(1 bursting; the other was seen by someone it couldn't see itself),
deaths 23 thirst / 25 starved / 15 bled / 10 other, no stuck/loop flags.

### Built (2026-10-29, build step 2)

Stepping onto feces with freshness × size ≥ FECES_STICKY 0.4
(Human::tread, from update()) puts 4 brown pixels on the feet (`Human::dirt`,
a BodyBlood painted over the blood by HumanSprites::dirty in feces'
colours, drying over 10 min) and prints MessKind::Print footprints for
8 steps, fading, each wiping some wet off, as blood does. Log "STEPPED IN
SHIT" and a little dread for the modest (not the erratic). Piles are not
squashed by stepping yet.

Disgust: every walk (searchFrom) adds messCost: the disgust of the mess
it remembers on a cell × 200 tenths of a cell (a fresh full pile = 20
cells' detour), nothing for the erratic; a human looking for a spot to
go ignores its own mess except fresh feces, and never picks a cell with
sticky feces. First sight of someone else's fresh feces scares the
modest a little (DISGUST_DREAD 0.05 × modesty × disgust). Prints count
as a little feces (disgust 0.15).

Filthy (feet dirt ≥ 0.15): seen once per filthy spell, −0.05 trust from
anyone not erratic (noticeFilth), and humans with modesty > 0.45 won't
greet them (welcomes → repelledBy). Feet can't be washed yet (step 3).

Soak (30 × 20 min): 140 pissed, 85 shat, 6 stepped in it (mostly where a
detour would be long), 4 still filthy at the end; 1 of 225 goings seen;
4 path give-ups (was 1), no stuck/loop flags.

### Built (2026-10-29, build step 3)

Mess::foul per water cell (pools, canals, basins alike), clearing at 1
per FOUL_CLEAR_TIME 900 s (Mess::update from World::update); drawn as a
murky brown over the water (drawWaterFoul in mess_art.cpp), like blood's
red. Human::foulWater spreads half to the water round.

Washing: startWash now also goes for feces on the feet (wantsWashing:
filth ≥ 0.12 × (1.5 − modesty), never the erratic), and filthy feet add
up to FILTH_WASH 1 × (0.5 + modesty) to Want::Wash, or exploring always
won. wash() takes blood off first, then feces (washDirt: 600 ink a wash,
fouling 0.05 per pixel's worth: one stepped-in pile ≈ 0.13). Log
"WASHED OFF SHIT".

Straight into water: facing water beside it (bursting at the edge), piss
reaches into it (PISS_INTO_WATER_PX) and fouls it 0.25 × size instead of
lying there; the erratic, who may now go beside water, drop feces in too
(1.0 × size). Everyone else still keeps its going away from water.

Drinking: each gulp of foul water costs 0.04 × foul health (never below
0.05: foul water hurts, it doesn't kill), and from foul 0.5 makes it sick
for 60 s (mind_.sick: no healing; log "THE FOUL WATER MADE ME SICK"). Water it
can see is foul costs 400 tenths of a cell per foulness to pick, and from
0.3 it won't drink there at all, unless parched or erratic.

Soak (30 × 20 min): 145 pissed, 93 shat, 13 stepped in it, 5 washed it
off, 3 still filthy at the end, 0 sick from foul water (one washing
fouls a cell ≈ 0.13, under the 0.5 for sickness; only feces in water get
there, and few are erratic by water); no stuck/loop flags.

Fouler, longer (user, 2026-10-29: "make water stay foul more, even
because of blood"): foul clears over 3600 s per unit (was 900); blood
going into water fouls it too (Blood::foul, 1 per unit of red, same slow
clearing, long after the red fades); World::foul() sums both and is what
drinking, choosing water and the brown murk use. Washing feces fouls
0.2 per pixel (was 0.05: one stepped-in pile ≈ 0.5), and a basin fouls
all its cells alike. Soak (30 × 20 min): 177 foul water cells, 15 at
≥ 0.5, but still 0 gulps of foul water and 0 sick: everyone sees the
foulness and picks clean water, so only the parched with nothing else
would drink it.

Seeping (user: "don't foul the whole body instantly. it should spread
slowly and last very long"): fouling goes only into the cell it enters
(no instant half to the neighbours, no whole-basin fouling). Mess::update
seeps 0.3 % of the difference per second between side-by-side water
cells, conserving the total, and the total fades by e every 3 h (half in
~2 h). Blood's foulness is handed to the Mess each update
(Blood::takeFoul), so all foulness lives in one map. One full pile in a
basin: ~15 min to even out at ~0.23 a cell, still 0.13 after 2 h; in a
pool (r 6) it dilutes through all ~110 cells within the hour.
Soak (30 × 20 min): 7857 water cells foul at all (spread thin), 4 at
≥ 0.3, 1 at ≥ 0.5; 9 gulps of foul water (≥ 0.1), still 0 sick; no
stuck/loop flags.

### Built (2026-10-29, build step 4)

Tile::Toilet (chunk.h): one cell, blocks walking not sight, takes blood;
World::toilets_ keeps each one's front cell, dirt and seed (world/toilet.h).
Knowing a toilet within reach makes every other spot cost TOILET_PREFER
500 × (0.5 + modesty) × (1 − urgency) more (else nobody ever walked to
one). Placed by World::placeToilets (world/gen_toilets.cpp) after statues: each
slot of a Rooms chunk has a 20 % chance of one, against a straight stretch
of wall with floor round the rest, and of a Maze chunk 15 %, filling the
end of a dead end. Drawn by render/toilet_art.cpp: 6×6 porcelain seen from
above, tank to the wall, the bowl browning and the rim yellow-streaking
with dirt.

Using one: seekRelief takes the front of any toilet it knows as a spot,
up to TOILET_REACH 2× its usual range, by the water too, costing only
its dirt × 60 × (1.5 − modesty) tenths of a cell (a clean toilet beats
any corner; a filthy one loses to a dead end, less so for the modest).
The erratic treat it as any spot. Still never where it's watched. At the
toilet it faces it and what it does goes in (dirt +0.3 a full shit, +0.1
a piss), unless it misses: 5 % + 25 % × urgency + 30 % erratic + 20 % ×
dread, and then it lands on the floor in front as usual. Log "USED A
TOILET" / "MISSED THE TOILET". Nothing cleans a toilet, and (user, 2026-10-29)
a toilet's dirt does not dry like other mess: the disgust it causes only
builds up, never fades (for now).

Soak (30 × 20 min): 22 toilets in 30 worlds, 1 use (a probe at another
step size: 5 of 226 goings). Why so few: only ~8 % of goings happen with
a toilet known within reach (toilets are rare, humans roam); of those,
most skip it because someone else is at it or could see it (shame), so
two humans can swap a toilet back and forth a few times. No stuck/loop
flags.

Waiting at a busy toilet (user: "make them decide based on urgency and
modesty"): Human::waitForToilet, before seekRelief's search. If someone
else stands in front of the nearest toilet it knows within reach and it
can see that, it waits when modesty × (1 − urgency) ≥ TOILET_WAIT 0.25
(standing TOILET_WAIT_GAP 3–5 cells off, idle 1.5 s at a time, log
"WAITED FOR THE TOILET" once), else it goes elsewhere as before; the
erratic never wait. Test (scratchpad queue.cpp): with one on the toilet,
modesty 0.56 waited, 0.30 and 0.33 went elsewhere. Seen there: a waiter
stands idle, so it can be greeted, and the Company want (another
session's) pulled it along while it needed to go; a miss leaves fresh
feces in front, which keeps everyone off that toilet for a good while.
 How dirty a toilet
is comes from the world, not from what the human last saw (to do).

### Built (2026-10-29, build step 5, with groups in mind)

Groups (human_company.cpp, another session's): companions keep within 2
cells of whoever they follow, which made every spot near a group member
"watched", so nobody in a group could go until bursting, in front of
everyone. Now (human_relief.cpp):
- inGroupWith(o): one keeps with the other (social_.companion or social_.leader), or
  both with the same one. watchedAt() doesn't count those it walks with,
  unless they are erratic.
- givePrivacy (in choose(), before waitForCompany): seeing someone it
  walks with off to go or going, it stands back, facing away, 0.8–1.4 s
  at a time (relief_.privacyFor); not when it needs to go itself, never the
  erratic. Test (scratchpad group.cpp, 20 pairs): every one went unseen,
  every follower looked away; groups held together as often as without.
- Modest humans don't take a filthy companion, and drop one who gets
  filthy while in sight (repelledBy in pickCompany / stillCompany).
- The other session's waitForCompany already skips the needy (hungry,
  thirsty, needs to go).

Accidents: bursting, the non-erratic go in their clothes and carry on
(accident()): a puddle at the feet and piss on legs and feet (new body
layer `soaked`, drawn yellow, faint when dry, washed off like feces at a
third the fouling); bowels bursting also feces on the legs and feet, a
pile and a print trail. Log "WET MYSELF" / "SOILED MYSELF". A fright of
FRIGHT_WET 0.3 or more (scare(): a hit, a trap, first sight of a body)
with the bladder ≥ 0.4 wets it at 50 % × (1 − brave): "WET MYSELF IN
FRIGHT". Shame: dread 0.15 × modesty, hope −0.05 × modesty. The erratic
just go where they stand. Dying lets go of bladder and bowels past 0.2
(letGo, from die()). filth() now counts legs and piss too.

Watchers (beSeenGoing, at every going and accident; erratic watchers
mind nothing): going in water they might drink, −0.3 trust (−0.1 in the
group), "SAW @ GO IN THE WATER"; an accident, the cruel (greedy ≥ 0.6,
sociable ≤ 0.4) −0.1, "LAUGHED AT @ FOR WETTING THEMSELVES", others just
note it, the group's kind say nothing; going in the open (not the group)
−0.08 × 2 × modesty × open cells / 8. Seen, the non-erratic note "SOMEONE
SAW ME GO" / "SOMEONE SAW MY ACCIDENT" and take dread 0.1 × modesty.
Talk lines for all this wait for step 6 (talk.cpp is being reworked by
another session).

Soak (30 × 20 min): 214 goings; 9 wet themselves bursting, 21 wet in
fright, 0 soiled; 5 accidents seen, 2 mocked; nobody seen going outside
their group, none seen going in water; 1 Wander loop flag (was 0), no
stuck. The filthy-companion rule changed nothing measurable there (no
filthy companions came up).

### Built (2026-10-29, build step 6)

Talk (talk.h/talk.cpp, data/lines.txt "Thirst and going" section, added
at the end of Moment so the chat moments' offsets stay put): [parched],
[foul_water], [need_to_go], [accident], [stepped_in] said to nobody;
[seen_going] to the nearest who saw; [into_water] by each watcher to
whoever went in the water; [mock] by the cruel; [privacy] by a group
member starting to look away, to the one going; [toilet_wait] to whoever
is on the toilet. New conditions (before the topic ones, which talk.cpp
checks by position): thirsty, parched, need.to.go, bursting (≥ 0.9),
filthy (≥ 0.15), they.thirsty, they.filthy; [refuse] got they.filthy
lines for snubbing the filthy. Checked: all ten moments pick lines, the
file loads without complaint; soak (30 × 20 min) unchanged, nothing stuck.

### Changed (2026-10-30): what goes in passes slowly

User asked whether drinking caused instant pissing: it did. Measured (15
worlds × 20 min): 38 of 85 goings after a drink came within 10 s of it,
and 3 wet themselves within 15 s of drinking (a 2–3 gulp drink put up to
+0.54 straight into the bladder). Now takeIn holds what goes in (relief_.drunk,
relief_.eaten) and press() passes it on by e every WATER_PASSES 90 s (most of a
drink within ~4 min) and FOOD_PASSES 240 s (a meal within ~10 min), same
totals. After: 11 of 87 within 10 s (those already needed to go before
drinking), 0 wet after drinking; soak (30 × 20 min): 139 pissed, 63
shat, 0 bursting accidents (was 9), nothing stuck.

## 2. Finding a spot

- `Want::Relieve` (base about Food's when urgent, low when merely
  needing), `Goal::Relieve`, `State::Relieving` (crouch for poop ~6 s,
  stand for piss ~3 s, facing a wall where it can).
- Where: a search scored by
  - **privacy**: no human in sight, and a cell that few known cells see
    (corners, dead ends, behind statues, the far side of tables);
  - **away from what matters to it**: known water (its drinking spots),
    its home if settled, tables with cakes, the room it is in when others
    are about;
  - **near what it already used**: a spot with its own old mess is
    fine to it, so it keeps coming back; others' fresh mess puts it off.
  - **a known toilet** wins over all of these, unless it's filthy.
- Nearness counts too, a lot: it never walks far for privacy (search
  capped at ~25 cells, less the more urgent), and the more urgent, the
  nearer wins over the better.
  At URGENT it goes on the spot unless someone's watching *and* it's
  modest (below).
- Watched mid-way (someone walks into sight): the modest stop and wait
  or go elsewhere; the shameless carry on.

## 3. What it leaves

A new world layer beside Blood, `Mess` (world/mess.h), same idea as
Stain: per-cell pixels, wet → dry, permanent until something clears it.

- **Piss**: a yellow puddle with a run toward lower ground; dries over
  ~30 min into a faint stain that stays for good, less disgusting as it
  fades. On water it tints it yellow, the way blood tints it red.
- **Feces**: a brown pile, drawn as it is (lumps, smear, flecks round
  it), wet-shiny when fresh; dries over ~2 h, darker and duller, and
  less disgusting, but **stays for good**. More disgusting than piss at
  every age.
- Disgust of a cell = amount × freshness, feces weighted ~3× piss. Dry
  old feces still put some off; a dry piss ring barely.
- **Stepping in fresh feces** dirties the feet (a body-stain map, brown)
  and prints a trail of brown footprints for some steps, exactly as
  blood does. Piss wets the feet only faintly (short damp trail).
- **Washing** it off works like washing blood but **fouls the water**
  (a `foul` value per water cell beside the tint, spreading to
  neighbours). Pooping or pissing straight into water fouls it far more.
- **Drinking fouled water damages health** (per gulp, ∝ foul) and can
  make you sick (`mind_.sick`). Foul clears very slowly from pools, faster
  from canals; basins (small, still) stay foul longest.

## 3b. Toilets

- `Tile::Toilet`: rare, 1 cell, against a wall, in Rooms chunks (~1 in
  6 chunks) and in some dead-end niches. Blocks walking like a table,
  used from the cell in front.
- A toilet's own dirt rises with every use (and with misses: a small
  chance each use, more for the urgent, the erratic, the drunk-on-fear,
  to leave mess on the floor in front). A filthy toilet is avoided like a
  latrine, but still beats going in the open for the modest.
- A toilet is private enough by itself only if nobody's in sight.

## 4. Accidents

- At 1.0 it goes where it stands, standing up, mid-walk: the mess lands
  on its legs/feet as body stains (BodyBlood-style map, new colour), log
  "WET MYSELF" / "SOILED MYSELF", shame.
- **Fear** brings it on: a big scare (spike plate, seeing a kill, being
  beaten) with bladder above ~0.4 can make it go on the spot. Dread
  sources already go through scare(), so it's one hook.
- Dying: bowels and bladder let go on death; corpses leave both.
- Soiled humans want to wash (Want::Wash already exists; dirt counts
  like blood on the legs), and others notice.

## 5. What others make of it

- **Disgust**: everyone but the erratic keeps off fresh mess like fresh
  pools of blood (shunned cells in search, weighted by disgust), and
  never sits, settles or makes a home near it; seeing it adds a little
  dread to the modest. Nobody minds the mess while going themselves.
- **Dirty humans**: someone with feces on its feet or legs is
  disgusting to be near (others keep a step further, greet less, a
  little less trust), until washed. A dirty-footed trail is followable.
- **Seeing someone go**: a modest watcher thinks a little less of them
  (−0.05 trust) if it's out in the open; nothing if it's in a corner.
  Seeing someone wet themselves: the cruel (greedy, low sociable) mock
  them (talk), the kind look away. Shame on the one seen.
- **Fouling water**: seeing someone piss or poop in water you drink from
  is a real offence (−0.3, settlers there −0.5 and may drive them off);
  it's the same hook as washing in someone's water in thirst_plan.md.
- **Tracking and knowledge**: fresh poop says someone was here recently
  (the trail tracker can use it like wet blood: "someone's around"); a
  latrine says people stay near here. Flesh-eaters' poop is darker; the
  sharp-eyed may grow suspicious of whoever they saw leave it.
- **Marking**: a territorial settler may piss at the edges of its home
  (guardHome), and intruders who see fresh marking keep out more.

## 6. Modesty (derived)

- `Human::modesty()` = mix of sociable (cares what others think) and
  1 − brave (afraid of being seen), 0..1. No new trait (user).
- Modest: holds on longer, wants more privacy, more shame when seen,
  more disgust. Shameless: goes nearer, in the open if nobody's there.
- Erratic: no shame, no disgust; goes in front of others and walks
  through mess.

## 7. Talk and log

Log: "RELIEVED MYSELF" (repeats collapse), "WET MYSELF", "SOILED
MYSELF", "SAW @ RELIEVE THEMSELVES IN THE WATER", "STEPPED IN IT".
Lines: `[need_to_go]` muttered, `[go_away 0.6]` (said to a watcher),
`[accident]`, `[mock]`, `[disgust]`, `[fouled]` shared with thirst.

## 8. Look and HUD

- Two poses: squat (new, from the Sit frames lowered) and stand-facing-
  wall (reuse standing with arms down). Small pixel art: puddle, pile.
- HUD: no bars, they're not survival stats. A status word when urgent
  ("NEEDS TO GO"), and in the stats menu (tap name) a small row.

## Build order

1. Bladder/bowels, filling from gulps and cakes, Relieve want/goal/state
   with the isolation search (capped distance, nobody in sight), Mess
   layer (puddle, pile, slow drying), poses, log. Soak.
2. Stepping in it: dirty feet, brown trail, disgust/shunning, dirty
   humans repel.
3. Washing it off fouls water; drinking foul water hurts (with thirst
   step 2).
4. Toilets: rare tile, dirt, misses.
5. Accidents (full, fright, death), body stains, shame, watchers,
   mocking, the erratic.
6. Talk lines.

## Open questions

1. Sleep doesn't exist yet: when it does, can sleepers have accidents? **No** (user, 2026-10-30; see sleep_plan.md).
