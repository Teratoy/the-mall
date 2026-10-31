# Lines editor

A local web page for rewriting what humans say (`data/lines.txt`)
comfortably: `make lines`, then open http://localhost:8765.

## User's decisions (2026-10-31)

- A local web page, served by a small Python script in `tools/`. It reads
  and writes `data/lines.txt` itself, so F5 in the game picks the edits up.
- What it does: rewrite the words; edit conditions and tones with pickers;
  add and delete lines; mark lines reviewed; stay up to date with lines
  that other sessions add to the game.
- Each line shows as it would in the game: a pixel bubble in the game's own
  font, wrapped at the game's width, edged in its tone's colour.
- Claude drafts rewrites of batches of moments on request. The drafts sit
  beside the lines in the tool, to accept, change or reject one by one.

## How it works

- `tools/lines_editor.py`: stdlib Python, no installs, runs on the host.
  Moment, condition and tone names are read from `src/human/talk.cpp`
  every time, so new ones show up without touching the tool.
- Edits are sent as small operations (this row, which read *so*, now reads
  *so*). The server re-reads the file for every one and changes only that
  row, keeping the column layout, so it never overwrites what another
  session added meanwhile. If the row has changed underneath, it refuses
  and the page reloads.
- The page checks the file every two seconds and reloads when it changes.
- `data/lines_review.json`: which lines are reviewed, keyed by moment,
  conditions and words. A line someone else changes or adds counts as not
  reviewed, so new lines from new features come up in "unreviewed".
  Moments the code has but the file lacks are listed too.
- `data/lines_drafts.json`: Claude's drafts. Each names the moment and the
  row it would replace (or none, to add a line) and the new row, with an
  optional note.
- `tools/font_dump.cpp` (`make fontdump`): saves raylib's default font,
  which the game draws all its text in, to `tools/lines_editor/font.png`
  and `font.json`, so the page measures and draws exactly as the game does.

### Built (2026-10-31)

All of the above.

### Conditions in plain words (built 2026-10-31)

The user asked for help knowing exactly what each condition means.
`tools/lines_editor/conditions.txt` says it for every condition and tone,
written from what `talk.cpp` checks, in groups. The editor shows it on
hover over a chip, read out under the selected line (or under every line
with "meanings" ticked), beside each suggestion while typing conditions,
and in a searchable list (`c`, or "All conditions…" in the editor, where
a click adds one). A condition the game gains without a description is
flagged in the sidebar, so the file is kept up with new features.
