# Lies

Cunning humans (greedy ≥ 0.5, brave ≥ 0.4) can lie to a foe. They can send
it on a route over spikes to a door, or to water over spikes (and, when
asked, point the wrong way out). The code has been there since
2026-10-29, but no soak run has ever logged a lie. This plan says why, and
proposes how to make lies part of the game.

## User's decisions (2026-10-30)

- Build step 1 (liars reach their marks, and don't show hostility in
  their words), 2c (bait with any trap that re-arms), 3 (lies about
  people) and 4 (tests).
- Not now: 2a false tips, 2b the wrong way out unasked.
- Traps that re-arm: spikes and dart plates. Slam rooms are spent once
  they shut, so they are never bait.
- Questions 1, 3, 4 and 5 weren't answered, so they stay as they are:
  lies go to foes who don't hate the liar back, they gain the liar
  nothing but the foe's harm, and a liar found out is only thought worse
  of by its mark.

## Why lies never happen (measured 2026-10-30)

Measured over the soak's 6 worlds × 15 min, crowd 6: 56 humans, 24 of them
cunning (43%).

| Condition | Time it held |
|---|---|
| A cunning human with a foe in sight | 950 s |
| ...where the foe doesn't hate it back, so would greet it | 400 s |
| ...and the liar knows of spikes the foe doesn't | **7 s** (1 pair) |
| ...and a route lie or water lie could be built | **0 s** |
| A cunning human knowing any spike plate at all | 229 of 10,411 s (2%) |

Four things stand in the way. The first is by far the biggest:

1. **Nothing to lie with.** Every lie needs a spike plate the liar knows
   and its mark doesn't. Spikes are only in non-spawn chunks, are hidden,
   and are seldom learnt, so a liar almost never has one.
2. **The liar keeps away from its mark.** It counts the mark a foe, and
   `keepAway` (a reflex, before any wish to greet) walks it off from foes
   within SUSPECT_DIST (5 cells).
3. **The liar turns its mark away.** If the mark greets first,
   `welcomes()` refuses any foe, and the liar snubs the very human it
   means to lie to.
4. **Narrow windows.** A route lie needs the mark to hold a key, and a
   locked door the mark has never seen. A water lie needs the mark thirsty
   but not on its way to water: a human heading for water can't be
   greeted, and every spawn room has a basin.

The trace also showed two smaller things:
- A cunning human wants to meet less than it wants to explore: Meet goes
  up with sociability, and the cunning are greedy and brave, not sociable.
- The liar greets its mark with foe lines: "YOU AGAIN." at a first
  meeting gives the game away.

## Tried in a scratch worktree (not merged)

Two one-line changes, obstacles 2 and 3, are enough for a set-up lie to
land:

```cpp
// shuns(): don't keep away from a foe worth lying to
(suspicious(o.id) || (isFoe(o.id) && !lieWorthy(o)))
// welcomes(): greet back a foe worth lying to
(!isFoe(o.id) || lieWorthy(o)) && ...
```

With them, `lie-route` in tests/scenarios.cpp passes. The liar greets,
gives its name, says "GO ON, IT'S SAFE.", and the mark thanks it and asks
to come along. Each change alone isn't enough; together they are. A boost
to Meet toward a foe wasn't needed.

The soak output stayed **byte-identical** with both changes. So in normal
play they change nothing, because obstacle 1 never lets a lie come up.
They are safe, but only worth having together with step 2 below.

## Proposed steps

1. **Let liars reach their marks.** The two changes above, and warm lines
   (not foe lines) when a cunning human greets a mark. Then the
   `lie-route` scenario's known note is dropped.
2. **Lies that need no trap.** Pick any (see Q2):
   a. **False tips:** "there's a cake (or key) down there", sending a foe
      to a far-off empty spot it can't check from where it stands. This
      uses the existing tell and checkTold machinery: when the mark finds
      nothing there, it thinks worse of the liar, as it already does for
      a wrong tip.
   b. **The wrong way out, unasked:** the cunning already point a foe the
      wrong way when asked. Let them offer it at a greeting.
   c. **Any known trap as bait:** darts and slam rooms as well as spikes,
      e.g. "there's a cake on that table" for a rigged one.
