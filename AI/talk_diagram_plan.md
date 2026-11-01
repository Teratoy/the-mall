# Talk diagram

A DIAGRAM tab in the lines editor (`make lines`, http://localhost:8765)
that shows every talk moment: what makes a human say it, who hears it,
what it does to them, and which moments can follow it. The aim is to see
how the talk fits together while writing lines, and to spot moments that
lead nowhere or are never reached.

## User's decisions (2026-11-01)

- Both a map of conversations (which moment answers which) and, for each
  moment, its triggers and consequences.
- A tab in the lines editor, not a picture or a separate page. Clicking a
  moment goes to its lines.
- Kept up to date from the code: the moments, their groups and where
  they are said are read from the source each time; only the links
  between moments and the words on triggers and consequences are kept by
  hand, in a small file a new feature adds rows to.
- Each moment shows: its triggers, its consequences, where it can branch
  to, its line count and how many are reviewed, the tones its lines use,
  whether the soak or scenarios ever reach it, and its say-chance.
- Branch chances both ways: a word written in the flow file (always,
  often, seldom), and beside it the number counted from the soak (step 5).
- The flow file is edited from the tab too (step 4).
- A moment with no section in the flow file is only flagged, as not
  described yet, in the tab and in `make coverage`; `make check` does not
  fail on it, so a new feature is never held up by the diagram.
- The tab opens on the overview; each moment there has a button that
  opens its focus view.

## What each moment shows

| What | Where it comes from |
|---|---|
| Name and group | `talk.h`: the `enum class Moment` and the comment above each group of names. `talk.cpp` gives the section names |
| Who it is said to | The flow file (someone, nobody, a group, through a wall). Drafted from the group comments in `talk.h`, which already say it |
| Triggers | Read: every `sayLater(talk::Moment::X, ...)` call, with its file and line, the function it is in and that function's `///` doc ("Thirsty, it walks to the nearest water it knows"). Written: a short line in the flow file saying it plainly, and the conditions that gate it |
| Consequences | Read: the tones its lines use, and what each tone does (from the head of `lines.txt`). Written: what saying it changes beyond tone (trust, memory, a cake handed over, a fight, a lesson learnt) |
| Branches | The flow file: `beg -> give`, `beg -> refuse`, each with when it happens and how likely it is (always, often, seldom). Some are read from the code too: a `sayLater` made right after another moment is said, such as `Snubbed` after `Refuse` in `human_speech.cpp` |
| Lines and reviewed | `data/lines.txt` and `data/lines_review.json`, which the editor already reads |
| Tones used | Counted from its lines: a coloured mark per tone, in the bubble edge colours |
| Say-chance | The number after the section name, `[hit 0.3]` |
| Reached in tests | `make coverage` writes `build/coverage.json` beside its text output; the tab reads it and shows when it was made. No file: shown as unknown |

## The flow file

`data/talk_flow.txt`, in the style of `lines.txt` (sections, `#`
comments, a header explaining the format):

```
[beg] someone
when   | hungry, it asks someone it thinks has cakes
does   | the one asked may share, or think less of it
-> give      often  | they share a cake
-> refuse    often  | they turn it away
-> threat    seldom | greedy and strong, it asks again with a fist
```

- `[moment] who`: who it is said to.
- `when |`: the trigger in plain words (one or more rows).
- `does |`: consequences beyond the tone.
- `-> moment chance | when`: a branch, by the other side or the speaker.

The editor lists moments that have no section, branches to moments that
don't exist, and moments that no branch leads to and that are not said
on their own (a trigger in the code but no `when`). Claude drafts the
first version of the whole file from the code; you review it in the tab
like the line drafts.

## The tab

- **Overview**: the groups from `talk.h` as boxes (greetings, cakes and
  fights, death and blood, chit-chat, thirst...), each moment a small
  card with its line count, reviewed bar, tone marks, say-chance and a
  dot for reached / not reached. Arrows between cards for the branches.
- **Focus**: click a moment and it moves to the middle, what leads to it
  on the left, what it can lead to on the right, two steps out each
  way. A side panel has its triggers (with the code places, clickable to
  copy `file:line`), consequences, and a sample bubble of its lines.
  OPEN LINES jumps to the moment in the editor.
- **Filters**: show only unreached, unreviewed, with no lines, or a tone.
- Laid out in plain JavaScript and SVG, no libraries, like the rest of
  the editor.

## Steps

1. `lines_editor.py` reads the moments, groups, `sayLater` call sites and
   their function docs, and `build/coverage.json`; `coverage.py` writes
   that JSON. Served at `/diagram.json`.
2. `data/talk_flow.txt`: the format, the parser in the editor, and the
   checks above. Claude drafts it for every moment.
3. The DIAGRAM tab: overview and focus, the side panel, the filters.
4. Editing the flow file from the tab (add a branch, change a trigger),
   with the same small-operation saving the lines use.
5. Counted chances: a tool runs the soak's worlds and counts, for each
   moment one human says to another, which moment follows between the
   two within a few seconds. Written to `build/talk_flow_counts.json`;
   the tab shows the share beside each branch's word, and lists pairs
   the soak shows often that the flow file has no branch for.

## Open questions

None.
