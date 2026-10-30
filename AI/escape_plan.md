# Waking and wanting out

The premise: a human wakes up somewhere in the mall with no idea where it
is, why it is there or what is going on. It wants out, and that is why it
explores. Everything else it does (eating, fighting, trusting, lying)
happens on the way out, or because the way out isn't turning up.

Today a human already wakes with an empty Memory, in the spawn room or
wherever a new chunk is made (World::generate puts one in each), and
explores because Want::Explore scores well. What's missing is the *why*:
nothing in a human knows it wants to leave, so nothing changes when the
leaving keeps failing. This plan gives exploring a reason and a cost.

## User's decisions (2026-10-29)

- **No way out, for now.** The mall goes on forever; step 6 is not built.
  Step 5 clues, if built, can only mislead or point at more mall.
- Humans wake with nothing and remember nothing from before (for now).
- Whether a settler defends its home depends on its traits.
- Despairing humans settling for good is fine, especially the erratic.
- Muttering alone: yes (waking and despair).
- A strong urge to get out may send a key holder to exit doors before it
  has explored everything, as hunger and fright already can.

## 1. Waking up

- New State::Waking: a just-made human lies on the floor for 3–6 s, then
  sits up (reuse Sitting/Rising) and looks round, before choose() runs at
  all. Sprite: lying frame (the dead pose, eyes open) then seated.
- Dazed for the first ~30 s (`escape_.daze`, 1 → 0): sight radius starts short
  and widens, walk is slower, it turns and glances more (like sitters do).
- Log + talk: `WOKE UP`, then a `[wake]` moment in lines.txt muttered to
  nobody ("WHERE AM I", "HELLO?", "HOW DID I GET HERE"). This is the first
  muttering-alone line; talking_plan.md left muttering out, so it needs
  your OK (open question 4).
- A human that wakes near others sees them at once, which makes the first
  greeting a real moment: new `[first_meet]` lines ("DO YOU KNOW WHERE WE
  ARE?", "ARE YOU ONE OF THEM?") used only while both are still new.

### Built (2026-10-29)

State::Waking is every human's first state (lying 3–6 s, Pose::Wake: the
dead pose turned on its back, eyes shut 1.5 s then slow blinks), then
Human::sitUp(): Sitting for 2–4 s with escape_.comingTo set, so news seen while
sat doesn't get it up early; it notes "WOKE UP, REMEMBERING NOTHING" and
says a `[wake 0.8]` line to nobody. escape_.daze 1 → 0 over DAZE_TIME 40 s:
sight radius DAZE_SIGHT 6 → 20 (Sight::look takes a radius) and steps up
to 60% slower. Hunger costs as sitting. HUD status "COMING TO" while
waking, and seated while daze ≥ 0.5. Talk conditions `dazed` and
`they.dazed` (daze ≥ 0.3) give [meet] its first-meeting lines instead of
a separate moment. Constants: WAKE_*, DAZE_* in human_tuning.h.

## 2. The urge to get out

- New `Human::escape_.out` 0..1, the urge to leave, high at waking. It replaces
  the flat curiosity reason for Want::Explore and Want::OpenDoor: their
  WANTING rows get an `escape_.out` weight, so a curious human still explores
  more, but everyone explores at first.
- Exploring now points somewhere. Each human holds a **hunch**, a
  direction it thinks "out" lies in (rolled at waking, or the way it was
  facing when it woke). Frontier search (`search()` in human_walk.cpp) adds a
  small cost for frontier cells against the hunch, so humans head off in
  a direction instead of clearing the nearest unknown corner. A hunch
  moves when something seems to point another way (step 4, step 5).
- Chunk exit doors become the thing that matters: a locked exit door seen
  is "maybe the way out". Your rule "explore everything before unlocking"
  stays as the default; `escape_.out` only adds to the existing door urge, the
  same way fright and hunger do now (doorUrge).

### Built (2026-10-29)

