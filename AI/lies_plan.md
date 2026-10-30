# Lies

Cunning humans (greedy ≥ 0.5, brave ≥ 0.4) can lie to a foe. They can send
it on a route over spikes to a door, or to water over spikes (and, when
asked, point the wrong way out). The code has been there since
2026-10-29, but no soak run has ever logged a lie. This plan says why, and
proposes how to make lies part of the game. **Nothing here is built yet:
it waits on the user's answers to the questions at the end.**

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