3. **Lies about people.** Tell others that someone is dangerous or
   trustworthy against what you really think. This is the talking plan's
   "lies about people" item. It uses gossip, so it only reaches those who
   already trust the liar.
4. **Tests.** Fix `lie-water`: its mark must not know of water, so it
   can't simply walk to the spawn basin. Add a scenario for each new kind
   of lie. Keep the census of lie opportunities as a tool in `tools/`,
   to check how often lies happen after tuning.

## Built (2026-10-30: steps 1, 2c, 3 and 4)

- **Reaching the mark** (human_blood.cpp, human_social.cpp): `shuns()`
  no longer keeps a liar away from a foe it means to lie to, and
  `welcomes()` greets such a foe back.
- **A straight face** (talk.cpp): `Human::masks(id)`. A cunning human's
  `foe` and `disliked` line conditions are false toward a foe, unless it
  is fighting them or afraid of them, so it doesn't greet a mark with
  "YOU AGAIN.". `Human::cunning()` replaces the copies of the test.
- **Bait that re-arms** (`Human::bait`): spikes, or a dart plate in the
  floor. Route lies, water lies and the wrong way out (when asked) can
  all go over either; their log lines say which ("TOLD @ A WAY OVER
  DARTS"). Slam rooms never count.
- **Rigged cakes** (`Human::riggedCakeFor`, in `lieFor`): to a hungry foe
  with no cake, the cunning tip a cake they know sits on a table rigged
  with darts. It sounds like an honest tip (the tell_cake lines), and the
  liar logs "TOLD @ OF A RIGGED CAKE". Every cake tip is now kept in
  `Memory::cakeTips`, so whoever is darted taking a cake it was told of
  blames the teller ("@ SENT ME TO A RIGGED CAKE", TRUST_LIED), lie or
  not. Like everyone, it only knows what it saw.
- **Being found out on a route**: every cell of a told route now counts
  as told clear, seen before or not. Before, a plate on ground the mark
  had already looked at was never laid at the liar's door.
- **Slander** (`gossipFor`): a cunning gossip speaks of anyone it is
  wary of (trust below 0) as if they were dangerous (SLANDER -0.8, in
  human_tuning.h). When that is at least SLANDER_LIE (0.3) worse than it
  thinks, it is a lie: the liar logs "LIED TO @ ABOUT #", and the
  listener hears an ordinary warning. It only reaches those who trust the
  liar enough to gossip with it.
- **Tests** (tests/scenarios.cpp): `lie-water` (its mark now knows of no
  water), `lie-route`, `lie-darts` (a real plate: the mark walks into it
  and blames the liar), `lie-cake` (a real rigged cake: the mark is
  darted and blames the liar) and `slander`, all passing. `lie-water` and
  `lie-route` also check that no foe line is said.

**Still rare in normal play.** The soak's output didn't change at all:
in its 6 worlds no liar ever knows a trap its foe doesn't, so no lie
comes up. Lies now work whenever the chance arises, but the chance is
still rare. Lies that need no trap (2a false tips, 2b the wrong way out
unasked) are what would make them common.

## Open questions

1. **Who gets lied to?** Only foes (trust ≤ -0.4) whose hatred isn't
   returned, as now? Or should the greedy also lie to strangers who
   compete with them for cakes?
2. **Which new lies?** 2a false tips, 2b the wrong way out unasked, 2c any
   trap as bait, 3 lies about people: all, some, or none for now?
3. **What a lie gains the liar.** Today it only harms the foe. Should a
   lie also serve greed, e.g. sending a rival away from cakes the liar
   wants?
4. **Being found out.** Today a mark that finds a trap on a told route
   thinks much worse of the teller (-0.6), and nobody else hears of it.
   Should a found-out liar's name spread through gossip?
5. **How often.** Is it fine for lies to stay rare (a notable event), or
   should they be a regular part of the cunning's play?