Human::escape_.out (outUrge()) starts at 1 and settles to OUT_REST 0.6 over
OUT_SETTLE 10 min; step 3's hope is meant to move it after that. The
Explore and OpenDoor rows of WANTING lost some base and curiosity (0.40 +
0.5 → 0.25 + 0.35, and 0.35 → 0.20 for doors) and both gain OUT_EXPLORE
0.4 × escape_.out, so at rest exploring weighs about what it did before and
OpenDoor stays 0.05 under it. Meet gains OUT_ASK 0.25 × escape_.out (asking
whoever it meets what this place is), so the extra exploring early on
doesn't crowd out greetings. Wanting out past 0.7, a key holder may be
door-urged (doorUrge, up to DOOR_URGE_OUT 0.3, the restless more), logged
once as "TRYING A DOOR. THERE HAS TO BE A WAY OUT".

Human::escape_.hunch is a compass angle rolled at birth, logged on waking ("FEELS
LIKE THE WAY OUT IS NORTH-EAST"), and turned HUNCH_FOLLOW 25% toward the
way it went at every door it passes. search()/searchFrom() take an
optional goalCost: a done cell goes back in the queue as an ending at
path cost + goal cost, so a farther cell may win. hunchCost(c, most) is
most × escape_.out × (1 − cos)/2: HUNCH_COST 300 for frontier cells,
HUNCH_DOOR_COST 1500 for doors (Explore through open doors, OpenDoor,
UrgedDoor).

Measured (headless probes, scratchpad): at the same moment, explore
targets with the hunch average cos +0.05 to it vs −0.18 without, 41%
different targets; key holders with 2+ known locked doors pick a
different door 19% of the time. Net travel after 10–25 min does NOT line
up with the birth hunch (mean cos ~0): the spawn room's single door and
"explore the chunk before unlocking" decide most of where humans go. The
hunch is a lean, not a compass; raise HUNCH_* if it should be stronger.

## 3. Hope and despair

The leaving keeps not working, and that should show.

- New `Human::escape_.hope` 0..1, starts ~0.7. Goes up on: crossing into a chunk
  never seen (+0.15), opening a door (+0.1), a hopeful tip from someone
  (step 4). Goes down on: a new chunk that is "just more of the same"
  after a few (diminishing: each one after the third lifts less and
  then lowers), a dead end (Explore failing, the existing deadEnd_), a
  locked door with no key, seeing a body (more if it was a friend),
  long stretches (~5 min) with nothing new found.
- Effects, mostly through things already there:
  - hope multiplies `escape_.out` into the Explore/OpenDoor wants;
  - low hope adds to dread_'s effects (more sitting, shorter strolls),
    makes hoarding lessons stick, and adds to the snap chance for
    mind_.erratic (cannibalism plan) — despair is a road to erratic;
  - very low hope (< 0.15): **settling**. The human stops trying to
    leave and makes a place its own: a room with water or a cake table,
    which it stays near, returns to, and keeps others away from
    (`escape_.home` cell; new Want::Home). Settlers are the mall's "locals"
    and the obvious people to trade with, rob or follow.
  - hope can come back: a new chunk, a friend, a hopeful rumour.
- HUD: `HOPE n` beside `FEAR n` on the friends row (keep it inside the
  measured strip). Log lines on crossing thresholds ("STARTING TO THINK
  THERE IS NO WAY OUT", "MADE A HOME"). Talk moments `[despair]`,
  `[hope]`, `[settle]`.

### Built (2026-10-29)

Code in src/human/human_hope.cpp (constants there and HOPE_*/DESPAIR_*
in human_tuning.h). escape_.hope starts 0.7. Rises: each new chunk set
foot in (World::chunkIdAt; the waking chunk doesn't count) +0.15 for the
first 3, then 0.05 less each, down to −0.08 (more of the same); unlocking
a door +0.1; a new friend +0.1. Falls (×0.5–1.5, the timid harder): a
body first seen −0.1 (a friend's −0.25), going starving −0.1, a dead end
at a locked door with no key −0.15, 5 min seeing no new cell −0.08.
Crossing HOPE_LOW 0.35 either way logs it and mutters `[despair]` /
`[hope]`. gloom() = how far under HOPE_LOW, 0..1: sit chance up to ×2,
Wander +0.5, strolls up to half as long, lesson fade halved, and
ERRATIC_DESPAIR 0.004/s to erratic (beats the calm fade past ~0.4
gloom). outDrive() = escape_.out × min(1.3, hope/0.7) now feeds Explore,
OpenDoor, Meet and the door urge.

Settling: hope under 0.15 (0.3 if erratic) → escape_.settled, escape_.home = nearest
known floor beside water or a table within 40 steps (else where it
stands), log + `[settle]`. Settled: no Explore, OpenDoor, CheckDoor or
Follow; Wander strolls round escape_.home (HUD "KEEPING TO HOME"). Hope back over
0.4 sets it off again. Territorial settlers (erratic, or 0.5 brave + 0.5
greedy − 0.4 sociable ≥ 0.3, about half) warn anyone they trust under
0.15 who comes within 6 of home (`[guard]`, rude, −0.1 trust) and go for
them if still inside 10 s later. HUD stats panel: HOPE under FEAR. Talk
conditions `hopeless`, `settled`.

Probe, 12 seeds × 30 min, 4 extra humans at spawn, vs the same with
lift() off: 10/150 humans settled (~13 min after waking), 15 went gloomy,
1 recovered; snaps 72 → 82 (0.42 → 0.55 per human); deaths 56% → 64%
(this setup is deadly either way: cakes are scarce); 1 intrusion, no
guard fights yet: settlers are few and far apart.

## 4. Theories about the way out

What humans believe about getting out spreads, and can be wrong.

- Memory gets `theory`: the hunch direction plus how sure (0..1) and who
  it came from. New Tell::Kind::Way: at a greeting a human may pass on
  its hunch ("I THINK IT'S THAT WAY"). The listener blends it into its
  own by trust and the teller's sureness, so groups that talk drift
  toward one shared direction — a rumour, right or not.
- A human with low hope tells the gloomy version ("THERE'S NO WAY OUT"),
  which lowers the listener's hope by trust: despair is catching, like
  fear already is.
- Cunning humans (existing lieFor rule) may send a foe the wrong way, or
  toward a known trap, as a "way out". Caught out the usual way (toldBy:
  walked the told direction and hit a trap → it was a lie).
- Settlers asked about the way out answer from experience: they have seen
  a lot of the mall, so their word carries weight, and they're gloomy.

### Built (2026-10-29)

Tell::Kind::Way (fields way, sure, gloomy, lie), made by Human::wayFor()
and told in exchange() at every greeting, beside gossip and news. Each
human has escape_.hunchSure (0.3 on waking). An honest tell gives the hunch and
its sureness; the listener is pulled toward it by w × sure / (sure + own
sure), w = trust in the teller + 0.2. Its sureness rises by up to 0.15 × w
when they agree and falls when they don't (min 0.05), and its hope rises
a little (+0.04 × w). Log: "@ THINKS THE WAY OUT IS SOUTH". Gloomy (gloom
≥ 0.3, or settled): "THERE IS NO WAY OUT", sure by chunks seen (5 = fully
sure), costing the listener up to 0.15 × w hope. Cunning humans (the
existing greedy ≥ 0.5, brave ≥ 0.4 rule) point a foe the wrong way: at the
nearest spike plate the foe doesn't know, if any (toldBy[plate], so
stepping on it is caught as a lie by learnTrap: −0.6 trust), else
opposite their own hunch. lieWorthy() now also sends a cunning human to
greet a foe it has a plate to lie about, even if the foe has no keys.
Not retold to the same human until its hunch turns by 45° or it turns
gloomy (escape_.wayTold). Moments `[tell_way]`, `[no_way_out]`, `[lie_way]`.

Probe, 20 seeds × 30 min, 6 extra humans at spawn: living pairs that told
each other the way end with hunches at mean cos 0.37, other pairs 0.00
(12 seeds: 0.43 vs 0.03). 672 way tells. No gloomy tells or lies came up
in those runs: the gloomy seldom meet anyone new, and lies need a
cunning human with a foe and a known spike plate the foe doesn't know.
Checked directly instead (scratchpad waytest): a gloomy tell lowers the
listener's hope, a lie points at the plate and is caught when they step
on it, an honest tell bends the listener's hunch.

### Asked, not volunteered (2026-10-29)

User: "THAT WAY. I'M SURE OF IT" said unprompted at every greeting made no
sense in play. Now nobody volunteers it. At a greeting, a human that still
wants out (outDrive ≥ ASK_WAY_FROM 0.4), and not to a foe, asks once per
human (`[ask_way]`, escape_.askedWay); only one of the pair asks. When the
question is said, the other answers (Human::answerWay): its hunch
(`[tell_way]`), that there is none (`[no_way_out]`), a lie to a foe
(`[lie_way]`), or `[no_idea]` when its sureness is under WAY_UNSURE 0.15
or it won't say. Then it may ask back. Lines name the direction with
{WAY} (compass word, filled in Human::say), and the `sure` condition
(hunchSure ≥ 0.5) picks confident lines; otherwise they hedge ("NORTH-EAST,
I THINK. DON'T QUOTE ME."). A question back can go unanswered if the
greeting ends and the other walks off.

### Pointing, and rarer (2026-10-29)

User: no compass directions; point and say the way; far fewer way
suggestions. Compass words are gone from speech and the log ("@ POINTED
THE WAY OUT"; no hunch note on waking; no {WAY} slot). Telling the way,
the speaker faces it (left/right) and points with its front arm, no
arrow (user: "just pointing finger"): level, raised for a way mostly
north, lowered for one mostly south (Pose::Point/PointUp/PointDown,
HumanLook::point(tilt), POINT_TILT in world_view.cpp, Speech::points). Lines say "THAT WAY". A first greeting that could ask
asks only ASK_WAY_CHANCE 15% of the time (once per human either way),
and asking back after answering is ASK_BACK_CHANCE 30%.

## 5. Clues in the world (needs your answer to Q1)

Exploring needs something to find besides more mall.

- **Exit signs**: small wall/floor feature (new world item, drawn like a
  green sign with an arrow) placed in some chunks, pointing at a chunk
  exit door. Seen signs pull the hunch toward their arrow. Whether the
  signs are true depends on Q1.
- **Daylight**: a door or skylight cell that glows like outside. Seen
  from afar, it's a strong pull and a big hope boost; reaching it shows
  what it really is (Q1).
- **Other people's marks** (later, ties into blood): humans scratch an
  arrow on the floor where they turned back, so others know a way is a
  dead end — or lie with it.

## 6. Getting out (needs your answer to Q1)

Depends on what you want. Options below in Q1; the plan builds 1–5 either
way, and this step only if there is a real exit.

## Order

1. Waking (state, daze, wake log/lines). Small, visible at once.
2. `escape_.out` + hunch-directed exploring. Check with the stuck soak.
3. Hope/despair + HUD + talk; settling last within this step.
4. Theories as Tell::Kind::Way, gloom spreading, lies.
5. Exit signs / daylight, per Q1.
6. Exit, per Q1.

After each: `make stuck` 30 seeds × 20 min (no new loops/flips; hunch
cost must not make frontier choice flip), and a headless A/B on chunks
reached per human, hope curves, settlers per run.

## Open questions

1. **Is there a real way out?**
   a. No. The mall goes on forever; the premise is the trap. Hope can
      only be fed by false clues, rumours and each other.
   b. Yes, far away: a rare Exit chunk only made past some distance from
      spawn. Who reaches it leaves the world for good ("GOT OUT" in the
      logs of those who saw), and its story spreads as a rumour.
   c. False exits only: daylight doors and exit signs that lead to more
      mall — reaching one is a big hope crash.
   d. Mix of b and c.
2. Should humans have anything from before waking (a fragment: a name
   they half-remember, an object they woke holding), or truly nothing?
3. Settling: OK that the most despairing stop exploring for good unless
   something lifts their hope? Should settlers defend their home (fight
   intruders) or only stay near it?
4. Muttering alone: OK to add it for waking (and despair) only?
5. Your rule "explore everything before unlocking doors" — keep it as is,
   or should a strong urge to get out let a key holder try exit doors
   sooner (as hunger and fright already can)?
